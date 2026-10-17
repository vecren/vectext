module;
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <dwrite_3.h>
export module vectext.ffi.directwrite;
import vectext.ffi.owner;
export namespace vectext::ffi::directwrite {
using ::BOOL;
using ::DWRITE_FONT_FACE_TYPE;
using ::DWRITE_FONT_FILE_TYPE;
using ::DWRITE_FONT_METRICS;
using ::DWRITE_FONT_SIMULATIONS_NONE;
using ::DWRITE_FONT_SIMULATIONS_BOLD;
using ::DWRITE_FONT_STRETCH_NORMAL;
using ::DWRITE_FONT_STYLE_ITALIC;
using ::DWRITE_FONT_STYLE_NORMAL;
using ::DWRITE_FONT_WEIGHT_BOLD;
using ::DWRITE_FONT_WEIGHT_NORMAL;
using ::DWRITE_GLYPH_IMAGE_FORMATS;
using ::DWRITE_GLYPH_IMAGE_FORMATS_CFF;
using ::DWRITE_GLYPH_IMAGE_FORMATS_COLR;
using ::DWRITE_GLYPH_IMAGE_FORMATS_JPEG;
using ::DWRITE_GLYPH_IMAGE_FORMATS_NONE;
using ::DWRITE_GLYPH_IMAGE_FORMATS_PNG;
using ::DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8;
using ::DWRITE_GLYPH_IMAGE_FORMATS_SVG;
using ::DWRITE_GLYPH_IMAGE_FORMATS_TIFF;
using ::DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE;
using ::DWRITE_GLYPH_METRICS;
using ::DWRITE_GLYPH_OFFSET;
using ::DWRITE_GLYPH_RUN;
using ::DWRITE_GRID_FIT_MODE_DISABLED;
using ::DWRITE_MEASURING_MODE_NATURAL;
using ::DWRITE_READING_DIRECTION;
using ::DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
using ::DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC;
using ::DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE;
using ::DWRITE_TEXTURE_ALIASED_1x1;
using ::HRESULT;
using ::IDWriteColorGlyphRunEnumerator1;
using ::IDWriteFactory5;
using ::IDWriteFont;
using ::IDWriteFontCollection;
using ::IDWriteFontFace;
using ::IDWriteFontFace3;
using ::IDWriteFontFace4;
using ::IDWriteFontFaceReference;
using ::IDWriteFontFallback;
using ::IDWriteFontFamily;
using ::IDWriteFontFile;
using ::IDWriteGlyphRunAnalysis;
using ::IDWriteInMemoryFontFileLoader;
using ::IDWriteLocalizedStrings;
using ::IDWriteNumberSubstitution;
using ::IDWriteTextAnalysisSource;
using ::IUnknown;
using ::RECT;
using ::UINT32;
using ::ULONG;
using ::WCHAR;
inline constexpr HRESULT success = S_OK, invalid_argument = E_INVALIDARG,
                         no_color = DWRITE_E_NOCOLOR;
template <class T> void release(T *value) { value->Release(); }
template <class T> using Owned = ffi::Owner<T, release<T>>;
template <class T> auto retain(T *value) -> Owned<T> {
    if (value)
        value->AddRef();
    return Owned<T>(value);
}
template <class T, class U> auto query(U *value, Owned<T> &output) -> HRESULT {
    return value->QueryInterface(__uuidof(T), reinterpret_cast<void **>(output.put()));
}
inline auto create_factory(Owned<IDWriteFactory5> &output) -> HRESULT {
    return DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory5),
                               reinterpret_cast<IUnknown **>(output.put()));
}
template <class Interface, class Derived> class RefCounted : public Interface {
    long references_{1};

  public:
    HRESULT __stdcall QueryInterface(REFIID iid, void **output) override {
        if (!output)
            return E_POINTER;
        *output = nullptr;
        if (!InlineIsEqualGUID(iid, __uuidof(IUnknown)) &&
            !InlineIsEqualGUID(iid, __uuidof(Interface)))
            return E_NOINTERFACE;
        *output = static_cast<Interface *>(this);
        AddRef();
        return S_OK;
    }
    ULONG __stdcall AddRef() override { return InterlockedIncrement(&references_); }
    ULONG __stdcall Release() override {
        auto count = InterlockedDecrement(&references_);
        if (!count)
            delete static_cast<Derived *>(this);
        return count;
    }
};
inline auto glyph_indices(IDWriteFontFace *face, const UINT32 *codepoints, UINT32 count,
                          UINT16 *glyphs) -> HRESULT {
    return face->GetGlyphIndices(codepoints, count, glyphs);
}
inline auto equal_name(const wchar_t *a, const wchar_t *b) -> bool {
    return CompareStringOrdinal(a, -1, b, -1, TRUE) == CSTR_EQUAL;
}
} // namespace vectext::ffi::directwrite
