module;
#include <climits>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_COLOR_H
#include FT_OUTLINE_H
#include FT_MULTIPLE_MASTERS_H
namespace vectext_freetype_constants {
inline constexpr auto k_FT_LOAD_NO_BITMAP = FT_LOAD_NO_BITMAP;
inline constexpr auto k_FT_LOAD_NO_HINTING = FT_LOAD_NO_HINTING;
inline constexpr auto k_FT_STYLE_FLAG_BOLD = FT_STYLE_FLAG_BOLD;
inline constexpr auto k_FT_STYLE_FLAG_ITALIC = FT_STYLE_FLAG_ITALIC;
} // namespace vectext_freetype_constants
#undef FT_LOAD_NO_BITMAP
#undef FT_LOAD_NO_HINTING
#undef FT_STYLE_FLAG_BOLD
#undef FT_STYLE_FLAG_ITALIC

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
using ::FT_Outline_Decompose;
using ::FT_Outline_Funcs;
using ::FT_PIXEL_MODE_BGRA;
using ::FT_PIXEL_MODE_GRAY;
using ::FT_PIXEL_MODE_MONO;
using ::FT_Render_Glyph;
using ::FT_RENDER_MODE_NORMAL;
using ::FT_Select_Charmap;
using ::FT_Set_Char_Size;
inline constexpr auto FT_LOAD_NO_BITMAP = vectext_freetype_constants::k_FT_LOAD_NO_BITMAP;
inline constexpr auto FT_LOAD_NO_HINTING = vectext_freetype_constants::k_FT_LOAD_NO_HINTING;
inline constexpr auto FT_STYLE_FLAG_BOLD = vectext_freetype_constants::k_FT_STYLE_FLAG_BOLD;
inline constexpr auto FT_STYLE_FLAG_ITALIC = vectext_freetype_constants::k_FT_STYLE_FLAG_ITALIC;
inline bool is_scalable(FT_Face face) { return FT_IS_SCALABLE(face); }
inline bool has_variations(FT_Face face) { return FT_HAS_MULTIPLE_MASTERS(face); }

using ::FT_COLOR_NO_ROOT_TRANSFORM;
using ::FT_Get_Char_Index;
using ::FT_Get_Color_Glyph_Layer;
using ::FT_Get_Color_Glyph_Paint;
using ::FT_LayerIterator;
using ::FT_OpaquePaint;
using ::FT_UInt;
using ::FT_Vector;
using Library = Owner<FT_LibraryRec_, FT_Done_FreeType>;
using Face = Owner<FT_FaceRec_, FT_Done_Face>;
using ::FT_Done_MM_Var;
using ::FT_Fixed;
using ::FT_Get_MM_Var;
using ::FT_Get_Var_Design_Coordinates;
using ::FT_MM_Var;
using ::FT_Set_Var_Design_Coordinates;
} // namespace vectext::ffi::freetype
