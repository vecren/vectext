module vectext;
import liunicode;
using namespace rstd::prelude;
namespace vectext {
auto TextBoundaries::create(ref<str> text) -> Result<TextBoundaries, TextError> {
    if (text->len() > usize(0x7fffffff)) return Err(TextError::Limit);
    auto parsed = liunicode::segment_utf8(text);
    if (parsed.is_err()) return Err(TextError::InvalidUtf8);
    auto data = rstd::sync::Arc<Data>::make(text);
    for (auto index : parsed->graphemes) data->graphemes.push(Index(index.to_primitive()));
    for (const auto &word : parsed->words)
        data->words.push(TextWord{{Index(word.start.to_primitive()), Index(word.end.to_primitive())}, word.is_word()});
    return Ok(TextBoundaries(rstd::move(data)));
}
auto TextBoundaries::unicode_version() -> UnicodeVersion {
    return {liunicode::version_major, liunicode::version_minor, liunicode::version_patch, 0};
}
}
