module vectext:layout.build;
import :font.state;
import :font.collection;
import :layout.queries;
import :layout;
import :font;
import :types;

using namespace rstd::prelude;
namespace vectext {
struct Character {
    Index code{}, begin{}, end{}, font{}, glyph{};
    Index object{~Index{}};
    Scalar advance{};
    Scalar ascent{}, descent{};
    Rect ink;
    bool newline{}, tab{}, missing{}, synthetic{};
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
    if (request.overflow < TextOverflow::Clip || request.overflow > TextOverflow::ThreeDots ||
        request.tabs < TextTabs::Stops || request.tabs > TextTabs::Ignore ||
        request.whitespace < TextWhitespace::Preserve ||
        request.whitespace > TextWhitespace::TrimWrap ||
        request.invalid_text < InvalidText::Reject || request.invalid_text > InvalidText::Replace)
        return Err(TextError::InvalidValue);
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
    if (!request.inline_objects.matches(request.text) || !request.font_spans.matches(request.text)) return Err(TextError::InvalidValue);
    return Ok(empty{});
}
auto build_layout(Collection &collection, TextRequest request) -> Result<BuiltLayout, TextError> {
    auto input = request.bytes.is_some() ? *request.bytes : request.text->as_bytes();
    if (input.len() > usize(collection.limits.text_bytes))
        return Err(TextError::Limit);
    Vec<Character> chars;
    String normalized;
    for (usize offset{}; offset < input.len();) {
        auto b = input[offset].to_primitive();
        Index cp = b;
        usize count(1);
        if (b >= 0xc2 && b <= 0xdf) {
            cp = b & 31;
            count = usize(2);
        } else if (b >= 0xe0 && b <= 0xef) {
            cp = b & 15;
            count = usize(3);
        } else if (b >= 0xf0 && b <= 0xf4) {
            cp = b & 7;
            count = usize(4);
        }
        bool valid = b > 0 && (b < 0x80 || count > usize(1));
        auto consumed = count;
        if (offset + count > input.len()) {
            valid = false;
            consumed = input.len() - offset;
        } else
            for (usize i(1); i < count; ++i) {
                auto next = input[offset + i].to_primitive();
                if ((next & 0xc0) != 0x80) {
                    valid = false;
                    consumed = usize(1);
                    break;
                }
                cp = (cp << 6) | (next & 63);
            }
        if ((count == usize(2) && cp < 0x80) || (count == usize(3) && cp < 0x800) ||
            (count == usize(4) && cp < 0x10000) || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
            valid = false;
        if (!valid) {
            if (request.invalid_text == InvalidText::Reject)
                return Err(TextError::InvalidUtf8);
            cp = 0xfffd;
        }
        auto begin = static_cast<Index>(normalized.len().to_primitive());
        normalized.push(static_cast<char32_t>(cp));
        auto end = static_cast<Index>(normalized.len().to_primitive());
        if (cp == '\n' && !chars.is_empty() && chars[chars.len() - usize(1)].code == '\r')
            chars[chars.len() - usize(1)].end = end;
        else
            chars.push(Character{.code = cp,
                                 .begin = begin,
                                 .end = end,
                                 .newline = cp == '\r' || cp == '\n',
                                 .tab = cp == '\t'});
        offset += consumed;
    }
    request.text = normalized.as_str();
    auto valid = validate(request, collection.limits);
    if (valid.is_err())
        return Err(valid.unwrap_err());
    if (request.boundaries && (!request.boundaries->present() || request.boundaries->text() != request.text))
        return Err(TextError::InvalidValue);
    auto matched = resolver(collection, request);
    if (matched.is_err())
        return Err(matched.unwrap_err());
    auto fonts = rstd::move(matched).unwrap();
    auto queries = Box<Queries>::make();
    auto *query = queries.as_mut_ptr().as_raw_ptr();
    BuiltLayout result{
        TextLayoutData{.text = String::make(request.text),
                       .queries = Box<TextQueries>::from_raw(
                           rstd::move(queries).into_raw().template cast<TextQueries>())},
        {}};
    if (request.boundaries) result.data.boundaries = *request.boundaries;
    result.data.inline_objects = request.inline_objects;
    result.data.font_spans = request.font_spans;
    result.data.revision = collection.counters.revision;
    result.fonts.push(fonts.primary.clone());
    Option<FontResolver> variants[4];
    auto resolver_at=[&](const Character &character) -> Result<FontResolver *,TextError> {
        auto style=character.synthetic ? TextFontStyle{request.bold,request.italic} : request.font_spans.style(character.begin,{request.bold,request.italic});
        if (style == TextFontStyle{request.bold,request.italic}) return Ok(&fonts);
        auto slot=unsigned(style.bold)*2+unsigned(style.italic);
        if (variants[slot].is_none()) {
            auto variant=request; variant.bold=style.bold; variant.italic=style.italic;
            auto matched=resolver(collection,variant); if (matched.is_err()) return Err(matched.unwrap_err());
            variants[slot]=Some(rstd::move(matched).unwrap());
        }
        return Ok(&*variants[slot]);
    };
    Scalar tab_width{};
    {
        auto &font = result.fonts[usize()];
        auto metrics = font->measure(font->glyph(' '));
        if (metrics.is_err())
            return Err(metrics.unwrap_err());
        tab_width = metrics->advance * 8;
        if (tab_width <= 0)
            tab_width = request.size * 4;
    }
    Index glyph_count{};
    bool want_dots = request.max_lines && request.overflow == TextOverflow::ThreeDots;
    if (want_dots)
        chars.push(Character{.code = '.', .synthetic = true});
    auto objects = request.inline_objects.objects(); usize next_object{};
    for (auto &character : chars) {
        auto cp = character.code;
        if (!character.newline && ++glyph_count > collection.limits.glyphs)
            return Err(TextError::Limit);
        if (!character.synthetic && next_object < objects.len() && character.begin == objects[next_object].range.start) {
            auto scaled = objects[next_object].scaled(request.size);
            if (scaled.is_err()) return Err(scaled.unwrap_err());
            auto object = rstd::move(scaled).unwrap();
            character.object = static_cast<Index>((next_object++).to_primitive());
            character.advance = object.advance; character.ascent = object.ascent; character.descent = object.descent; character.ink = object.ink;
            continue;
        }
        if (!character.newline && !character.tab) {
            auto selected=resolver_at(character); if (selected.is_err()) return Err(selected.unwrap_err());
            auto resolved = selected.unwrap()->resolve(collection, cp);
            if (resolved.is_err())
                return Err(resolved.unwrap_err());
            auto &font = resolved->font;
            auto glyph = resolved->glyph;
            character.missing = resolved->missing;
            usize slot{};
            for (; slot < result.fonts.len(); ++slot)
                if (result.fonts[slot]->id == font->id)
                    break;
            if (slot == result.fonts.len())
                result.fonts.push(font.clone());
            character.font = static_cast<Index>(slot.to_primitive());
            character.glyph = glyph;
            auto measured = font->measure(glyph);
            if (measured.is_err())
                return Err(measured.unwrap_err());
            character.advance = measured->advance;
            character.ink = measured->ink;
            if (!finite(character.advance) || character.advance < 0 || !character.ink.valid())
                return Err(TextError::Font);
        }
    }
    Character dot;
    if (want_dots) {
        dot = chars[chars.len() - usize(1)];
        chars.pop();
    }
    auto advance = [tab_width, request](const Character &c, Scalar x) {
        return c.tab && request.tabs == TextTabs::Ignore ? 0.0
               : c.tab ? (__builtin_floor(x / tab_width) + 1) * tab_width - x
                       : c.advance;
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
        auto next_start = end;
        bool clipped = request.max_lines &&
                       result.data.lines.len() + usize(1) == usize(request.max_lines) &&
                       end < chars.len();
        auto visible_end = end;
        if (request.whitespace == TextWhitespace::TrimWrap && end < chars.len() &&
            !chars[end].newline) {
            while (visible_end > start &&
                   (chars[visible_end - usize(1)].code == ' ' || chars[visible_end - usize(1)].tab))
                --visible_end;
            while (next_start < chars.len() &&
                   (chars[next_start].code == ' ' || chars[next_start].tab))
                ++next_start;
        }
        Vec<Character> visible;
        for (auto i = start; i < visible_end; ++i)
            visible.push(Character(chars[i]));
        if (clipped) {
            if (want_dots && request.width.is_some()) {
                Scalar used{};
                Vec<Scalar> widths;
                widths.push(0.0);
                for (auto &c : visible) {
                    used += advance(c, used);
                    widths.push(Scalar(used));
                }
                while (!visible.is_empty() && used + 3 * dot.advance > *request.width) {
                    visible.pop();
                    used = widths[visible.len()];
                }
            }
            auto cut = visible.is_empty() ? byte_start : visible[visible.len() - usize(1)].end;
            result.data.hidden =
                Some(TextRange{cut, static_cast<Index>(request.text->len().to_primitive())});
            query->hidden = Some(cut);
            if (want_dots) {
                dot.begin = dot.end = cut;
                for (int i = 0; i < 3; ++i)
                    visible.push(Character(dot));
            }
        }
        if (result.data.glyphs.len() + visible.len() > usize(collection.limits.glyphs))
            return Err(TextError::Limit);
        width = 0;
        auto &first = result.fonts[usize()];
        Scalar ascent = first->ascent, descent = first->descent, height = first->height;
        for (auto &c : visible) {
            width += advance(c, width);
            if (c.object != ~Index{}) {
                if (c.ascent > ascent) ascent = c.ascent;
                if (c.descent > descent) descent = c.descent;
                continue;
            }
            auto &font = result.fonts[usize(c.font)];
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
                      .byte_end = clipped              ? result.data.hidden->start
                                  : next_start > start ? chars[next_start - usize(1)].end
                                                       : byte_start,
                      .glyph_start = static_cast<Index>(result.data.glyphs.len().to_primitive()),
                      .logical = {x, y, width, height},
                      .baseline = y + ascent};
        auto stop_start = static_cast<Index>(query->stops.len().to_primitive());
        query->stops.push(Stop{byte_start, x});
        Scalar position{};
        auto line_index = static_cast<Index>(result.data.lines.len().to_primitive());
        for (auto &c : visible) {
            auto delta = advance(c, position);
            auto ink = c.ink;
            ink.x += x + position;
            ink.y += line.baseline;
            if (c.object != ~Index{}) {
                result.data.inline_boxes.push(TextInlineBox{c.object, line_index, {x + position, y, delta, height}, ink});
            } else {
                result.data.glyphs.push(TextGlyph{c.font,
                                              c.glyph,
                                              c.begin,
                                              c.end,
                                              line_index,
                                              {x + position, line.baseline},
                                              delta,
                                              !c.tab && c.code != ' ',
                                              c.missing,
                                              c.synthetic});
            }
            if (c.missing)
                ++result.data.missing_glyphs;
            unite(line.ink, ink);
            position += delta;
            query->stops.push(Stop{c.end, x + position});
            if (c.object != ~Index{}) continue;
            auto glyph_end = static_cast<Index>(result.data.glyphs.len().to_primitive());
            if (!result.data.runs.is_empty() &&
                result.data.runs[result.data.runs.len() - usize(1)].line == line_index &&
                result.data.runs[result.data.runs.len() - usize(1)].font == c.font &&
                result.data.runs[result.data.runs.len() - usize(1)].byte_end == c.begin) {
                auto &run = result.data.runs[result.data.runs.len() - usize(1)];
                run.byte_end = c.end;
                run.glyph_end = glyph_end;
            } else
                result.data.runs.push(
                    TextRun{c.font, c.begin, c.end, glyph_end - 1, glyph_end, line_index});
        }
        line.glyph_end = static_cast<Index>(result.data.glyphs.len().to_primitive());
        if (!clipped)
            for (auto i = visible_end; i < next_start; ++i)
                query->stops.push(Stop{chars[i].end, x + width});
        if (!clipped && end < chars.len() && chars[end].newline &&
            chars[end].end - chars[end].begin == 2)
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
        end = next_start;
        if (clipped || end == chars.len())
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
    if (request.width.is_none() && request.alignment != TextAlignment::Natural) {
        result.data.ink = {};
        usize box_index{};
        for (usize li{}; li < result.data.lines.len(); ++li) {
            auto &line = result.data.lines[li];
            auto extra = result.data.logical.width - line.logical.width;
            Scalar shift = request.alignment == TextAlignment::Center ? extra / 2
                           : request.alignment == TextAlignment::Right ||
                                   request.alignment == TextAlignment::End
                               ? extra
                               : 0;
            line.logical.x += shift;
            line.ink.x += shift;
            for (Index i = line.glyph_start; i < line.glyph_end; ++i)
                result.data.glyphs[usize(i)].position.x += shift;
            while (box_index < result.data.inline_boxes.len() && result.data.inline_boxes[box_index].line == li.to_primitive()) {
                auto &box = result.data.inline_boxes[box_index++];
                box.logical.x += shift; box.ink.x += shift;
            }
            auto &ql = query->lines[li];
            ql.bounds = line.logical;
            for (Index i = ql.start; i < ql.end; ++i)
                query->stops[usize(i)].x += shift;
            unite(result.data.ink, line.ink);
        }
    }
    ++collection.counters.layouts;
    return Ok(rstd::move(result));
}
} // namespace vectext
