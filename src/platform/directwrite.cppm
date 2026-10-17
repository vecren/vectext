module vectext:platform.directwrite;
import :font;
import :layout;
import :platform.common;
import vectext.ffi.directwrite;
using namespace rstd::prelude;
using namespace rstd::literals;

namespace vectext::directwrite {
namespace dw = ffi::directwrite;
using rstd::sync::Arc;
using Pattern = dw::Owned<dw::IDWriteFontFace3>;
struct Context {
    dw::Owned<dw::IDWriteFactory5> factory;
};
// DirectWrite retains the byte owner until this source-specific loader is released.
struct MemoryLoader {
    Arc<Context> context;
    dw::Owned<dw::IDWriteInMemoryFontFileLoader> loader;
    bool registered{};
    explicit MemoryLoader(Arc<Context> context) : context(rstd::move(context)) {}
    ~MemoryLoader() {
        if (registered)
            context->factory->UnregisterFontFileLoader(loader.get());
    }
};
struct FontBytes final : dw::RefCounted<dw::IUnknown, FontBytes> {
    Vec<u8> bytes;
    explicit FontBytes(Vec<u8> data) : bytes(rstd::move(data)) {}
};
struct SourceData {
    Option<Arc<Context>> context;
    Option<Arc<MemoryLoader>> loader;
    dw::Owned<FontBytes> memory;
    dw::Owned<dw::IDWriteFontFile> file;
    Pattern pattern;
    Index index{};
    String name, family;
    bool bold{}, italic{};
};
auto source_bold(const SourceData &source) -> bool { return source.bold || (source.pattern->GetSimulations() & dw::DWRITE_FONT_SIMULATIONS_BOLD); }
auto source_bytes(const SourceData &source) -> Serial {
    return source.memory ? source.memory->bytes.len().to_primitive() : 0;
}
auto wide(ref<str> text) -> Vec<wchar_t> {
    Vec<wchar_t> result;
    for (auto character : text->chars()) {
        auto cp = static_cast<unsigned>(character.to_primitive());
        if (cp > 0xffff) {
            result.push(static_cast<wchar_t>(0xd800 + ((cp - 0x10000) >> 10)));
            result.push(static_cast<wchar_t>(0xdc00 + ((cp - 0x10000) & 0x3ff)));
        } else
            result.push(static_cast<wchar_t>(cp));
    }
    result.push(0);
    return result;
}
auto localized(dw::IDWriteLocalizedStrings *strings) -> Result<String, TextError> {
    if (!strings || !strings->GetCount())
        return Err(TextError::Font);
    unsigned index{}, length{};
    dw::BOOL found{};
    if (strings->FindLocaleName(L"en-us", &index, &found) < 0)
        return Err(TextError::Font);
    if (!found)
        index = 0;
    if (strings->GetStringLength(index, &length) < 0 || length > 4096)
        return Err(TextError::Font);
    Vec<wchar_t> buffer;
    buffer.resize(usize(length + 1), 0);
    if (strings->GetString(index, buffer.as_mut_slice().as_raw_ptr(), length + 1) < 0)
        return Err(TextError::Font);
    String result;
    for (unsigned i = 0; i < length; ++i) {
        unsigned cp = buffer[usize(i)];
        if (cp >= 0xd800 && cp <= 0xdbff) {
            if (++i >= length)
                return Err(TextError::Font);
            unsigned low = buffer[usize(i)];
            if (low < 0xdc00 || low > 0xdfff)
                return Err(TextError::Font);
            cp = 0x10000 + ((cp - 0xd800) << 10) + low - 0xdc00;
        } else if (cp >= 0xdc00 && cp <= 0xdfff)
            return Err(TextError::Font);
        result.push(static_cast<char32_t>(cp));
    }
    return Ok(rstd::move(result));
}
auto source_metadata(SourceData &source) -> Result<empty, TextError> {
    dw::Owned<dw::IDWriteLocalizedStrings> names;
    if (source.pattern->GetFamilyNames(names.put()) < 0)
        return Err(TextError::Font);
    auto family = localized(names.get());
    if (family.is_err())
        return Err(family.unwrap_err());
    source.family = rstd::move(family).unwrap();
    source.name = source.family.clone();
    if (source.pattern->GetFaceNames(names.put()) < 0)
        return Err(TextError::Font);
    auto style = localized(names.get());
    if (style.is_err())
        return Err(style.unwrap_err());
    source.name.push_ascii(' ');
    source.name.push_str(style->as_str());
    source.bold = source.pattern->GetWeight() >= dw::DWRITE_FONT_WEIGHT_BOLD;
    source.italic = source.pattern->GetStyle() != dw::DWRITE_FONT_STYLE_NORMAL;
    source.index = source.pattern->GetIndex();
    return Ok(empty{});
}
struct CachedMatch {
    String family, language;
    bool bold{}, italic{};
    Index codepoint{};
    Pattern pattern;
    Scalar scale{1};
};
struct Config {
    bool system{};
    Option<Arc<Context>> context;
    dw::Owned<dw::IDWriteFontCollection> fonts;
    dw::Owned<dw::IDWriteFontFallback> fallback;
    Vec<CachedMatch> matches;
};
auto make_config(bool system, ref<str> path) -> Result<Config, TextError> {
    if (!path->is_empty())
        return Err(TextError::Unsupported);
    Config result;
    result.system = system;
    auto context = Arc<Context>::make();
    if (dw::create_factory(context->factory) < 0)
        return Err(TextError::Unsupported);
    if (system && (context->factory->GetSystemFontCollection(result.fonts.put(), true) < 0 ||
                   context->factory->GetSystemFontFallback(result.fallback.put()) < 0))
        return Err(TextError::Font);
    result.context = Some(rstd::move(context));
    return Ok(rstd::move(result));
}
auto parse_source(Config &config, Vec<u8> bytes, Index index, ref<str>)
    -> Result<SourceData, TextError> {
    SourceData result;
    result.context = Some(config.context->clone());
    auto &context = **result.context;
    auto loader = Arc<MemoryLoader>::make(config.context->clone());
    if (context.factory->CreateInMemoryFontFileLoader(loader->loader.put()) < 0)
        return Err(TextError::Unsupported);
    if (context.factory->RegisterFontFileLoader(loader->loader.get()) < 0)
        return Err(TextError::Font);
    loader->registered = true;
    result.loader = Some(rstd::move(loader));
    result.memory.reset(new FontBytes(rstd::move(bytes)));
    auto &data = result.memory->bytes;
    if ((*result.loader)
            ->loader->CreateInMemoryFontFileReference(
                context.factory.get(), data.as_slice().as_raw_ptr(),
                static_cast<unsigned>(data.len().to_primitive()), result.memory.get(),
                result.file.put()) < 0)
        return Err(TextError::Font);
    dw::BOOL supported{};
    dw::DWRITE_FONT_FILE_TYPE file_type{};
    dw::DWRITE_FONT_FACE_TYPE face_type{};
    unsigned faces{};
    if (result.file->Analyze(&supported, &file_type, &face_type, &faces) < 0 || !supported ||
        index >= faces)
        return Err(TextError::Font);
    dw::Owned<dw::IDWriteFontFace> face;
    auto *file = result.file.get();
    if (context.factory->CreateFontFace(face_type, 1, &file, index,
                                        dw::DWRITE_FONT_SIMULATIONS_NONE, face.put()) < 0 ||
        dw::query(face.get(), result.pattern) < 0)
        return Err(TextError::Font);
    auto metadata = source_metadata(result);
    if (metadata.is_err())
        return Err(metadata.unwrap_err());
    return Ok(rstd::move(result));
}
auto same(Pattern const &a, Pattern const &b) -> bool {
    dw::Owned<dw::IDWriteFontFaceReference> left, right;
    return a->GetFontFaceReference(left.put()) >= 0 && b->GetFontFaceReference(right.put()) >= 0 &&
           left->Equals(right.get());
}
auto generic_family(ref<str> family) -> ref<str> {
    if (family->is_empty() || family == "sans-serif"_str)
        return "Segoe UI"_str;
    if (family == "serif"_str)
        return "Times New Roman"_str;
    if (family == "monospace"_str)
        return "Consolas"_str;
    return family;
}
struct AnalysisSource final : dw::RefCounted<dw::IDWriteTextAnalysisSource, AnalysisSource> {
    wchar_t text[2]{};
    unsigned length{};
    Vec<wchar_t> locale;
    AnalysisSource(Index cp, ref<str> language) : locale(wide(language)) {
        length = cp > 0xffff ? 2 : 1;
        text[0] = static_cast<wchar_t>(length == 1 ? cp : 0xd800 + ((cp - 0x10000) >> 10));
        if (length == 2)
            text[1] = static_cast<wchar_t>(0xdc00 + ((cp - 0x10000) & 0x3ff));
    }
    dw::HRESULT __stdcall GetTextAtPosition(unsigned position, const wchar_t **value,
                                            unsigned *count) override {
        *value = position < length ? text + position : nullptr;
        *count = position < length ? length - position : 0;
        return dw::success;
    }
    dw::HRESULT __stdcall GetTextBeforePosition(unsigned position, const wchar_t **value,
                                                unsigned *count) override {
        *value = position && position <= length ? text : nullptr;
        *count = position <= length ? position : 0;
        return dw::success;
    }
    dw::DWRITE_READING_DIRECTION __stdcall GetParagraphReadingDirection() override {
        return dw::DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
    }
    dw::HRESULT __stdcall GetLocaleName(unsigned position, unsigned *count,
                                        const wchar_t **name) override {
        if (position >= length)
            return dw::invalid_argument;
        *count = length - position;
        *name = locale.as_slice().as_raw_ptr();
        return dw::success;
    }
    dw::HRESULT __stdcall
    GetNumberSubstitution(unsigned position, unsigned *count,
                          dw::IDWriteNumberSubstitution **substitution) override {
        if (position >= length)
            return dw::invalid_argument;
        *count = length - position;
        *substitution = nullptr;
        return dw::success;
    }
};
struct Match {
    Pattern pattern;
    Scalar scale{1};
};
auto system_match(Config &config, TextRequest request, Index cp) -> Result<Match, TextError> {
    for (auto &entry : config.matches)
        if (entry.family.as_str() == request.family &&
            entry.language.as_str() == request.language && entry.bold == request.bold &&
            entry.italic == request.italic && entry.codepoint == cp)
            return Ok(Match{dw::retain(entry.pattern.get()), entry.scale});
    auto family = wide(generic_family(request.family));
    auto weight = request.bold ? dw::DWRITE_FONT_WEIGHT_BOLD : dw::DWRITE_FONT_WEIGHT_NORMAL;
    auto style = request.italic ? dw::DWRITE_FONT_STYLE_ITALIC : dw::DWRITE_FONT_STYLE_NORMAL;
    dw::Owned<dw::IDWriteFont> font;
    float scale = 1;
    if (cp) {
        dw::Owned<AnalysisSource> source(new AnalysisSource(cp, request.language));
        unsigned mapped{};
        if (config.fallback->MapCharacters(
                source.get(), 0, source->length, config.fonts.get(), family.as_slice().as_raw_ptr(),
                weight, style, dw::DWRITE_FONT_STRETCH_NORMAL, &mapped, font.put(), &scale) < 0 ||
            mapped != source->length || !finite(scale) || scale <= 0)
            return Err(TextError::Font);
    } else {
        unsigned index{};
        dw::BOOL found{};
        if (config.fonts->FindFamilyName(family.as_slice().as_raw_ptr(), &index, &found) < 0)
            return Err(TextError::Font);
        if (!found && config.fonts->FindFamilyName(L"Segoe UI", &index, &found) < 0)
            return Err(TextError::Font);
        if (!found)
            return Err(TextError::Font);
        dw::Owned<dw::IDWriteFontFamily> matched;
        if (config.fonts->GetFontFamily(index, matched.put()) < 0 ||
            matched->GetFirstMatchingFont(weight, dw::DWRITE_FONT_STRETCH_NORMAL, style,
                                          font.put()) < 0)
            return Err(TextError::Font);
    }
    Pattern pattern;
    if (font) {
        dw::Owned<dw::IDWriteFontFace> face;
        if (font->CreateFontFace(face.put()) < 0 || dw::query(face.get(), pattern) < 0)
            return Err(TextError::Font);
    }
    if (config.matches.len() >= usize(64))
        config.matches.remove(usize());
    config.matches.push(CachedMatch{String::make(request.family), String::make(request.language),
                                    request.bold, request.italic, cp, dw::retain(pattern.get()),
                                    scale});
    return Ok(Match{rstd::move(pattern), scale});
}
struct Face {
    Option<Arc<Context>> context;
    Option<Arc<MemoryLoader>> loader;
    Pattern font;
    Scalar size{}, ascent{}, descent{}, height{}, units{};
    TextLimits limits;
    auto glyph(Index cp) -> Index {
        unsigned short result{};
        return dw::glyph_indices(font.get(), &cp, 1, &result) < 0 ? 0 : result;
    }
    auto measure(Index index) -> Result<GlyphMetrics, TextError> {
        if (index >= font->GetGlyphCount())
            return Err(TextError::InvalidIndex);
        auto glyph = static_cast<unsigned short>(index);
        dw::DWRITE_GLYPH_METRICS metrics{};
        if (font->GetDesignGlyphMetrics(&glyph, 1, &metrics, false) < 0)
            return Err(TextError::Font);
        Scalar factor = size / units;
        return Ok(GlyphMetrics{
            metrics.advanceWidth * factor,
            {metrics.leftSideBearing * factor,
             (metrics.topSideBearing - metrics.verticalOriginY) * factor,
             (Scalar(metrics.advanceWidth) - metrics.leftSideBearing - metrics.rightSideBearing) *
                 factor,
             (Scalar(metrics.advanceHeight) - metrics.topSideBearing - metrics.bottomSideBearing) *
                 factor}});
    }
    auto rasterize(Index index, Scalar scale) -> Result<GlyphBitmap, TextError> {
        if (index >= font->GetGlyphCount())
            return Err(TextError::InvalidIndex);
        Scalar em = size * scale;
        if (!finite(em) || em > 4294967294.0)
            return Err(TextError::Limit);
        auto glyph = static_cast<unsigned short>(index);
        dw::Owned<dw::IDWriteFontFace4> face4;
        if (dw::query(font.get(), face4) < 0)
            return Err(TextError::Unsupported);
        dw::DWRITE_GLYPH_IMAGE_FORMATS formats{};
        auto ppem = static_cast<unsigned>(__builtin_ceil(em));
        if (face4->GetGlyphImageFormats(glyph, ppem, ppem, &formats) < 0)
            return Err(TextError::Raster);
        if (formats != dw::DWRITE_GLYPH_IMAGE_FORMATS_NONE &&
            !(formats &
              (dw::DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE | dw::DWRITE_GLYPH_IMAGE_FORMATS_CFF)))
            return Err(TextError::Unsupported);
        float advance = 0;
        dw::DWRITE_GLYPH_OFFSET offset{};
        dw::DWRITE_GLYPH_RUN run{
            font.get(), static_cast<float>(em), 1, &glyph, &advance, &offset, false, 0};
        dw::Owned<dw::IDWriteGlyphRunAnalysis> analysis;
        if ((*context)->factory->CreateGlyphRunAnalysis(
                &run, nullptr, dw::DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
                dw::DWRITE_MEASURING_MODE_NATURAL, dw::DWRITE_GRID_FIT_MODE_DISABLED,
                dw::DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, 0, 0, analysis.put()) < 0)
            return Err(TextError::Raster);
        dw::RECT bounds{};
        if (analysis->GetAlphaTextureBounds(dw::DWRITE_TEXTURE_ALIASED_1x1, &bounds) < 0)
            return Err(TextError::Raster);
        auto width = static_cast<long long>(bounds.right) - bounds.left;
        auto height = static_cast<long long>(bounds.bottom) - bounds.top;
        if (width < 0 || height < 0)
            return Err(TextError::Raster);
        if (width > limits.bitmap_extent || height > limits.bitmap_extent ||
            static_cast<Serial>(width) * static_cast<Serial>(height) > limits.bitmap_bytes)
            return Err(TextError::Limit);
        if (!width || !height) {
            dw::Owned<dw::IDWriteColorGlyphRunEnumerator1> colors;
            auto color_formats = static_cast<dw::DWRITE_GLYPH_IMAGE_FORMATS>(
                dw::DWRITE_GLYPH_IMAGE_FORMATS_COLR | dw::DWRITE_GLYPH_IMAGE_FORMATS_SVG |
                dw::DWRITE_GLYPH_IMAGE_FORMATS_PNG | dw::DWRITE_GLYPH_IMAGE_FORMATS_JPEG |
                dw::DWRITE_GLYPH_IMAGE_FORMATS_TIFF |
                dw::DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8);
            auto color = (*context)->factory->TranslateColorGlyphRun(
                {0, 0}, &run, nullptr, color_formats, dw::DWRITE_MEASURING_MODE_NATURAL, nullptr, 0,
                colors.put());
            if (color >= 0)
                return Err(TextError::Unsupported);
            if (color != dw::no_color)
                return Err(TextError::Raster);
            auto metrics = measure(index);
            if (metrics.is_err())
                return Err(metrics.unwrap_err());
            if (metrics->ink.width > 0 && metrics->ink.height > 0)
                return Err(TextError::Unsupported);
            return Ok(GlyphBitmap{0, 0, {}, scale, {}});
        }
        GlyphBitmap result{static_cast<Index>(width),
                           static_cast<Index>(height),
                           {Scalar(bounds.left), Scalar(bounds.top)},
                           scale,
                           {}};
        result.coverage.resize(usize(width * height), 0);
        if (analysis->CreateAlphaTexture(dw::DWRITE_TEXTURE_ALIASED_1x1, &bounds,
                                         result.coverage.as_mut_slice().as_raw_ptr(),
                                         static_cast<unsigned>(width * height)) < 0)
            return Err(TextError::Raster);
        return Ok(rstd::move(result));
    }
};
auto initialize(Face &face, const SourceData &source, Scalar size, TextLimits limits)
    -> Result<empty, TextError> {
    face.context = Some(source.context->clone());
    if (source.loader.is_some())
        face.loader = Some(source.loader->clone());
    face.font = dw::retain(source.pattern.get());
    face.size = size;
    face.limits = limits;
    dw::DWRITE_FONT_METRICS metrics{};
    face.font->GetMetrics(&metrics);
    if (!metrics.designUnitsPerEm)
        return Err(TextError::Font);
    face.units = metrics.designUnitsPerEm;
    face.ascent = metrics.ascent * size / face.units;
    face.descent = metrics.descent * size / face.units;
    face.height = face.ascent + face.descent +
                  (metrics.lineGap > 0 ? metrics.lineGap * size / face.units : 0);
    return Ok(empty{});
}
} // namespace vectext::directwrite
