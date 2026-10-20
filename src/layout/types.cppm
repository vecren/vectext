export module vectext:layout.types;
export import vectext.error;
export import :boundaries;
import :types;
using namespace rstd::prelude;
using namespace rstd::literals;

export namespace vectext {

enum class TextDirection { Auto, LeftToRight, RightToLeft };
enum class TextWrap { None, Word, WordOrCharacter };
enum class TextAlignment { Natural, Start, End, Left, Right, Center };
enum class TextAffinity { Downstream, Upstream };
struct FontSource {
    Serial collection{}, source{};
    friend auto operator==(const FontSource &, const FontSource &) -> bool = default;
};
enum class TextOverflow { Clip, ThreeDots };
enum class TextTabs { Stops, Ignore };
enum class TextWhitespace { Preserve, TrimWrap };
enum class InvalidText { Reject, Replace };
struct TextFontStyle {
    bool bold{}, italic{};
    auto operator==(const TextFontStyle &) const -> bool = default;
};
struct TextFontSpan {
    TextRange range;
    Option<bool> bold, italic;
    auto operator==(const TextFontSpan &) const -> bool = default;
};
class TextFontSpans {
    struct Data {
        String text;
        Vec<TextFontSpan> spans;
        Data(ref<str> text, Vec<TextFontSpan> spans) : text(String::make(text)), spans(rstd::move(spans)) {}
    };
    Option<rstd::sync::Arc<Data>> data_;
    TextFontSpans(ref<str> text, Vec<TextFontSpan> spans)
        : data_(Some(rstd::sync::Arc<Data>::make(text,rstd::move(spans)))) {}
  public:
    TextFontSpans() = default;
    TextFontSpans(const TextFontSpans &other) : data_(other.data_.is_some() ? Some(other.data_->clone()) : None<rstd::sync::Arc<Data>>()) {}
    auto operator=(const TextFontSpans &other) -> TextFontSpans & {
        if (this != &other) data_ = other.data_.is_some() ? Some(other.data_->clone()) : None<rstd::sync::Arc<Data>>();
        return *this;
    }
    TextFontSpans(TextFontSpans &&) = default;
    auto operator=(TextFontSpans &&) -> TextFontSpans & = default;
    static auto create(ref<str> text, slice<TextFontSpan> spans) -> Result<TextFontSpans,TextError> {
        if (text->len() > usize(~Index{})) return Err(TextError::InvalidValue);
        Vec<TextFontSpan> checked; Index end{};
        for (auto span : spans) {
            if (span.range.start < end || span.range.start >= span.range.end || usize(span.range.end) > text->len() ||
                !text->is_char_boundary(usize(span.range.start)) || !text->is_char_boundary(usize(span.range.end)) ||
                (span.bold.is_none() && span.italic.is_none())) return Err(TextError::InvalidValue);
            end = span.range.end;
            if (!checked.is_empty() && checked[checked.len()-usize(1)].range.end == span.range.start &&
                checked[checked.len()-usize(1)].bold == span.bold && checked[checked.len()-usize(1)].italic == span.italic)
                checked[checked.len()-usize(1)].range.end = span.range.end;
            else checked.push(rstd::move(span));
        }
        if (checked.is_empty()) return Ok(TextFontSpans{});
        return Ok(TextFontSpans(text,rstd::move(checked)));
    }
    auto spans() const -> slice<TextFontSpan> { return data_.is_some() ? (*data_)->spans.as_slice() : slice<TextFontSpan>{}; }
    auto matches(ref<str> text) const -> bool { return data_.is_none() || (*data_)->text.as_str() == text; }
    auto style(Index byte, TextFontStyle inherited = {}) const -> TextFontStyle {
        auto values = spans(); usize first{},last=values.len();
        while (first < last) {
            auto middle=first+(last-first)/usize(2); const auto &span=values[middle];
            if (byte < span.range.start) last=middle;
            else if (byte >= span.range.end) first=middle+usize(1);
            else return {span.bold.is_some() ? *span.bold : inherited.bold,span.italic.is_some() ? *span.italic : inherited.italic};
        }
        return inherited;
    }
    auto operator==(const TextFontSpans &other) const -> bool {
        if (data_.is_none() || other.data_.is_none()) return data_.is_none() && other.data_.is_none();
        return rstd::sync::Arc<Data>::ptr_eq(*data_,*other.data_) || ((*data_)->text == (*other.data_)->text && spans() == other.spans());
    }
};
enum class TextInlineUnit { Logical, Em };
struct TextInlineObject {
    TextRange range;
    Scalar advance{}, ascent{}, descent{};
    Rect ink;
    TextInlineUnit unit{TextInlineUnit::Logical};
    auto valid() const -> bool {
        return finite(advance) && advance >= 0 && finite(ascent) && ascent >= 0 &&
            finite(descent) && descent >= 0 && ink.valid() &&
            (unit == TextInlineUnit::Logical || unit == TextInlineUnit::Em);
    }
    auto scaled(Scalar em) const -> Result<TextInlineObject, TextError> {
        if (!valid() || !finite(em) || em <= 0) return Err(TextError::InvalidValue);
        auto factor = unit == TextInlineUnit::Em ? em : 1;
        TextInlineObject result{range, advance * factor, ascent * factor, descent * factor,
            {ink.x * factor, ink.y * factor, ink.width * factor, ink.height * factor}};
        if (!result.valid() || result.advance > 1048576 || result.ascent + result.descent > 1048576 ||
            __builtin_fabs(result.ink.x) > 1048576 || __builtin_fabs(result.ink.y) > 1048576 ||
            result.ink.width > 1048576 || result.ink.height > 1048576) return Err(TextError::Limit);
        return Ok(result);
    }
    auto operator==(const TextInlineObject &) const -> bool = default;
};
class TextInlineObjects {
    struct Data {
        String text;
        Vec<TextInlineObject> objects;
        Data(ref<str> text, Vec<TextInlineObject> objects) : text(String::make(text)), objects(rstd::move(objects)) {}
    };
    Option<rstd::sync::Arc<Data>> data_;
    TextInlineObjects(ref<str> text, Vec<TextInlineObject> objects)
        : data_(Some(rstd::sync::Arc<Data>::make(text, rstd::move(objects)))) {}
  public:
    TextInlineObjects() = default;
    TextInlineObjects(const TextInlineObjects &other) : data_(other.data_.is_some() ? Some(other.data_->clone()) : None<rstd::sync::Arc<Data>>()) {}
    auto operator=(const TextInlineObjects &other) -> TextInlineObjects & {
        if (this != &other) data_ = other.data_.is_some() ? Some(other.data_->clone()) : None<rstd::sync::Arc<Data>>();
        return *this;
    }
    TextInlineObjects(TextInlineObjects &&) = default;
    auto operator=(TextInlineObjects &&) -> TextInlineObjects & = default;
    static auto create(ref<str> text, slice<TextInlineObject> objects) -> Result<TextInlineObjects, TextError> {
        if (text->len() > usize(~Index{})) return Err(TextError::InvalidValue);
        Index end{};
        for (const auto &object : objects) {
            auto range = object.range;
            if (!object.valid() || range.start < end || range.end < range.start || range.end - range.start != 3 ||
                usize(range.end) > text->len() || text->as_bytes()[usize(range.start)] != u8(0xef) ||
                text->as_bytes()[usize(range.start + 1)] != u8(0xbf) || text->as_bytes()[usize(range.start + 2)] != u8(0xbc))
                return Err(TextError::InvalidValue);
            end = range.end;
        }
        if (objects.is_empty()) return Ok(TextInlineObjects{});
        return Ok(TextInlineObjects(text, Vec<TextInlineObject>::from(objects)));
    }
    auto objects() const -> slice<TextInlineObject> { return data_.is_some() ? (*data_)->objects.as_slice() : slice<TextInlineObject>{}; }
    auto matches(ref<str> text) const -> bool { return data_.is_none() || (*data_)->text.as_str() == text; }
    auto overlaps(TextRange range) const -> bool {
        auto values = objects(); usize first{}, last = values.len();
        while (first < last) {
            auto middle = first + (last - first) / usize(2);
            if (values[middle].range.end <= range.start) first = middle + usize(1); else last = middle;
        }
        return range.start < range.end && first < values.len() && values[first].range.start < range.end;
    }
    auto operator==(const TextInlineObjects &other) const -> bool {
        if (data_.is_none() || other.data_.is_none()) return data_.is_none() && other.data_.is_none();
        return rstd::sync::Arc<Data>::ptr_eq(*data_, *other.data_) ||
            ((*data_)->text == (*other.data_)->text && objects() == other.objects());
    }
};
struct TextRequest {
    ref<str> text{""_str}, family{"sans"_str}, language{"und"_str};
    Scalar size{16};
    Option<Scalar> width{};
    TextDirection direction{TextDirection::Auto};
    TextWrap wrap{TextWrap::WordOrCharacter};
    bool bold{}, italic{};
    TextAlignment alignment{TextAlignment::Natural};
    Scalar line_spacing{};
    Option<FontSource> source;
    Index max_lines{};
    TextOverflow overflow{TextOverflow::Clip};
    TextTabs tabs{TextTabs::Stops};
    TextWhitespace whitespace{TextWhitespace::Preserve};
    // Byte input is decoded once; all result indices refer to normalized text().
    Option<slice<u8>> bytes;
    InvalidText invalid_text{InvalidText::Reject};
    const TextBoundaries *boundaries{};
    TextInlineObjects inline_objects;
    TextFontSpans font_spans;
};
struct TextFormat {
    ref<str> family{"sans"_str}, language{"und"_str};
    Scalar size{16};
    TextDirection direction{TextDirection::Auto};
    TextWrap wrap{TextWrap::WordOrCharacter};
    bool bold{}, italic{};
    TextAlignment alignment{TextAlignment::Natural};
    Scalar line_spacing{};
    auto valid() const -> bool {
        return finite(size) && size > 0 && finite(line_spacing) && line_spacing >= 0 &&
               (alignment >= TextAlignment::Natural && alignment <= TextAlignment::Center) &&
               (direction == TextDirection::Auto || direction == TextDirection::LeftToRight ||
                direction == TextDirection::RightToLeft) &&
               (wrap == TextWrap::None || wrap == TextWrap::Word ||
                wrap == TextWrap::WordOrCharacter);
    }
};
struct GlyphBitmap {
    Index width{}, height{};
    Point origin;
    Scalar scale{1};
    Vec<unsigned char> coverage;
};
struct FontRasterizer {
    virtual ~FontRasterizer() = default;
    virtual auto rasterize(Index glyph, Scalar scale) const -> Result<GlyphBitmap, TextError> = 0;
};
class SystemTextService;
class FontInstance {
    struct Data {
        Serial id;
        String name;
        Box<FontRasterizer> rasterizer;
        Data(Serial id, String name, Box<FontRasterizer> rasterizer)
            : id(id), name(rstd::move(name)), rasterizer(rstd::move(rasterizer)) {}
    };
    rstd::sync::Arc<Data> data_;

