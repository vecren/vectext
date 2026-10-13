module vectext:font.raster;
import :font.state;
import :layout;
using namespace rstd::prelude;
namespace vectext {
struct Rasterizer final : FontRasterizer {
    Arc<Font> font;
    explicit Rasterizer(Arc<Font> font) : font(rstd::move(font)) {}
    auto rasterize(Index glyph, Scalar scale) const -> Result<GlyphBitmap, TextError> override {
        return font->rasterize(glyph, scale);
    }
};
} // namespace vectext
