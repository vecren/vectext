module;
#include <climits>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_COLOR_H
namespace vectext_freetype_constants {
inline constexpr auto k_FT_LOAD_NO_BITMAP = FT_LOAD_NO_BITMAP;
inline constexpr auto k_FT_LOAD_NO_HINTING = FT_LOAD_NO_HINTING;
} // namespace vectext_freetype_constants
#undef FT_LOAD_NO_BITMAP
#undef FT_LOAD_NO_HINTING

export module vectext.ffi.freetype;
import vectext.ffi.owner;

export namespace vectext::ffi::freetype {
using ::FT_Bitmap;
using ::FT_Done_Face;
using ::FT_Done_FreeType;
using ::FT_ENCODING_UNICODE;
using ::FT_F26Dot6;
using ::FT_Face;
using ::FT_GLYPH_FORMAT_OUTLINE;
using ::FT_Init_FreeType;
using ::FT_Library;
using ::FT_Load_Glyph;
using ::FT_New_Memory_Face;
using ::FT_PIXEL_MODE_BGRA;
using ::FT_PIXEL_MODE_GRAY;
using ::FT_PIXEL_MODE_MONO;
using ::FT_Render_Glyph;
using ::FT_RENDER_MODE_NORMAL;
using ::FT_Select_Charmap;
using ::FT_Set_Char_Size;
inline constexpr auto FT_LOAD_NO_BITMAP = vectext_freetype_constants::k_FT_LOAD_NO_BITMAP;
inline constexpr auto FT_LOAD_NO_HINTING = vectext_freetype_constants::k_FT_LOAD_NO_HINTING;
inline bool is_scalable(FT_Face face) { return FT_IS_SCALABLE(face); }

using ::FT_COLOR_NO_ROOT_TRANSFORM;
using ::FT_Get_Char_Index;
using ::FT_Get_Color_Glyph_Layer;
using ::FT_Get_Color_Glyph_Paint;
using ::FT_LayerIterator;
using ::FT_OpaquePaint;
using ::FT_UInt;
using Library = Owner<FT_LibraryRec_, FT_Done_FreeType>;
using Face = Owner<FT_FaceRec_, FT_Done_Face>;
} // namespace vectext::ffi::freetype
