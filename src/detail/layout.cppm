module vectext:detail.layout;
import :detail.state;
import :detail.collection;
import :detail.queries;
import :layout;
import :font;
import :types;
import vectext.ffi.fontconfig;
import vectext.ffi.freetype;
using namespace rstd::prelude;
namespace vectext::detail {
struct Character {
    Index code{}, begin{}, end{}, font{}, glyph{};
    Scalar advance{};
    Rect ink;
    bool newline{}, tab{}, missing{};
};
struct BuiltLayout {
    TextLayoutData data;
    Vec<Arc<Font>> fonts;
};
inline void unite(Rect &target, Rect other) {
    if (other.width == 0 || other.height == 0)
        return;
    if (target.width == 0 || target.height == 0) {
        target = other;
        return;
    }
    auto left = target.x < other.x ? target.x : other.x;
    auto top = target.y < other.y ? target.y : other.y;
    auto right = target.x + target.width > other.x + other.width ? target.x + target.width
                                                                 : other.x + other.width;
    auto bottom = target.y + target.height > other.y + other.height ? target.y + target.height
                                                                    : other.y + other.height;
    target = {left, top, right - left, bottom - top};
}
auto validate_text(slice<u8> bytes) -> Result<empty, TextError> {
    if (rstd::str_::from_utf8(bytes).is_err())
        return Err(TextError::InvalidUtf8);
    for (auto byte : bytes)
        if (byte == u8())
            return Err(TextError::InvalidUtf8);
    return Ok(empty{});
}
auto validate(TextRequest request, TextLimits limits) -> Result<empty, TextError> {
    if (request.text->len() > usize(limits.text_bytes) || request.family->len() > usize(4096) ||
        request.language->len() > usize(128))
        return Err(TextError::Limit);
    TextFormat format{request.family,    request.language,  request.size,
                      request.direction, request.wrap,      request.bold,
                      request.italic,    request.alignment, request.line_spacing};
    if (request.size < 1.0 / 64 || !format.valid() ||
        (request.width.is_some() && (!finite(*request.width) || *request.width < 0)))
        return Err(TextError::InvalidValue);
    if (request.size > 16384 || request.line_spacing > 1048576 ||
        (request.width.is_some() && *request.width > 1048576))
        return Err(TextError::Limit);
    auto text_valid = validate_text(request.text->as_bytes());
    if (text_valid.is_err())
        return text_valid;
    const ref<str> names[]{request.family, request.language};
    for (auto value : names) {
        if (rstd::str_::from_utf8(value->as_bytes()).is_err())
            return Err(TextError::InvalidUtf8);
        for (auto byte : value->as_bytes())
            if (byte == u8())
                return Err(TextError::InvalidValue);
    }
    if (request.direction == TextDirection::RightToLeft)
        return Err(TextError::Unsupported);
    return Ok(empty{});
}
auto build_layout(Collection &collection, TextRequest request) -> Result<BuiltLayout, TextError> {
    auto valid = validate(request, collection.limits);
    if (valid.is_err())
        return Err(valid.unwrap_err());
    auto matched = candidates(collection, request);
    if (matched.is_err())
        return Err(matched.unwrap_err());
    auto choices = rstd::move(matched).unwrap();
    auto primary = candidate_font(collection, choices[usize()], request.size);
    if (primary.is_err())
        return Err(primary.unwrap_err());
    auto queries = Box<Queries>::make();
    auto *query = queries.as_mut_ptr().as_raw_ptr();
    BuiltLayout result{
        TextLayoutData{.text = String::make(request.text),
                       .queries = Box<TextQueries>::from_raw(
                           rstd::move(queries).into_raw().template cast<TextQueries>())},
        {}};
    result.data.revision = collection.counters.revision;
    result.fonts.push(rstd::move(primary).unwrap());
    Scalar tab_width{};
    {
        auto &font = result.fonts[usize()];
        auto guard = font->mutex.lock().unwrap();
        auto size = font->set_size(1);
        if (size.is_err())
            return Err(size.unwrap_err());
        if (ft::FT_Load_Glyph(font->face.get(), ft::FT_Get_Char_Index(font->face.get(), ' '),
                              ft::FT_LOAD_NO_HINTING | ft::FT_LOAD_NO_BITMAP))
            return Err(TextError::Font);
        tab_width = font->face->glyph->advance.x / 64.0 * 8;
        if (tab_width <= 0)
            tab_width = request.size * 4;
    }
    Vec<Character> chars;
    Index glyph_count{};
    auto iterator = request.text->char_indices();
    while (auto item = iterator.next()) {
        auto [offset, value] = *item;
        Index cp = value.to_primitive(), begin = static_cast<Index>(offset.to_primitive());
        Index end = begin + (cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4);
        if (cp == '\n' && !chars.is_empty() && chars[chars.len() - usize(1)].code == '\r') {
            chars[chars.len() - usize(1)].end = end;
            continue;
        }
        Character character{.code = cp,
                            .begin = begin,
                            .end = end,
                            .newline = cp == '\n' || cp == '\r',
                            .tab = cp == '\t'};
        if (!character.newline && ++glyph_count > collection.limits.glyphs)
            return Err(TextError::Limit);
        if (!character.newline && !character.tab) {
            Option<Arc<Font>> selected;
            Index glyph{};
            for (auto &candidate : choices) {
                fc::FcCharSet *charset{};
                if (fc::FcPatternGetCharSet(candidate.pattern.get(), fc::FC_CHARSET, 0, &charset) ==
                        fc::FcResultMatch &&
                    !fc::FcCharSetHasChar(charset, cp))
                    continue;
                auto loaded = candidate_font(collection, candidate, request.size);
                if (loaded.is_err())
                    return Err(loaded.unwrap_err());
                auto font = rstd::move(loaded).unwrap();
                {
                    auto guard = font->mutex.lock().unwrap();
                    glyph = ft::FT_Get_Char_Index(font->face.get(), cp);
                }
                if (glyph) {
                    selected = Some(rstd::move(font));
                    break;
                }
            }
            if (selected.is_none()) {
                selected = Some(result.fonts[usize()].clone());
                character.missing = true;
            }
            auto &font = *selected;
            usize slot{};
            for (; slot < result.fonts.len(); ++slot)
                if (result.fonts[slot]->id == font->id)
                    break;
            if (slot == result.fonts.len())
                result.fonts.push(font.clone());
            character.font = static_cast<Index>(slot.to_primitive());
            character.glyph = glyph;
            auto guard = font->mutex.lock().unwrap();
            auto size = font->set_size(1);
            if (size.is_err())
                return Err(size.unwrap_err());
            if (ft::FT_Load_Glyph(font->face.get(), glyph,
                                  ft::FT_LOAD_NO_HINTING | ft::FT_LOAD_NO_BITMAP))
                return Err(TextError::Font);
            auto &m = font->face->glyph->metrics;
            character.advance = font->face->glyph->advance.x / 64.0;
            character.ink = {m.horiBearingX / 64.0, -m.horiBearingY / 64.0, m.width / 64.0,
                             m.height / 64.0};
            if (!finite(character.advance) || character.advance < 0 || !character.ink.valid())
                return Err(TextError::Font);
        }
        chars.push(rstd::move(character));
    }
    auto advance = [tab_width](const Character &c, Scalar x) {
        return c.tab ? (__builtin_floor(x / tab_width) + 1) * tab_width - x : c.advance;
    };
    usize start{};
    Index byte_start{};
    Scalar y{};
    bool finished{};
    while (!finished) {
        usize end = start, last_break = start;
        Scalar width{};
        while (end < chars.len() && !chars[end].newline) {
            auto next = width + advance(chars[end], width);
            if (request.width.is_some() && request.wrap != TextWrap::None &&
                next > *request.width && end > start) {
                if (last_break > start) {
                    end = last_break;
                    break;
                }
                if (request.wrap == TextWrap::WordOrCharacter)
                    break;
            }
            width = next;
            ++end;
            if (chars[end - usize(1)].code == ' ' || chars[end - usize(1)].code == '-') {
                last_break = end;
                if (request.width.is_some() && request.wrap != TextWrap::None &&
                    width > *request.width)
                    break;
            }
        }
        width = 0;
        auto &first = result.fonts[usize()];
        Scalar ascent = first->ascent, descent = first->descent, height = first->height;
        for (auto i = start; i < end; ++i) {
            width += advance(chars[i], width);
            auto &font = result.fonts[usize(chars[i].font)];
            if (font->ascent > ascent)
                ascent = font->ascent;
            if (font->descent > descent)
                descent = font->descent;
            if (font->height > height)
                height = font->height;
        }
        if (height < ascent + descent)
            height = ascent + descent;
        Scalar x{};
        if (request.width.is_some()) {
            auto extra = *request.width > width ? *request.width - width : 0;
            if (request.alignment == TextAlignment::Right ||
                request.alignment == TextAlignment::End)
                x = extra;
            else if (request.alignment == TextAlignment::Center)
                x = extra / 2;
        }
        TextLine line{.byte_start = byte_start,
                      .byte_end = end > start ? chars[end - usize(1)].end : byte_start,
                      .glyph_start = static_cast<Index>(result.data.glyphs.len().to_primitive()),
                      .logical = {x, y, width, height},
                      .baseline = y + ascent};
        auto stop_start = static_cast<Index>(query->stops.len().to_primitive());
        query->stops.push(Stop{byte_start, x});
        Scalar position{};
        auto line_index = static_cast<Index>(result.data.lines.len().to_primitive());
        for (auto i = start; i < end; ++i) {
            auto &c = chars[i];
            auto delta = advance(c, position);
            result.data.glyphs.push(TextGlyph{c.font,
                                              c.glyph,
                                              c.begin,
                                              c.end,
                                              line_index,
                                              {x + position, line.baseline},
                                              delta,
                                              !c.tab && c.code != ' ',
                                              c.missing});
            if (c.missing)
                ++result.data.missing_glyphs;
            auto ink = c.ink;
            ink.x += x + position;
            ink.y += line.baseline;
            unite(line.ink, ink);
            position += delta;
            query->stops.push(Stop{c.end, x + position});
            auto glyph_end = static_cast<Index>(result.data.glyphs.len().to_primitive());
            if (!result.data.runs.is_empty() &&
                result.data.runs[result.data.runs.len() - usize(1)].line == line_index &&
                result.data.runs[result.data.runs.len() - usize(1)].font == c.font) {
                auto &run = result.data.runs[result.data.runs.len() - usize(1)];
                run.byte_end = c.end;
                run.glyph_end = glyph_end;
            } else
                result.data.runs.push(
                    TextRun{c.font, c.begin, c.end, glyph_end - 1, glyph_end, line_index});
        }
        line.glyph_end = static_cast<Index>(result.data.glyphs.len().to_primitive());
        if (end < chars.len() && chars[end].newline && chars[end].end - chars[end].begin == 2)
            query->stops.push(Stop{chars[end].begin + 1, x + width});
        query->lines.push(QueryLine{line.logical, stop_start,
                                    static_cast<Index>(query->stops.len().to_primitive())});
        unite(result.data.ink, line.ink);
        if (result.data.lines.is_empty()) {
            result.data.logical = line.logical;
            result.data.baseline = line.baseline;
        } else {
            auto left = result.data.logical.x < x ? result.data.logical.x : x;
            auto right = result.data.logical.x + result.data.logical.width > x + width
                             ? result.data.logical.x + result.data.logical.width
                             : x + width;
            result.data.logical = {left, 0, right - left, y + height};
        }
        result.data.lines.push(rstd::move(line));
        y += height + request.line_spacing;
        if (!finite(y) || y > 1048576 || width > 1048576)
            return Err(TextError::Limit);
        if (end == chars.len())
            finished = true;
        else if (chars[end].newline) {
            byte_start = chars[end].end;
            start = end + usize(1);
        } else {
            byte_start = chars[end].begin;
            start = end;
        }
    }
    if (request.width.is_some() && request.alignment != TextAlignment::Natural) {
        auto right = result.data.logical.x + result.data.logical.width;
        result.data.logical.x = 0;
        result.data.logical.width = right > *request.width ? right : *request.width;
    }
    ++collection.counters.layouts;
    return Ok(rstd::move(result));
}
} // namespace vectext::detail
