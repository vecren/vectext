module vectext;
import :platform.android;
import :ffi.android;
import vectext.ffi.freetype;

namespace vectext::android {
using namespace rstd::prelude;
namespace ft = vectext::ffi::freetype;

auto make_config(bool system, ref<str> path) -> Result<Config, TextError> {
    if (!path->is_empty())
        return Err(TextError::Unsupported);
    return Ok(Config{system});
}

auto copy_pattern(const Pattern &pattern) -> Pattern {
    Pattern result{String::make(pattern.path.as_str()), pattern.index, pattern.weight,
                   pattern.italic};
    for (auto axis : pattern.axes)
        result.axes.push(Axis(axis));
    return result;
}

auto same_pattern(const Pattern &a, const Pattern &b) -> bool {
    if (a.path.as_str() != b.path.as_str() || a.index != b.index || a.axes.len() != b.axes.len())
        return false;
    for (usize i{}; i < a.axes.len(); ++i)
        if (a.axes[i] != b.axes[i])
            return false;
    return true;
}

auto parse_source(Config &, Vec<u8> bytes, Index index, ref<str> path)
    -> Result<SourceData, TextError> {
    ft::Library library;
    ft::Face face;
    if (ft::FT_Init_FreeType(library.put()) ||
        ft::FT_New_Memory_Face(
            library.get(), reinterpret_cast<const unsigned char *>(bytes.as_slice().as_raw_ptr()),
            bytes.len().to_primitive(), index, face.put()) ||
        ft::FT_Select_Charmap(face.get(), ft::FT_ENCODING_UNICODE))
        return Err(TextError::Font);
    if (!ft::is_scalable(face.get()))
        return Err(TextError::Unsupported);
    auto family = String::make(string_view(face->family_name ? face->family_name : "Unknown"));
    auto name = String::make(family.as_str());
    if (face->style_name) {
        name.push_ascii(' ');
        name.push_str(string_view(face->style_name));
    }
    auto bold = (face->style_flags & ft::FT_STYLE_FLAG_BOLD) != 0;
    auto italic = (face->style_flags & ft::FT_STYLE_FLAG_ITALIC) != 0;
    Pattern pattern{String::make(path), index, rstd::uint16_t(bold ? 700 : 400), italic};
    return Ok(SourceData{rstd::move(bytes), index, rstd::move(name), rstd::move(family), bold,
                         italic, rstd::move(pattern)});
}

auto system_match(const Config &config, TextRequest request, Index cp)
    -> Result<Pattern, TextError> {
    if (!config.system || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
        return Err(TextError::InvalidValue);
    auto family = terminated(request.family);
    auto language = terminated(request.language);
    ::ffi::Matcher matcher(::ffi::AFontMatcher_create());
    if (!matcher)
        return Err(TextError::Font);
    ::ffi::AFontMatcher_setStyle(matcher.get(), request.bold ? 700 : 400, request.italic);
    ::ffi::AFontMatcher_setLocales(matcher.get(), language.as_slice().as_raw_ptr());
    rstd::uint16_t text[2]{};
    unsigned count = 1;
    if (cp < 0x10000)
        text[0] = rstd::uint16_t(cp);
    else {
        auto scalar = cp - 0x10000;
        text[0] = rstd::uint16_t(0xd800 + (scalar >> 10));
        text[1] = rstd::uint16_t(0xdc00 + (scalar & 0x3ff));
        count = 2;
    }
    ::ffi::Font font(::ffi::AFontMatcher_match(matcher.get(), family.as_slice().as_raw_ptr(), text,
                                               count, nullptr));
    if (!font)
        return Err(TextError::Font);
    auto raw_path = ::ffi::AFont_getFontFilePath(font.get());
    auto index = ::ffi::AFont_getCollectionIndex(font.get());
    if (!raw_path || index > ~Index{})
        return Err(TextError::Font);
    Pattern result{String::make(string_view(raw_path)), Index(index),
                   ::ffi::AFont_getWeight(font.get()), ::ffi::AFont_isItalic(font.get())};
    auto axes = ::ffi::AFont_getAxisCount(font.get());
    if (axes > 4096)
        return Err(TextError::Limit);
    for (rstd::uint32_t i{}; i < axes; ++i) {
        auto value = Scalar(::ffi::AFont_getAxisValue(font.get(), i));
        if (!finite(value))
            return Err(TextError::Font);
        result.axes.push({::ffi::AFont_getAxisTag(font.get(), i), value});
    }
    return Ok(rstd::move(result));
}

auto source_bytes(const SourceData &source) -> Serial { return source.bytes.len().to_primitive(); }
auto source_bold(const SourceData &source) -> bool {
    constexpr Index weight_axis =
        (Index('w') << 24) | (Index('g') << 16) | (Index('h') << 8) | Index('t');
    for (auto axis : source.pattern.axes)
        if (axis.tag == weight_axis)
            return axis.value >= 600;
    return source.bold || source.pattern.weight >= 600;
}

auto initialize(Face &face, const SourceData &source, Scalar size, TextLimits limits)
    -> Result<empty, TextError> {
    auto loaded = face.initialize(source.bytes.as_slice(), source.index, size, limits);
    if (loaded.is_err() || source.pattern.axes.is_empty())
        return loaded;
    ft::FT_MM_Var *description{};
    if (ft::FT_Get_MM_Var(face.face.get(), &description) || !description)
        return Err(TextError::Font);
    struct AxesOwner {
        ft::FT_Library library;
        ft::FT_MM_Var *description;
        ~AxesOwner() { ft::FT_Done_MM_Var(library, description); }
    } owner{face.library.get(), description};
    if (description->num_axis > 4096)
        return Err(TextError::Limit);
    Vec<ft::FT_Fixed> values;
    for (unsigned i{}; i < description->num_axis; ++i) {
        auto value = description->axis[i].def;
        for (auto axis : source.pattern.axes) {
            if (axis.tag != description->axis[i].tag)
                continue;
            auto fixed = axis.value * 65536;
            if (!finite(fixed) || fixed < Scalar(description->axis[i].minimum) ||
                fixed > Scalar(description->axis[i].maximum))
                return Err(TextError::Font);
            value = static_cast<ft::FT_Fixed>(fixed);
        }
        values.push(ft::FT_Fixed(value));
    }
    if (ft::FT_Set_Var_Design_Coordinates(face.face.get(), values.len().to_primitive(),
                                          values.as_mut_slice().as_raw_ptr()))
        return Err(TextError::Font);
    auto scaled = face.set_size(1);
    if (scaled.is_err())
        return scaled;
    return face.refresh_metrics();
}
} // namespace vectext::android
