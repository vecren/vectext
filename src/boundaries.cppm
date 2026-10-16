export module vectext:boundaries;
export import vectext.error;
import :types;
using namespace rstd::prelude;
using namespace rstd::literals;

export namespace vectext {
struct TextRange { Index start{}, end{}; auto operator==(const TextRange &) const -> bool = default; };
struct TextWord { TextRange range; bool word{}; auto operator==(const TextWord &) const -> bool = default; };
enum class TextBoundaryBias { Before, After };
struct UnicodeVersion { Index major{}, minor{}, update{}, patch{}; };
class TextBoundaries {
    struct Data {
        Serial id{new_identity()};
        String text;
        Vec<Index> graphemes;
        Vec<TextWord> words;
        explicit Data(ref<str> text) : text(String::make(text)) {}
    };
    Option<rstd::sync::Arc<Data>> data_;
    explicit TextBoundaries(rstd::sync::Arc<Data> data) : data_(Some(rstd::move(data))) {}
    auto lower(Index index) const -> usize {
        auto values = graphemes(); usize low{}, high = values.len();
        while (low < high) { auto mid = (low + high) / usize(2); if (values[mid] < index) low = mid + usize(1); else high = mid; }
        return low;
    }
  public:
    TextBoundaries() = default;
    TextBoundaries(const TextBoundaries &other)
        : data_(other.data_.is_some() ? Some(other.data_->clone()) : None<rstd::sync::Arc<Data>>()) {}
    auto operator=(const TextBoundaries &other) -> TextBoundaries & {
        if (this != &other) data_ = other.data_.is_some() ? Some(other.data_->clone()) : None<rstd::sync::Arc<Data>>();
        return *this;
    }
    TextBoundaries(TextBoundaries &&) = default;
    auto operator=(TextBoundaries &&) -> TextBoundaries & = default;
    static auto create(ref<str>) -> Result<TextBoundaries, TextError>;
    static auto unicode_version() -> UnicodeVersion;
    auto present() const -> bool { return data_.is_some(); }
    auto identity() const -> Serial { return present() ? (*data_)->id : 0; }
    auto operator==(const TextBoundaries &other) const -> bool { return identity() == other.identity(); }
    auto text() const -> ref<str> { return present() ? (*data_)->text.as_str() : ""_str; }
    auto graphemes() const -> slice<Index> { return present() ? (*data_)->graphemes.as_slice() : slice<Index>{}; }
    auto words() const -> slice<TextWord> { return present() ? (*data_)->words.as_slice() : slice<TextWord>{}; }
    auto contains(Index index) const -> bool {
        auto at = lower(index); return at < graphemes().len() && graphemes()[at] == index;
    }
    auto snap(Index index, TextBoundaryBias bias) const -> Result<Index, TextError> {
        if (!present() || usize(index) > text()->len() || unsigned(bias) > unsigned(TextBoundaryBias::After)) return Err(TextError::InvalidIndex);
        auto at = lower(index); auto values = graphemes();
        if (values[at] == index || bias == TextBoundaryBias::After) return Ok(Index(values[at]));
        return Ok(Index(values[at - usize(1)]));
    }
    auto previous(Index index) const -> Result<Index, TextError> {
        if (!present() || usize(index) > text()->len()) return Err(TextError::InvalidIndex);
        auto at = lower(index); return Ok(Index(graphemes()[at == usize() ? at : at - usize(1)]));
    }
    auto next(Index index) const -> Result<Index, TextError> {
        if (!present() || usize(index) > text()->len()) return Err(TextError::InvalidIndex);
        auto at = lower(index); auto values = graphemes();
        if (values[at] == index && at + usize(1) < values.len()) ++at;
        return Ok(Index(values[at]));
    }
    auto word_at(Index index) const -> Result<TextWord, TextError> {
        if (!present() || usize(index) > text()->len()) return Err(TextError::InvalidIndex);
        auto values = words();
        if (values.is_empty()) return Ok(TextWord{});
        usize low{}, high = values.len();
        while (low < high) { auto mid = (low + high) / usize(2); if (values[mid].range.end <= index) low = mid + usize(1); else high = mid; }
        return Ok(TextWord(values[low == values.len() ? low - usize(1) : low]));
    }
};
} // namespace vectext