    friend class SystemTextService;
    FontInstance(Serial id, String name, Box<FontRasterizer> rasterizer)
        : data_(rstd::sync::Arc<Data>::make(id, rstd::move(name), rstd::move(rasterizer))) {}

  public:
    FontInstance(String name, Box<FontRasterizer> rasterizer)
        : data_(rstd::sync::Arc<Data>::make(new_identity(), rstd::move(name),
                                            rstd::move(rasterizer))) {}
    FontInstance(const FontInstance &other) : data_(other.data_.clone()) {}
    auto operator=(const FontInstance &other) -> FontInstance & {
        if (this != &other)
            data_ = other.data_.clone();
        return *this;
    }
    FontInstance(FontInstance &&) = default;
    auto operator=(FontInstance &&) -> FontInstance & = default;
    auto id() const -> Serial { return data_->id; }
    auto name() const -> ref<str> { return data_->name.as_str(); }
    auto rasterize(Index glyph, Scalar scale) const -> Result<GlyphBitmap, TextError> {
        if (!finite(scale) || scale <= 0 || scale > 64)
            return Err(TextError::InvalidValue);
        auto result = data_->rasterizer->rasterize(glyph, scale);
        if (result.is_err())
            return Err(result.unwrap_err());
        auto bitmap = rstd::move(result).unwrap();
        if (bitmap.scale != scale || !finite(bitmap.origin.x) || !finite(bitmap.origin.y) ||
            Serial(bitmap.width) * bitmap.height != bitmap.coverage.len().to_primitive())
            return Err(TextError::Raster);
        return Ok(rstd::move(bitmap));
    }
};
struct TextGlyph {
    Index font{}, glyph{}, byte_start{}, byte_end{}, line{};
    Point position;
    Scalar advance{};
    bool drawable{true}, missing{}, synthetic{};
};
struct TextLine {
    Index byte_start{}, byte_end{}, glyph_start{}, glyph_end{};
    Rect logical, ink;
    Scalar baseline{};
    Scalar ascent{}, descent{};
};
struct TextRun {
    Index font{}, byte_start{}, byte_end{}, glyph_start{}, glyph_end{}, line{};
};
struct TextHit {
    Index byte_index{};
    bool inside{};
    TextAffinity affinity{TextAffinity::Downstream};
};
struct TextCaret {
    Rect bounds;
};
struct TextRangeBox {
    Index line{};
    Rect bounds;
    Scalar baseline{};
    auto operator==(const TextRangeBox &) const -> bool = default;
};
struct TextInlineBox {
    Index object{}, line{};
    Rect logical, ink;
};
struct TextQueries {
    virtual ~TextQueries() = default;
    virtual auto hit(Point point) const -> Result<TextHit, TextError> = 0;
    virtual auto caret(Index byte_index) const -> Result<TextCaret, TextError> = 0;
    virtual auto caret(Index byte_index, TextAffinity) const -> Result<TextCaret, TextError> {
        return caret(byte_index);
    }
};
struct TextLayoutData {
    String text;
    Rect logical, ink;
    Scalar baseline{};
    Index missing_glyphs{};
    Vec<FontInstance> fonts;
    Vec<TextGlyph> glyphs;
    Vec<TextLine> lines;
    Box<TextQueries> queries;
    Vec<TextRun> runs;
    Serial revision{};
    Option<TextRange> hidden;
    TextBoundaries boundaries;
    TextInlineObjects inline_objects;
    Vec<TextInlineBox> inline_boxes;
    TextFontSpans font_spans;
};
class TextLayout {
    struct Data {
        Serial id{new_identity()};
        TextLayoutData value;
        explicit Data(TextLayoutData value) : value(rstd::move(value)) {}
    };
    rstd::sync::Arc<Data> data_;
    explicit TextLayout(TextLayoutData data)
        : data_(rstd::sync::Arc<Data>::make(rstd::move(data))) {}

