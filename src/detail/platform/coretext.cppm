module vectext:detail.platform.coretext;
import :font;
import :layout;
import :detail.platform.common;
import vectext.ffi.owner;
import vectext.ffi.corefoundation;
import vectext.ffi.coretext;
import vectext.ffi.coregraphics;
using namespace rstd::prelude;
using namespace rstd::literals;

namespace vectext::detail::coretext {
namespace cf = ffi::corefoundation;
namespace ct = ffi::coretext;
namespace cg = ffi::coregraphics;
template <class T> using Owned = ffi::Owner<rstd::mtp::rm_ptr<T>, cf::CFRelease>;
using Pattern = Owned<ct::CTFontDescriptorRef>;
using NativeFont = Owned<ct::CTFontRef>;

auto cf_string(ref<str> value) -> Owned<cf::CFStringRef> {
    return Owned<cf::CFStringRef>(cf::CFStringCreateWithBytes(
        cf::kCFAllocatorDefault,
        reinterpret_cast<const unsigned char *>(value->as_bytes().as_raw_ptr()),
        value->len().to_primitive(), cf::kCFStringEncodingUTF8, false));
}
auto string(cf::CFStringRef value) -> String {
    if (!value)
        return String();
    auto length = cf::CFStringGetMaximumSizeForEncoding(cf::CFStringGetLength(value),
                                                        cf::kCFStringEncodingUTF8);
    if (length < 0)
        return String();
    Vec<char> bytes;
    bytes.resize(usize(length + 1), '\0');
    if (!cf::CFStringGetCString(value, bytes.as_mut_slice().as_raw_ptr(), length + 1,
                                cf::kCFStringEncodingUTF8))
        return String();
    return String::make(string_view(bytes.as_slice().as_raw_ptr()));
}
auto retain(ct::CTFontDescriptorRef value) -> Pattern {
    return Pattern(static_cast<ct::CTFontDescriptorRef>(cf::CFRetain(value)));
}
struct SourceData {
    Vec<u8> bytes;
    Index index{};
    String name, family;
    unsigned traits{};
    Owned<cf::CFDataRef> data;
    Pattern pattern;
};
auto source_metadata(SourceData &source) -> Result<empty, TextError> {
    NativeFont font(ct::CTFontCreateWithFontDescriptor(source.pattern.get(), 16, nullptr));
    if (!font)
        return Err(TextError::Font);
    Owned<cf::CFStringRef> name(ct::CTFontCopyFullName(font.get()));
    Owned<cf::CFStringRef> family(ct::CTFontCopyFamilyName(font.get()));
    source.name = string(name.get());
    source.family = string(family.get());
    source.traits = ct::CTFontGetSymbolicTraits(font.get());
    return Ok(empty{});
}
auto parse_source(Vec<u8> bytes, Index index, ref<str>) -> Result<SourceData, TextError> {
    SourceData source;
    source.bytes = rstd::move(bytes);
    source.index = index;
    // The descriptor and CFData are destroyed before the backing bytes.
    source.data.reset(cf::CFDataCreateWithBytesNoCopy(
        cf::kCFAllocatorDefault,
        reinterpret_cast<const unsigned char *>(source.bytes.as_slice().as_raw_ptr()),
        source.bytes.len().to_primitive(), cf::kCFAllocatorNull));
    if (!source.data)
        return Err(TextError::Font);
    Owned<cf::CFArrayRef> descriptors(
        ct::CTFontManagerCreateFontDescriptorsFromData(source.data.get()));
    if (!descriptors || index >= cf::CFArrayGetCount(descriptors.get()))
        return Err(TextError::Font);
    source.pattern = retain(
        static_cast<ct::CTFontDescriptorRef>(cf::CFArrayGetValueAtIndex(descriptors.get(), index)));
    auto status = source_metadata(source);
    if (status.is_err())
        return Err(status.unwrap_err());
    return Ok(rstd::move(source));
}
struct CachedMatch {
    String family, language;
    bool bold{}, italic{};
    Vec<Pattern> patterns;
};
struct Config {
    bool system{};
    Vec<CachedMatch> matches;
};
auto make_config(bool system, ref<str> path) -> Result<Config, TextError> {
    if (!path->is_empty())
        return Err(TextError::Unsupported);
    return Ok(Config{system, {}});
}
auto generic_family(ref<str> family) -> ref<str> {
    if (family->is_empty() || family == "sans-serif"_str)
        return "Helvetica"_str;
    if (family == "serif"_str)
        return "Times"_str;
    if (family == "monospace"_str)
        return "Menlo"_str;
    return family;
}
auto system_patterns(Config &config, TextRequest request) -> Result<Vec<Pattern>, TextError> {
    for (auto &cached : config.matches) {
        if (cached.family.as_str() != request.family ||
            cached.language.as_str() != request.language || cached.bold != request.bold ||
            cached.italic != request.italic)
            continue;
        Vec<Pattern> result;
        for (auto &pattern : cached.patterns)
            result.push(retain(pattern.get()));
        return Ok(rstd::move(result));
    }
    auto family = cf_string(generic_family(request.family));
    int traits =
        (request.bold ? ct::kCTFontBoldTrait : 0) | (request.italic ? ct::kCTFontItalicTrait : 0);
    Owned<cf::CFNumberRef> number(
        cf::CFNumberCreate(cf::kCFAllocatorDefault, cf::kCFNumberSInt32Type, &traits));
    if (!family || !number)
        return Err(TextError::Font);
    const void *trait_keys[]{ct::kCTFontSymbolicTrait}, *trait_values[]{number.get()};
    Owned<cf::CFDictionaryRef> trait_dict(cf::CFDictionaryCreate(
        cf::kCFAllocatorDefault, trait_keys, trait_values, 1, &cf::kCFTypeDictionaryKeyCallBacks,
        &cf::kCFTypeDictionaryValueCallBacks));
    const void *keys[]{ct::kCTFontFamilyNameAttribute, ct::kCTFontTraitsAttribute},
        *values[]{family.get(), trait_dict.get()};
    if (!trait_dict)
        return Err(TextError::Font);
    Owned<cf::CFDictionaryRef> attributes(cf::CFDictionaryCreate(
        cf::kCFAllocatorDefault, keys, values, 2, &cf::kCFTypeDictionaryKeyCallBacks,
        &cf::kCFTypeDictionaryValueCallBacks));
    if (!attributes)
        return Err(TextError::Font);
    Pattern descriptor(ct::CTFontDescriptorCreateWithAttributes(attributes.get()));
    if (!descriptor)
        return Err(TextError::Font);
    NativeFont font(ct::CTFontCreateWithFontDescriptor(descriptor.get(), 16, nullptr));
    if (!font)
        return Err(TextError::Font);
    auto language = cf_string(request.language);
    const void *languages[]{language.get()};
    Owned<cf::CFArrayRef> preferences;
    if (!request.language->is_empty()) {
        if (!language)
            return Err(TextError::Font);
        preferences.reset(
            cf::CFArrayCreate(cf::kCFAllocatorDefault, languages, 1, &cf::kCFTypeArrayCallBacks));
        if (!preferences)
            return Err(TextError::Font);
    }
    Owned<cf::CFArrayRef> cascade(
        ct::CTFontCopyDefaultCascadeListForLanguages(font.get(), preferences.get()));
    Vec<Pattern> patterns;
    Pattern primary(ct::CTFontCopyFontDescriptor(font.get()));
    if (!primary)
        return Err(TextError::Font);
    patterns.push(rstd::move(primary));
    if (cascade) {
        for (cf::CFIndex i = 0;
             i < cf::CFArrayGetCount(cascade.get()) && patterns.len() < usize(4096); ++i) {
            auto next =
                static_cast<ct::CTFontDescriptorRef>(cf::CFArrayGetValueAtIndex(cascade.get(), i));
            bool duplicate{};
            for (auto &entry : patterns)
                duplicate |= cf::CFEqual(entry.get(), next);
            if (!duplicate)
                patterns.push(retain(next));
        }
    }
    Vec<Pattern> result;
    for (auto &pattern : patterns)
        result.push(retain(pattern.get()));
    // Bound metadata retained independently of the font instance cache.
    if (config.matches.len() >= usize(64))
        config.matches.remove(usize());
    config.matches.push(CachedMatch{String::make(request.family), String::make(request.language),
                                    request.bold, request.italic, rstd::move(patterns)});
    return Ok(rstd::move(result));
}
struct Face {
    NativeFont font;
    Scalar size{}, ascent{}, descent{}, height{};
    TextLimits limits;
    auto glyph(Index codepoint) -> Index {
        cf::UniChar chars[2]{};
        cg::CGGlyph glyphs[2]{};
        cf::CFIndex count = 1;
        if (codepoint <= 0xffff)
            chars[0] = static_cast<cf::UniChar>(codepoint);
        else {
            chars[0] = static_cast<cf::UniChar>(0xd800 + ((codepoint - 0x10000) >> 10));
            chars[1] = static_cast<cf::UniChar>(0xdc00 + ((codepoint - 0x10000) & 0x3ff));
            count = 2;
        }
        ct::CTFontGetGlyphsForCharacters(font.get(), chars, glyphs, count);
        return glyphs[0];
    }
    auto measure(Index index) -> Result<GlyphMetrics, TextError> {
        if (index >= ct::CTFontGetGlyphCount(font.get()))
            return Err(TextError::InvalidIndex);
        auto glyph = static_cast<cg::CGGlyph>(index);
        cg::CGSize advance{};
        ct::CTFontGetAdvancesForGlyphs(font.get(), ct::kCTFontOrientationHorizontal, &glyph,
                                       &advance, 1);
        auto bounds = ct::CTFontGetBoundingRectsForGlyphs(
            font.get(), ct::kCTFontOrientationHorizontal, &glyph, nullptr, 1);
        return Ok(GlyphMetrics{advance.width,
                               {bounds.origin.x, -bounds.origin.y - bounds.size.height,
                                bounds.size.width, bounds.size.height}});
    }
    auto rasterize(Index index, Scalar scale) -> Result<GlyphBitmap, TextError> {
        if (index >= ct::CTFontGetGlyphCount(font.get()))
            return Err(TextError::InvalidIndex);
        auto transform = cg::CGAffineTransformMakeScale(scale, scale);
        ffi::Owner<rstd::mtp::rm_ptr<cg::CGPathRef>, cg::CGPathRelease> path(
            ct::CTFontCreatePathForGlyph(font.get(), static_cast<cg::CGGlyph>(index), &transform));
        if (!path || cg::CGPathIsEmpty(path.get())) {
            const unsigned color_tables[]{0x434f4c52, 0x43424454, 0x73626978, 0x53564720};
            for (auto table : color_tables) {
                Owned<cf::CFDataRef> data(
                    ct::CTFontCopyTable(font.get(), table, ct::kCTFontTableOptionNoOptions));
                if (data)
                    return Err(TextError::Unsupported);
            }
            auto metrics = measure(index);
            if (metrics.is_err())
                return Err(metrics.unwrap_err());
            if (metrics->ink.width > 0 && metrics->ink.height > 0)
                return Err(TextError::Unsupported);
            return Ok(GlyphBitmap{0, 0, {}, scale, {}});
        }
        auto bounds = cg::CGPathGetPathBoundingBox(path.get());
        Scalar left = __builtin_floor(bounds.origin.x), bottom = __builtin_floor(bounds.origin.y);
        Scalar top = __builtin_ceil(bounds.origin.y + bounds.size.height);
        Scalar width = __builtin_ceil(bounds.origin.x + bounds.size.width) - left,
               height = top - bottom;
        if (!finite(left) || !finite(bottom) || !finite(top) || !finite(width) || !finite(height) ||
            width < 0 || height < 0)
            return Err(TextError::Raster);
        if (width > limits.bitmap_extent || height > limits.bitmap_extent ||
            width * height > limits.bitmap_bytes)
            return Err(TextError::Limit);
        GlyphBitmap result{
            static_cast<Index>(width), static_cast<Index>(height), {left, -top}, scale, {}};
        if (!result.width || !result.height)
            return Ok(rstd::move(result));
        result.coverage.resize(usize(result.width) * usize(result.height), 0);
        ffi::Owner<rstd::mtp::rm_ptr<cg::CGColorSpaceRef>, cg::CGColorSpaceRelease> colors(
            cg::CGColorSpaceCreateDeviceGray());
        if (!colors)
            return Err(TextError::Raster);
        ffi::Owner<rstd::mtp::rm_ptr<cg::CGContextRef>, cg::CGContextRelease> context(
            cg::CGBitmapContextCreate(result.coverage.as_mut_slice().as_raw_ptr(), result.width,
                                      result.height, 8, result.width, colors.get(),
                                      cg::kCGImageAlphaNone));
        if (!context)
            return Err(TextError::Raster);
        cg::CGContextSetAllowsAntialiasing(context.get(), true);
        cg::CGContextSetShouldAntialias(context.get(), true);
        cg::CGContextSetGrayFillColor(context.get(), 1, 1);
        cg::CGContextTranslateCTM(context.get(), -left, -bottom);
        cg::CGContextAddPath(context.get(), path.get());
        cg::CGContextFillPath(context.get());
        return Ok(rstd::move(result));
    }
};
auto initialize(Face &face, const SourceData &source, Scalar size, TextLimits limits)
    -> Result<empty, TextError> {
    face.font.reset(ct::CTFontCreateWithFontDescriptor(source.pattern.get(), size, nullptr));
    if (!face.font)
        return Err(TextError::Font);
    face.size = size;
    face.limits = limits;
    face.ascent = ct::CTFontGetAscent(face.font.get());
    face.descent = ct::CTFontGetDescent(face.font.get());
    face.height = face.ascent + face.descent + ct::CTFontGetLeading(face.font.get());
    if (face.height < face.ascent + face.descent)
        face.height = face.ascent + face.descent;
    return Ok(empty{});
}
} // namespace vectext::detail::coretext
