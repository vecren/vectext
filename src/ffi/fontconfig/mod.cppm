module;
#include <climits>
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>
namespace vectext_fontconfig_constants {
inline constexpr auto k_FcFalse = FcFalse;
inline constexpr auto k_FcTrue = FcTrue;
inline constexpr auto k_FC_CHARSET = FC_CHARSET;
inline constexpr auto k_FC_FAMILY = FC_FAMILY;
inline constexpr auto k_FC_FILE = FC_FILE;
inline constexpr auto k_FC_INDEX = FC_INDEX;
inline constexpr auto k_FC_LANG = FC_LANG;
inline constexpr auto k_FC_PIXEL_SIZE = FC_PIXEL_SIZE;
inline constexpr auto k_FC_SCALABLE = FC_SCALABLE;
inline constexpr auto k_FC_SLANT = FC_SLANT;
inline constexpr auto k_FC_SLANT_ITALIC = FC_SLANT_ITALIC;
inline constexpr auto k_FC_SLANT_ROMAN = FC_SLANT_ROMAN;
inline constexpr auto k_FC_WEIGHT = FC_WEIGHT;
inline constexpr auto k_FC_WEIGHT_BOLD = FC_WEIGHT_BOLD;
inline constexpr auto k_FC_WEIGHT_REGULAR = FC_WEIGHT_REGULAR;
} // namespace vectext_fontconfig_constants
#undef FC_CHARSET
#undef FC_FAMILY
#undef FC_FILE
#undef FC_INDEX
#undef FC_LANG
#undef FC_PIXEL_SIZE
#undef FC_SCALABLE
#undef FC_SLANT
#undef FC_SLANT_ITALIC
#undef FC_SLANT_ROMAN
#undef FC_WEIGHT
#undef FC_WEIGHT_BOLD
#undef FC_WEIGHT_REGULAR

#undef FcTrue
#undef FcFalse
export module vectext.ffi.fontconfig;
import vectext.ffi.owner;

export namespace vectext::ffi::fontconfig {
using ::FcBool;
using ::FcChar8;
using ::FcCharSet;
using ::FcCharSetHasChar;
using ::FcConfig;
using ::FcConfigBuildFonts;
using ::FcConfigCreate;
using ::FcConfigDestroy;
using ::FcConfigGetFonts;
using ::FcConfigParseAndLoad;
using ::FcConfigSubstitute;
using ::FcDefaultSubstitute;
inline constexpr auto FcFalse = vectext_fontconfig_constants::k_FcFalse;
using ::FcFontSet;
using ::FcFontSetAdd;
using ::FcFontSetCreate;
using ::FcFontSetDestroy;
using ::FcFontSetSort;
using ::FcFreeTypeQueryFace;
using ::FcInitLoadConfigAndFonts;
using ::FcMatchPattern;
using ::FcPattern;
using ::FcPatternAddBool;
using ::FcPatternAddDouble;
using ::FcPatternAddInteger;
using ::FcPatternAddString;
using ::FcPatternCreate;
using ::FcPatternDestroy;
using ::FcPatternDuplicate;
using ::FcPatternGetBool;
using ::FcPatternGetCharSet;
using ::FcPatternGetInteger;
using ::FcPatternGetString;
using ::FcResult;
using ::FcResultMatch;
using ::FcSetApplication;
using ::FcSetSystem;
inline constexpr auto FcTrue = vectext_fontconfig_constants::k_FcTrue;
inline constexpr auto FC_CHARSET = vectext_fontconfig_constants::k_FC_CHARSET;
inline constexpr auto FC_FAMILY = vectext_fontconfig_constants::k_FC_FAMILY;
inline constexpr auto FC_FILE = vectext_fontconfig_constants::k_FC_FILE;
inline constexpr auto FC_INDEX = vectext_fontconfig_constants::k_FC_INDEX;
inline constexpr auto FC_LANG = vectext_fontconfig_constants::k_FC_LANG;
inline constexpr auto FC_PIXEL_SIZE = vectext_fontconfig_constants::k_FC_PIXEL_SIZE;
inline constexpr auto FC_SCALABLE = vectext_fontconfig_constants::k_FC_SCALABLE;
inline constexpr auto FC_SLANT = vectext_fontconfig_constants::k_FC_SLANT;
inline constexpr auto FC_SLANT_ITALIC = vectext_fontconfig_constants::k_FC_SLANT_ITALIC;
inline constexpr auto FC_SLANT_ROMAN = vectext_fontconfig_constants::k_FC_SLANT_ROMAN;
inline constexpr auto FC_WEIGHT = vectext_fontconfig_constants::k_FC_WEIGHT;
inline constexpr auto FC_WEIGHT_BOLD = vectext_fontconfig_constants::k_FC_WEIGHT_BOLD;
inline constexpr auto FC_WEIGHT_REGULAR = vectext_fontconfig_constants::k_FC_WEIGHT_REGULAR;

using Pattern = Owner<FcPattern, FcPatternDestroy>;
using FontSet = Owner<FcFontSet, FcFontSetDestroy>;
using Config = Owner<FcConfig, FcConfigDestroy>;
} // namespace vectext::ffi::fontconfig