  public:
    TextLayout(const TextLayout &other) : data_(other.data_.clone()) {}
    auto operator=(const TextLayout &other) -> TextLayout & {
        if (this != &other)
            data_ = other.data_.clone();
        return *this;
    }
    TextLayout(TextLayout &&) = default;
    auto operator=(TextLayout &&) -> TextLayout & = default;
    static auto create(TextLayoutData data) -> Result<TextLayout, TextError> {
        if (data.boundaries.present()) {
            if (data.boundaries.text() != data.text.as_str()) return Err(TextError::InvalidValue);
        } else {
            auto boundaries = TextBoundaries::create(data.text.as_str());
            if (boundaries.is_err()) return Err(boundaries.unwrap_err());
            data.boundaries = rstd::move(boundaries).unwrap();
        }
        auto bytes = data.text.as_str()->len().to_primitive();
        if (!data.logical.valid() || !data.ink.valid() || !finite(data.baseline) ||
            data.lines.is_empty())
            return Err(TextError::InvalidValue);
        auto text = data.text.as_str();
        if (!data.inline_objects.matches(text) || !data.font_spans.matches(text)) return Err(TextError::InvalidValue);
        if (data.hidden.is_some() &&
            (data.hidden->start > data.hidden->end || data.hidden->end != bytes ||
             !text->is_char_boundary(usize(data.hidden->start))))
            return Err(TextError::InvalidValue);
        Index missing{};
        for (const auto &glyph : data.glyphs) {
            if (usize(glyph.font) >= data.fonts.len() || usize(glyph.line) >= data.lines.len() ||
                (glyph.synthetic ? glyph.byte_start != glyph.byte_end
                                 : glyph.byte_start >= glyph.byte_end) ||
                glyph.byte_end > bytes || !text->is_char_boundary(usize(glyph.byte_start)) ||
                !text->is_char_boundary(usize(glyph.byte_end)) || !finite(glyph.position.x) ||
                !finite(glyph.position.y) || !finite(glyph.advance))
                return Err(TextError::InvalidValue);
            if (glyph.synthetic &&
                (data.hidden.is_none() || glyph.byte_start != data.hidden->start))
                return Err(TextError::InvalidValue);
            if (data.inline_objects.overlaps({glyph.byte_start, glyph.byte_end})) return Err(TextError::InvalidValue);
            if (glyph.missing)
                ++missing;
        }
        if (missing != data.missing_glyphs)
            return Err(TextError::InvalidValue);
        Index next_glyph{}, line_index{};
        for (const auto &line : data.lines) {
            if (!line.logical.valid() || !line.ink.valid() || !finite(line.baseline) ||
                !finite(line.ascent) || !finite(line.descent) || line.ascent < 0 ||
                line.descent < 0 || !finite(line.baseline - line.ascent) ||
                !finite(line.baseline + line.descent) ||
                line.byte_start > line.byte_end || line.byte_end > bytes ||
                !text->is_char_boundary(usize(line.byte_start)) ||
                !text->is_char_boundary(usize(line.byte_end)) || line.glyph_start != next_glyph ||
                line.glyph_start > line.glyph_end || usize(line.glyph_end) > data.glyphs.len())
                return Err(TextError::InvalidValue);
            for (Index i = line.glyph_start; i < line.glyph_end; ++i) {
                const auto &glyph = data.glyphs[usize(i)];
                if (glyph.line != line_index || glyph.byte_start < line.byte_start ||
                    glyph.byte_end > line.byte_end)
                    return Err(TextError::InvalidValue);
            }
            next_glyph = line.glyph_end;
            ++line_index;
        }
        if (usize(next_glyph) != data.glyphs.len())
            return Err(TextError::InvalidValue);
        Index run_glyph{};
        for (const auto &run : data.runs) {
            if (usize(run.font) >= data.fonts.len() || usize(run.line) >= data.lines.len() ||
                run.byte_start > run.byte_end || run.byte_end > bytes ||
                !text->is_char_boundary(usize(run.byte_start)) ||
                !text->is_char_boundary(usize(run.byte_end)) || run.glyph_start != run_glyph ||
                run.glyph_end < run.glyph_start || usize(run.glyph_end) > data.glyphs.len())
                return Err(TextError::InvalidValue);
            const auto &line = data.lines[usize(run.line)];
            if (run.byte_start < line.byte_start || run.byte_end > line.byte_end ||
                run.glyph_start < line.glyph_start || run.glyph_end > line.glyph_end)
                return Err(TextError::InvalidValue);
            for (Index i = run.glyph_start; i < run.glyph_end; ++i) {
                const auto &glyph = data.glyphs[usize(i)];
                if (glyph.font != run.font || glyph.line != run.line ||
                    glyph.byte_start < run.byte_start || glyph.byte_end > run.byte_end)
                    return Err(TextError::InvalidValue);
            }
            run_glyph = run.glyph_end;
        }
        if (!data.runs.is_empty() && usize(run_glyph) != data.glyphs.len())
            return Err(TextError::InvalidValue);
        usize box_index{};
        auto objects = data.inline_objects.objects();
        for (usize i{}; i < objects.len(); ++i) {
            const auto &object = objects[i];
            if (data.hidden.is_some() && object.range.start >= data.hidden->start) break;
            if (box_index >= data.inline_boxes.len()) return Err(TextError::InvalidValue);
            const auto &box = data.inline_boxes[box_index++];
            if (box.object != i.to_primitive() || usize(box.line) >= data.lines.len() || !box.logical.valid() || !box.ink.valid())
                return Err(TextError::InvalidValue);
            if (box_index > usize(1) && box.line < data.inline_boxes[box_index - usize(2)].line) return Err(TextError::InvalidValue);
            const auto &line = data.lines[usize(box.line)];
            if (object.range.start < line.byte_start || object.range.end > line.byte_end ||
                box.logical.y != line.logical.y || box.logical.height != line.logical.height)
                return Err(TextError::InvalidValue);
        }
        if (box_index != data.inline_boxes.len()) return Err(TextError::InvalidValue);
        return Ok(TextLayout(rstd::move(data)));
    }
    auto id() const -> Serial { return data_->id; }
    auto text() const -> ref<str> { return data_->value.text.as_str(); }
    auto logical_bounds() const -> Rect { return data_->value.logical; }
    // Font extents retain interline spacing, without leading after the last line.
    auto typographic_bounds() const -> Rect {
        auto bounds = logical_bounds();
        Scalar top = lines()[usize()].baseline - lines()[usize()].ascent;
        Scalar bottom = top;
        for (const auto &line : lines()) {
            top = rstd::cmp::min(top, line.baseline - line.ascent);
            bottom = rstd::cmp::max(bottom, line.baseline + line.descent);
        }
        return {bounds.x, top, bounds.width, bottom - top};
    }
    auto ink_bounds() const -> Rect { return data_->value.ink; }
    auto baseline() const -> Scalar { return data_->value.baseline; }
    auto missing_glyphs() const -> Index { return data_->value.missing_glyphs; }
    auto glyphs() const -> slice<TextGlyph> { return data_->value.glyphs.as_slice(); }
    auto fonts() const -> slice<FontInstance> { return data_->value.fonts.as_slice(); }
    auto runs() const -> slice<TextRun> { return data_->value.runs.as_slice(); }
    auto hidden_range() const -> Option<TextRange> { return data_->value.hidden; }
    auto revision() const -> Serial { return data_->value.revision; }
    auto lines() const -> slice<TextLine> { return data_->value.lines.as_slice(); }
    auto boundaries() const -> const TextBoundaries & { return data_->value.boundaries; }
    auto inline_objects() const -> const TextInlineObjects & { return data_->value.inline_objects; }
    auto font_spans() const -> const TextFontSpans & { return data_->value.font_spans; }
    auto inline_boxes() const -> slice<TextInlineBox> { return data_->value.inline_boxes.as_slice(); }
    // Logical advances cover whole intersecting clusters, excluding synthetic glyphs.
    auto range_boxes(TextRange range) const -> Result<Vec<TextRangeBox>, TextError> {
        if (range.start > range.end || usize(range.end) > text()->len() ||
            !text()->is_char_boundary(usize(range.start)) || !text()->is_char_boundary(usize(range.end)))
            return Err(TextError::InvalidIndex);
        Vec<TextRangeBox> result;
        if (range.start == range.end) return Ok(rstd::move(result));
        if (data_->value.hidden.is_some() && range.end > data_->value.hidden->start)
            range.end = data_->value.hidden->start;
        usize object_begin{};
        for (usize line_index{}; line_index < lines().len(); ++line_index) {
            const auto &line = lines()[line_index];
            if (range.start >= line.byte_end || range.end <= line.byte_start) continue;
            Vec<Rect> intervals;
            for (Index i = line.glyph_start; i < line.glyph_end; ++i) {
                const auto &glyph = glyphs()[usize(i)];
                if (glyph.synthetic || glyph.byte_start >= range.end || glyph.byte_end <= range.start) continue;
                auto end = glyph.position.x + glyph.advance;
                auto left = rstd::cmp::min(glyph.position.x, end), right = rstd::cmp::max(glyph.position.x, end);
                Rect bounds{left, line.logical.y, right - left, line.logical.height};
                if (!bounds.valid()) return Err(TextError::Layout);
                if (bounds.width > 0 && bounds.height > 0) intervals.push(rstd::move(bounds));
            }
            auto boxes = inline_boxes();
            while (object_begin < boxes.len() && boxes[object_begin].line < line_index.to_primitive()) ++object_begin;
            for (auto i = object_begin; i < boxes.len() && boxes[i].line == line_index.to_primitive(); ++i) {
                auto object_range = inline_objects().objects()[usize(boxes[i].object)].range;
                if (object_range.start < range.end && object_range.end > range.start && boxes[i].logical.width > 0 && boxes[i].logical.height > 0)
                    intervals.push(Rect(boxes[i].logical));
            }
            rstd::slice_::sort_unstable_by(intervals.as_mut_slice().as_mut_ref(),
                [](const Rect &left, const Rect &right) { return left.x < right.x; });
            for (auto bounds : intervals) {
                if (!result.is_empty()) {
                    auto &previous = result[result.len() - usize(1)];
                    auto right = previous.bounds.x + previous.bounds.width;
                    auto tolerance = __DBL_EPSILON__ * 8 * rstd::cmp::max(1.0, rstd::cmp::max(__builtin_fabs(right), __builtin_fabs(bounds.x)));
                    if (previous.line == line_index.to_primitive() && bounds.x <= right + tolerance) {
                        previous.bounds.width = rstd::cmp::max(previous.bounds.x + previous.bounds.width, bounds.x + bounds.width) - previous.bounds.x;
                        if (!previous.bounds.valid()) return Err(TextError::Layout);
                        continue;
                    }
                }
                result.push(TextRangeBox{static_cast<Index>(line_index.to_primitive()), bounds, line.baseline});
            }
        }
        return Ok(rstd::move(result));
    }
    auto hit(Point point) const -> Result<TextHit, TextError> {
        if (!finite(point.x) || !finite(point.y))
            return Err(TextError::InvalidValue);
        auto result = data_->value.queries->hit(point);
        if (result.is_ok() && (!text()->is_char_boundary(usize(result.unwrap().byte_index)) ||
                               (result.unwrap().affinity != TextAffinity::Downstream &&
                                result.unwrap().affinity != TextAffinity::Upstream)))
            return Err(TextError::Layout);
        return result;
    }
    auto caret(Index byte_index, TextAffinity affinity = TextAffinity::Downstream) const
        -> Result<TextCaret, TextError> {
        if (affinity != TextAffinity::Downstream && affinity != TextAffinity::Upstream)
            return Err(TextError::InvalidValue);
        if (!text()->is_char_boundary(usize(byte_index)))
            return Err(TextError::InvalidIndex);
        auto result = data_->value.queries->caret(byte_index, affinity);
        if (result.is_ok() && !result.unwrap().bounds.valid())
            return Err(TextError::Layout);
        return result;
    }
    auto caret_grapheme(Index byte_index, TextAffinity affinity = TextAffinity::Downstream) const -> Result<TextCaret, TextError> {
        if (!boundaries().contains(byte_index)) return Err(TextError::InvalidIndex);
        return caret(byte_index, affinity);
    }
    auto hit_grapheme(Point point) const -> Result<TextHit, TextError> {
        auto result = hit(point);
        if (result.is_err()) return result;
        auto found = result.unwrap();
        auto before = boundaries().snap(found.byte_index, TextBoundaryBias::Before);
        auto after = boundaries().snap(found.byte_index, TextBoundaryBias::After);
        if (before.is_err() || after.is_err()) return Err(TextError::Layout);
        if (before.unwrap() == after.unwrap()) return result;
        auto left = caret(before.unwrap(), found.affinity), right = caret(after.unwrap(), found.affinity);
        if (left.is_err() || right.is_err()) return Err(TextError::Layout);
        auto distance = [&](Rect rect) {
            auto x = point.x - rect.x, y = point.y - rect.y - rect.height / 2;
            return x * x + y * y;
        };
        found.byte_index = distance(left.unwrap().bounds) <= distance(right.unwrap().bounds) ? before.unwrap() : after.unwrap();
        return Ok(found);
    }
};
struct TextService {
    virtual ~TextService() = default;
    virtual auto layout(TextRequest request) -> Result<TextLayout, TextError> = 0;
};
} // namespace vectext
