module vectext:detail.raster;
import :detail.state;
import :layout;
import :types;
import :font;
import vectext.ffi.freetype;
using namespace rstd::prelude;
namespace vectext::detail {
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
struct Rasterizer final : FontRasterizer {
    Arc<Font> font;
    explicit Rasterizer(Arc<Font> font) : font(rstd::move(font)) {}
    auto rasterize(Index glyph, Scalar scale) const -> Result<GlyphBitmap, TextError> override {
        auto guard = font->mutex.lock().unwrap();
        auto face = font->face.get();
        if (glyph >= static_cast<Serial>(face->num_glyphs))
            return Err(TextError::InvalidIndex);
        auto size = font->set_size(scale);
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
        if (metrics.width < 0 || metrics.height < 0 ||
            width > font->limits.bitmap_extent + Serial(2) ||
            height > font->limits.bitmap_extent + Serial(2) ||
            (width > 2 && height > 2 && (width - 2) * (height - 2) > font->limits.bitmap_bytes))
            return Err(TextError::Limit);
        if (ft::FT_Render_Glyph(face->glyph, ft::FT_RENDER_MODE_NORMAL))
            return Err(TextError::Raster);
        auto coverage = bitmap_coverage(face->glyph->bitmap, font->limits);
        if (coverage.is_err())
            return Err(coverage.unwrap_err());
        return Ok(GlyphBitmap{face->glyph->bitmap.width,
                              face->glyph->bitmap.rows,
                              {Scalar(face->glyph->bitmap_left), -Scalar(face->glyph->bitmap_top)},
                              scale,
                              rstd::move(coverage).unwrap()});
    }
};
} // namespace vectext::detail
