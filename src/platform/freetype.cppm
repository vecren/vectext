module vectext:platform.freetype;
import :font;
import :layout;
import :types;
import :platform.common;
import vectext.ffi.freetype;
using namespace rstd::prelude;
namespace vectext::freetype {
namespace ft = ffi::freetype;
struct VariationDescription {
    ft::FT_Library library;
    ft::FT_MM_Var *data{};
    explicit VariationDescription(ft::FT_Library library) : library(library) {}
    VariationDescription(const VariationDescription &) = delete;
    auto operator=(const VariationDescription &) -> VariationDescription & = delete;
    ~VariationDescription() { if (data) ft::FT_Done_MM_Var(library, data); }
    auto resolve(ft::FT_Face face) -> Result<Vec<ft::FT_Fixed>, TextError> {
        if (ft::FT_Get_MM_Var(face, &data) || !data)
            return Err(TextError::Font);
        if (data->num_axis > 4096) return Err(TextError::Limit);
        Vec<ft::FT_Fixed> values;
        values.resize(usize(data->num_axis), ft::FT_Fixed{});
        if (ft::FT_Get_Var_Design_Coordinates(face, data->num_axis, values.as_mut_slice().as_raw_ptr()))
            return Err(TextError::Font);
        return Ok(rstd::move(values));
    }
};

auto open_face(ft::Library &library, ft::Face &face, slice<u8> bytes, Index index)
    -> Result<empty, TextError> {
    if (ft::FT_Init_FreeType(library.put()) ||
        ft::FT_New_Memory_Face(library.get(), reinterpret_cast<const unsigned char *>(bytes.as_raw_ptr()),
                               bytes.len().to_primitive(), index, face.put()) ||
        !ft::is_scalable(face.get()) ||
        ft::FT_Select_Charmap(face.get(), ft::FT_ENCODING_UNICODE))
        return Err(TextError::Font);
    return Ok(empty{});
}

auto axes(slice<u8> bytes, Index index) -> Result<Vec<FontAxis>, TextError> {
    ft::Library library;
    ft::Face face;
    auto opened = open_face(library, face, bytes, index);
    if (opened.is_err()) return Err(opened.unwrap_err());
    Vec<FontAxis> result;
    if (!ft::has_variations(face.get())) return Ok(rstd::move(result));
    VariationDescription variation(library.get());
    auto defaults = variation.resolve(face.get());
    if (defaults.is_err()) return Err(defaults.unwrap_err());
    for (unsigned i{}; i < variation.data->num_axis; ++i) {
        const auto &axis = variation.data->axis[i];
        result.push({Index(axis.tag), axis.minimum / 65536.0, (*defaults)[usize(i)] / 65536.0,
                     axis.maximum / 65536.0});
    }
    return Ok(rstd::move(result));
}

auto apply_variations(ft::FT_Library library, ft::FT_Face face, slice<FontVariation> requested)
    -> Result<empty, TextError> {
    if (requested.is_empty()) return Ok(empty{});
    VariationDescription variation(library);
    auto loaded = variation.resolve(face);
    if (loaded.is_err()) return Err(loaded.unwrap_err());
    auto values = rstd::move(loaded).unwrap();
    for (unsigned i{}; i < variation.data->num_axis; ++i) {
        const auto &axis = variation.data->axis[i];
        for (auto coordinate : requested) {
            if (coordinate.tag != axis.tag) continue;
            const auto fixed = coordinate.value * 65536;
            if (!finite(fixed) || fixed < Scalar(axis.minimum) || fixed > Scalar(axis.maximum))
                return Err(TextError::Font);
            values[usize(i)] = static_cast<ft::FT_Fixed>(fixed);
        }
    }
    if (ft::FT_Set_Var_Design_Coordinates(face, variation.data->num_axis, values.as_mut_slice().as_raw_ptr()))
        return Err(TextError::Font);
    return Ok(empty{});
}

auto outline_point(const ft::FT_Vector *point) -> Point { return {point->x / 64.0, -(point->y / 64.0)}; }
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
    auto initialize(slice<u8> bytes, Index index, Scalar pixel_size, TextLimits budget,
                    slice<FontVariation> variations)
        -> Result<empty, TextError> {
        size = pixel_size;
        limits = budget;
        auto opened = open_face(library, face, bytes, index);
        if (opened.is_err()) return opened;
        auto varied = apply_variations(library.get(), face.get(), variations);
        if (varied.is_err()) return varied;
        auto status = set_size(1);
        if (status.is_err())
            return status;
        return refresh_metrics();
    }
    auto refresh_metrics() -> Result<empty, TextError> {
        ascent = face->size->metrics.ascender / 64.0;
        descent = -face->size->metrics.descender / 64.0;
        height = face->size->metrics.height / 64.0;
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
    auto loaded_glyph_supported() const -> bool {
        if (face->glyph->format != ft::FT_GLYPH_FORMAT_OUTLINE)
            return false;
        if (face->glyph->outline.n_points)
            return true;
        ft::FT_LayerIterator iterator{};
        ft::FT_UInt layer{}, color{};
        ft::FT_OpaquePaint paint{};
        return !ft::FT_Get_Color_Glyph_Layer(face.get(), face->glyph->glyph_index, &layer, &color,
                                            &iterator) &&
               !ft::FT_Get_Color_Glyph_Paint(face.get(), face->glyph->glyph_index,
                                            ft::FT_COLOR_NO_ROOT_TRANSFORM, &paint);
    }
    auto outline(Index glyph) -> Result<GlyphOutline, TextError> {
        auto native = face.get();
        if (glyph >= static_cast<Serial>(native->num_glyphs)) return Err(TextError::InvalidIndex);
        auto status = set_size(1);
        if (status.is_err()) return Err(status.unwrap_err());
        if (ft::FT_Load_Glyph(native, glyph, ft::FT_LOAD_NO_BITMAP | ft::FT_LOAD_NO_HINTING))
            return Err(TextError::Font);
        if (!loaded_glyph_supported()) return Err(TextError::Unsupported);
        GlyphOutlineBuilder builder;
        const ft::FT_Outline_Funcs callbacks{
            [](const ft::FT_Vector *to, void *data) -> int {
                static_cast<GlyphOutlineBuilder *>(data)->move(outline_point(to));
                return 0;
            },
            [](const ft::FT_Vector *to, void *data) -> int {
                static_cast<GlyphOutlineBuilder *>(data)->line(outline_point(to));
                return 0;
            },
            [](const ft::FT_Vector *control, const ft::FT_Vector *to, void *data) -> int {
                static_cast<GlyphOutlineBuilder *>(data)->quadratic(outline_point(control), outline_point(to));
                return 0;
            },
            [](const ft::FT_Vector *first, const ft::FT_Vector *second, const ft::FT_Vector *to, void *data) -> int {
                static_cast<GlyphOutlineBuilder *>(data)->cubic(outline_point(first), outline_point(second),
                                                               outline_point(to));
                return 0;
            }, 0, 0};
        if (ft::FT_Outline_Decompose(&native->glyph->outline, &callbacks, &builder))
            return Err(TextError::Font);
        return builder.finish();
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
        if (!loaded_glyph_supported())
            return Err(TextError::Unsupported);
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
} // namespace vectext::freetype
