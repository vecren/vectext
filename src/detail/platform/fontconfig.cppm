module vectext:detail.platform.fontconfig;
import :font;
import :detail.platform.common;
import :detail.platform.freetype;
import vectext.ffi.fontconfig;
import vectext.ffi.freetype;
using namespace rstd::prelude;
namespace vectext::detail::fontconfig {
namespace fc = ffi::fontconfig;
namespace ft = ffi::freetype;
using Config = fc::Config;
using Pattern = fc::Pattern;
using Face = freetype::Face;
struct SourceData {
    Vec<u8> bytes;
    Index index{};
    String name;
    Pattern pattern;
};
auto make_config(bool system, ref<str> path_text) -> Result<Config, TextError> {
    fc::Config result;
    if (path_text->is_empty())
        result.reset(system ? fc::FcInitLoadConfigAndFonts() : fc::FcConfigCreate());
    else {
        result.reset(fc::FcConfigCreate());
        auto path = terminated(path_text);
        if (!result ||
            !fc::FcConfigParseAndLoad(
                result.get(), reinterpret_cast<const fc::FcChar8 *>(path.as_slice().as_raw_ptr()),
                fc::FcTrue) ||
            !fc::FcConfigBuildFonts(result.get()))
            return Err(TextError::Font);
    }
    if (!result)
        return Err(TextError::Font);
    return Ok(rstd::move(result));
}

auto parse_source(Config &, Vec<u8> bytes, Index index, ref<str> path)
    -> Result<SourceData, TextError> {
    ft::Library library;
    ft::Face face;
    if (ft::FT_Init_FreeType(library.put()) ||
        ft::FT_New_Memory_Face(
            library.get(), reinterpret_cast<const unsigned char *>(bytes.as_slice().as_raw_ptr()),
            bytes.len().to_primitive(), index, face.put()) ||
        !ft::is_scalable(face.get()) || ft::FT_Select_Charmap(face.get(), ft::FT_ENCODING_UNICODE))
        return Err(TextError::Font);
    auto name = String::make(string_view(face->family_name ? face->family_name : "Unknown"));
    if (face->style_name) {
        name.push_ascii(' ');
        name.push_str(string_view(face->style_name));
    }
    auto filename = terminated(path);
    Pattern pattern(fc::FcFreeTypeQueryFace(
        face.get(), reinterpret_cast<const fc::FcChar8 *>(filename.as_slice().as_raw_ptr()), index,
        nullptr));
    if (!pattern)
        return Err(TextError::Font);
    return Ok(SourceData{rstd::move(bytes), index, rstd::move(name), rstd::move(pattern)});
}
auto source_bytes(const SourceData &source) -> Serial { return source.bytes.len().to_primitive(); }
auto initialize(Face &face, const SourceData &source, Scalar size, TextLimits limits)
    -> Result<empty, TextError> {
    return face.initialize(source.bytes.as_slice(), source.index, size, limits);
}
} // namespace vectext::detail::fontconfig
