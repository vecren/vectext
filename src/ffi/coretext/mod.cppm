module;
#include <CoreText/CoreText.h>
export module vectext.ffi.coretext;
import vectext.ffi.owner;
export namespace vectext::ffi::coretext {
using ::CTFontCopyDefaultCascadeListForLanguages;
using ::CTFontCopyFamilyName;
using ::CTFontCopyFontDescriptor;
using ::CTFontCopyFullName;
using ::CTFontCopyTable;
using ::CTFontCreatePathForGlyph;
using ::CTFontCreateWithFontDescriptor;
using ::CTFontDescriptorCopyAttribute;
using ::CTFontDescriptorCreateWithAttributes;
using ::CTFontDescriptorRef;
using ::CTFontGetAdvancesForGlyphs;
using ::CTFontGetAscent;
using ::CTFontGetBoundingRectsForGlyphs;
using ::CTFontGetDescent;
using ::CTFontGetGlyphCount;
using ::CTFontGetGlyphsForCharacters;
using ::CTFontGetLeading;
using ::CTFontGetSymbolicTraits;
using ::CTFontManagerCreateFontDescriptorsFromData;
using ::CTFontRef;
using ::kCTFontBoldTrait;
using ::kCTFontCharacterSetAttribute;
using ::kCTFontFamilyNameAttribute;
using ::kCTFontItalicTrait;
using ::kCTFontOrientationHorizontal;
using ::kCTFontSymbolicTrait;
using ::kCTFontTableOptionNoOptions;
using ::kCTFontTraitsAttribute;
} // namespace vectext::ffi::coretext
