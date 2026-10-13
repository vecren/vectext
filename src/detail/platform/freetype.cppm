module vectext:detail.platform.freetype;
import :font;
import :layout;
import :types;
import :detail.platform.common;
import vectext.ffi.freetype;
using namespace rstd::prelude;
namespace vectext::detail::freetype {
namespace ft = ffi::freetype;
auto bitmap_coverage(const ft::FT_Bitmap &bitmap, TextLimits limits)
    -> Result<Vec<unsigned char>, TextError> {
    if (bitmap.width > limits.bitmap_extent || bitmap.rows > limits.bitmap_extent ||
        Serial(bitmap.width) * bitmap.rows > limits.bitmap_bytes)
        return Err(TextError::Limit);
    Vec<unsigned char> pixels;
    if (!bitmap.width || !bitmap.rows)
        return Ok(rstd::move(pixels));
    if (bitmap.pixel_mode != ft::FT_PIXEL_MODE_GRAY && bitmap.pixel_mode != ft::FT_PIXEL_MODE_MONO)
        return Err(TextError::Unsupported);
    auto pitch = bitmap.pitch < 0 ? -static_cast<rstd::int64_t>(bitmap.pitch) : bitmap.pitch;
    auto row_bytes =
        bitmap.pixel_mode == ft::FT_PIXEL_MODE_MONO ? (Serial(bitmap.width) + 7) / 8 : bitmap.width;
    if (!bitmap.buffer || static_cast<Serial>(pitch) < row_bytes ||
        (bitmap.pixel_mode == ft::FT_PIXEL_MODE_GRAY &&
         (bitmap.num_grays < 2 || bitmap.num_grays > 256)))
        return Err(TextError::Raster);
    pixels.reserve(usize(bitmap.width) * usize(bitmap.rows));
    for (Index y = 0; y < bitmap.rows; ++y) {
        auto row = bitmap.buffer + (bitmap.pitch < 0 ? bitmap.rows - 1 - y : y) * pitch;
        for (Index x = 0; x < bitmap.width; ++x) {
            unsigned value;
            if (bitmap.pixel_mode == ft::FT_PIXEL_MODE_MONO)
                value = (row[x / 8] & (0x80 >> (x % 8))) ? 255 : 0;
            else {
                if (row[x] >= bitmap.num_grays)
                    return Err(TextError::Raster);
                value =
                    (unsigned(row[x]) * 255 + (bitmap.num_grays - 1) / 2) / (bitmap.num_grays - 1);
            }
            pixels.push(static_cast<unsigned char>(value));
        }
    }
    return Ok(rstd::move(pixels));
}
struct Face {
    ft::Library library;
    ft::Face face;
    Scalar size{}, ascent{}, descent{}, height{};
    TextLimits limits;
    auto initialize(slice<u8> bytes, Index index, Scalar pixel_size, TextLimits budget)
        -> Result<empty, TextError> {
        size = pixel_size;
        limits = budget;
        if (ft::FT_Init_FreeType(library.put()) ||
            ft::FT_New_Memory_Face(library.get(),
                                   reinterpret_cast<const unsigned char *>(bytes.as_raw_ptr()),
                                   bytes.len().to_primitive(), index, face.put()) ||
            !ft::is_scalable(face.get()) ||
            ft::FT_Select_Charmap(face.get(), ft::FT_ENCODING_UNICODE))
            return Err(TextError::Font);
        auto status = set_size(1);
        if (status.is_err())
            return status;
        ascent = face->size->metrics.ascender / 64.0;
        descent = -face->size->metrics.descender / 64.0;
        height = face->size->metrics.height / 64.0;
        if (height < ascent + descent)
            height = ascent + descent;
        if (height <= 0)
            return Err(TextError::Font);
        return Ok(empty{});
    }
    auto set_size(Scalar scale) -> Result<empty, TextError> {
        if (ft::FT_Set_Char_Size(face.get(), 0, static_cast<ft::FT_F26Dot6>(size * scale * 64 + .5),
                                 72, 72))
            return Err(TextError::Font);
        return Ok(empty{});
    }

    auto glyph(Index codepoint) -> Index { return ft::FT_Get_Char_Index(face.get(), codepoint); }
    auto measure(Index glyph) -> Result<GlyphMetrics, TextError> {
        auto status = set_size(1);
        if (status.is_err())
            return Err(status.unwrap_err());
        if (ft::FT_Load_Glyph(face.get(), glyph, ft::FT_LOAD_NO_HINTING | ft::FT_LOAD_NO_BITMAP))
            return Err(TextError::Font);
        auto &m = face->glyph->metrics;
        return Ok(GlyphMetrics{
            face->glyph->advance.x / 64.0,
            {m.horiBearingX / 64.0, -m.horiBearingY / 64.0, m.width / 64.0, m.height / 64.0}});
    }
    auto rasterize(Index glyph, Scalar scale) -> Result<GlyphBitmap, TextError> {
        auto face = this->face.get();
        if (glyph >= static_cast<Serial>(face->num_glyphs))
            return Err(TextError::InvalidIndex);
        auto size = set_size(scale);
        if (size.is_err())
            return Err(size.unwrap_err());
        if (ft::FT_Load_Glyph(face, glyph, ft::FT_LOAD_NO_BITMAP | ft::FT_LOAD_NO_HINTING))
            return Err(TextError::Raster);
        if (face->glyph->format != ft::FT_GLYPH_FORMAT_OUTLINE)
            return Err(TextError::Unsupported);
        if (!face->glyph->outline.n_points) {
            ft::FT_LayerIterator iterator{};
            ft::FT_UInt layer{}, color{};
            ft::FT_OpaquePaint paint{};
            if (ft::FT_Get_Color_Glyph_Layer(face, glyph, &layer, &color, &iterator) ||
                ft::FT_Get_Color_Glyph_Paint(face, glyph, ft::FT_COLOR_NO_ROOT_TRANSFORM, &paint))
                return Err(TextError::Unsupported);
        }
        const auto &metrics = face->glyph->metrics;
        auto width = static_cast<Serial>((metrics.width + 63) / 64 + 2);
        auto height = static_cast<Serial>((metrics.height + 63) / 64 + 2);
        if (metrics.width < 0 || metrics.height < 0 || width > limits.bitmap_extent + Serial(2) ||
            height > limits.bitmap_extent + Serial(2) ||
            (width > 2 && height > 2 && (width - 2) * (height - 2) > limits.bitmap_bytes))
            return Err(TextError::Limit);
        if (ft::FT_Render_Glyph(face->glyph, ft::FT_RENDER_MODE_NORMAL))
            return Err(TextError::Raster);
        auto coverage = bitmap_coverage(face->glyph->bitmap, limits);
        if (coverage.is_err())
            return Err(coverage.unwrap_err());
        return Ok(GlyphBitmap{face->glyph->bitmap.width,
                              face->glyph->bitmap.rows,
                              {Scalar(face->glyph->bitmap_left), -Scalar(face->glyph->bitmap_top)},
                              scale,
                              rstd::move(coverage).unwrap()});
    }
};
} // namespace vectext::detail::freetype
