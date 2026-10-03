export module vectext:layout;
export import vectext.error;
import :types;
using namespace rstd::prelude;
using namespace rstd::literals;

export namespace vectext {

enum class TextDirection { Auto, LeftToRight, RightToLeft };
enum class TextWrap { None, Word, WordOrCharacter };
enum class TextAlignment { Natural, Start, End, Left, Right, Center };
enum class TextAffinity { Downstream, Upstream };
struct TextRequest {
    ref<str> text{""_str}, family{"sans"_str}, language{"und"_str};
    Scalar size{16};
    Option<Scalar> width{};
    TextDirection direction{TextDirection::Auto};
    TextWrap wrap{TextWrap::WordOrCharacter};
    bool bold{}, italic{};
    TextAlignment alignment{TextAlignment::Natural};
    Scalar line_spacing{};
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
    bool drawable{true}, missing{};
};
struct TextLine {
    Index byte_start{}, byte_end{}, glyph_start{}, glyph_end{};
    Rect logical, ink;
    Scalar baseline{};
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
        auto bytes = data.text.as_str()->len().to_primitive();
        if (!data.logical.valid() || !data.ink.valid() || !finite(data.baseline) ||
            data.lines.is_empty())
            return Err(TextError::InvalidValue);
        auto text = data.text.as_str();
        Index missing{};
        for (const auto &glyph : data.glyphs) {
            if (usize(glyph.font) >= data.fonts.len() || usize(glyph.line) >= data.lines.len() ||
                glyph.byte_start >= glyph.byte_end || glyph.byte_end > bytes ||
                !text->is_char_boundary(usize(glyph.byte_start)) ||
                !text->is_char_boundary(usize(glyph.byte_end)) || !finite(glyph.position.x) ||
                !finite(glyph.position.y) || !finite(glyph.advance))
                return Err(TextError::InvalidValue);
            if (glyph.missing)
                ++missing;
        }
        if (missing != data.missing_glyphs)
            return Err(TextError::InvalidValue);
        Index next_glyph{}, line_index{};
        for (const auto &line : data.lines) {
            if (!line.logical.valid() || !line.ink.valid() || !finite(line.baseline) ||
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
                run.byte_start >= run.byte_end || run.byte_end > bytes ||
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
        return Ok(TextLayout(rstd::move(data)));
    }
    auto id() const -> Serial { return data_->id; }
    auto text() const -> ref<str> { return data_->value.text.as_str(); }
    auto logical_bounds() const -> Rect { return data_->value.logical; }
    auto ink_bounds() const -> Rect { return data_->value.ink; }
    auto baseline() const -> Scalar { return data_->value.baseline; }
    auto missing_glyphs() const -> Index { return data_->value.missing_glyphs; }
    auto glyphs() const -> slice<TextGlyph> { return data_->value.glyphs.as_slice(); }
    auto fonts() const -> slice<FontInstance> { return data_->value.fonts.as_slice(); }
    auto runs() const -> slice<TextRun> { return data_->value.runs.as_slice(); }
    auto revision() const -> Serial { return data_->value.revision; }
    auto lines() const -> slice<TextLine> { return data_->value.lines.as_slice(); }
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
};
struct TextService {
    virtual ~TextService() = default;
    virtual auto layout(TextRequest request) -> Result<TextLayout, TextError> = 0;
};
} // namespace vectext
