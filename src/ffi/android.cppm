module;
#include <android/font_matcher.h>

module vectext:ffi.android;
import vectext.ffi.owner;

namespace ffi {
using ::AFont;
using ::AFont_close;
using ::AFont_getAxisCount;
using ::AFont_getAxisTag;
using ::AFont_getAxisValue;
using ::AFont_getCollectionIndex;
using ::AFont_getFontFilePath;
using ::AFont_getWeight;
using ::AFont_isItalic;
using ::AFontMatcher;
using ::AFontMatcher_create;
using ::AFontMatcher_destroy;
using ::AFontMatcher_match;
using ::AFontMatcher_setLocales;
using ::AFontMatcher_setStyle;
using Font = vectext::ffi::Owner<AFont, AFont_close>;
using Matcher = vectext::ffi::Owner<AFontMatcher, AFontMatcher_destroy>;
} // namespace ffi
