module;
#define U_SHOW_CPLUSPLUS_API 0
#include <unicode/ubrk.h>
#include <unicode/utext.h>
#include <unicode/uchar.h>
module vectext;
using namespace rstd::prelude;

namespace vectext {
auto TextBoundaries::create(ref<str> text) -> Result<TextBoundaries, TextError> {
    if (text->len() > usize(0x7fffffff)) return Err(TextError::Limit);
    if (rstd::str_::from_utf8(text->as_bytes()).is_err()) return Err(TextError::InvalidUtf8);
    auto data = rstd::sync::Arc<Data>::make(text);
    struct TextOwner { UText value = UTEXT_INITIALIZER; ~TextOwner() { utext_close(&value); } } native;
    auto bytes = data->text.as_str()->as_bytes(); UErrorCode status = U_ZERO_ERROR;
    utext_openUTF8(&native.value, reinterpret_cast<const char *>(bytes.as_ptr().as_raw_ptr()), int64_t(bytes.len().to_primitive()), &status);
    if (U_FAILURE(status)) return Err(TextError::Layout);
    const UBreakIteratorType types[]{UBRK_CHARACTER, UBRK_WORD};
    for (auto type : types) {
        struct IteratorOwner { UBreakIterator *value{}; ~IteratorOwner() { if (value) ubrk_close(value); } } iterator;
        iterator.value = ubrk_open(type, "", nullptr, 0, &status);
        if (U_FAILURE(status) || !iterator.value) return Err(TextError::Layout);
        ubrk_setUText(iterator.value, &native.value, &status);
        if (U_FAILURE(status)) return Err(TextError::Layout);
        auto start = ubrk_first(iterator.value);
        if (type == UBRK_CHARACTER) data->graphemes.push(Index(start));
        for (auto end = ubrk_next(iterator.value); end != UBRK_DONE; start = end, end = ubrk_next(iterator.value)) {
            if (start < 0 || end < start || usize(unsigned(end)) > bytes.len() || !text->is_char_boundary(usize(unsigned(end))))
                return Err(TextError::Layout);
            if (type == UBRK_CHARACTER) data->graphemes.push(Index(end));
            else data->words.push(TextWord{{Index(start), Index(end)}, ubrk_getRuleStatus(iterator.value) >= UBRK_WORD_NONE_LIMIT});
        }
    }
    return Ok(TextBoundaries(rstd::move(data)));
}
auto TextBoundaries::unicode_version() -> UnicodeVersion {
    UVersionInfo version; u_getUnicodeVersion(version); return {version[0], version[1], version[2], version[3]};
}
} // namespace vectext
