export module vectext:layout.format_builder;
import :layout.types;
using namespace rstd::prelude;

export namespace vectext {
class TextFormatBuilder {
    TextFormat value_;

  public:
    TextFormatBuilder() = default;
    explicit TextFormatBuilder(TextFormat value) : value_(rstd::move(value)) {}

    auto family(ref<str> value) & -> TextFormatBuilder & {
        value_.family = rstd::move(value);
        return *this;
    }
    auto family(ref<str> value) && -> TextFormatBuilder {
        family(rstd::move(value));
        return rstd::move(*this);
    }
    auto language(ref<str> value) & -> TextFormatBuilder & {
        value_.language = rstd::move(value);
        return *this;
    }
    auto language(ref<str> value) && -> TextFormatBuilder {
        language(rstd::move(value));
        return rstd::move(*this);
    }
    auto size(Scalar value) & -> TextFormatBuilder & {
        value_.size = rstd::move(value);
        return *this;
    }
    auto size(Scalar value) && -> TextFormatBuilder {
        size(rstd::move(value));
        return rstd::move(*this);
    }
    auto direction(TextDirection value) & -> TextFormatBuilder & {
        value_.direction = rstd::move(value);
        return *this;
    }
    auto direction(TextDirection value) && -> TextFormatBuilder {
        direction(rstd::move(value));
        return rstd::move(*this);
    }
    auto wrap(TextWrap value) & -> TextFormatBuilder & {
        value_.wrap = rstd::move(value);
        return *this;
    }
    auto wrap(TextWrap value) && -> TextFormatBuilder {
        wrap(rstd::move(value));
        return rstd::move(*this);
    }
    auto bold(bool value) & -> TextFormatBuilder & {
        value_.bold = rstd::move(value);
        return *this;
    }
    auto bold(bool value) && -> TextFormatBuilder {
        bold(rstd::move(value));
        return rstd::move(*this);
    }
    auto italic(bool value) & -> TextFormatBuilder & {
        value_.italic = rstd::move(value);
        return *this;
    }
    auto italic(bool value) && -> TextFormatBuilder {
        italic(rstd::move(value));
        return rstd::move(*this);
    }
    auto alignment(TextAlignment value) & -> TextFormatBuilder & {
        value_.alignment = rstd::move(value);
        return *this;
    }
    auto alignment(TextAlignment value) && -> TextFormatBuilder {
        alignment(rstd::move(value));
        return rstd::move(*this);
    }
    auto line_spacing(Scalar value) & -> TextFormatBuilder & {
        value_.line_spacing = rstd::move(value);
        return *this;
    }
    auto line_spacing(Scalar value) && -> TextFormatBuilder {
        line_spacing(rstd::move(value));
        return rstd::move(*this);
    }
    auto line_metrics(TextLineMetrics value) & -> TextFormatBuilder & {
        value_.line_metrics = value;
        return *this;
    }
    auto line_metrics(TextLineMetrics value) && -> TextFormatBuilder {
        line_metrics(value);
        return rstd::move(*this);
    }
    auto line_advance(TextLineAdvance value) & -> TextFormatBuilder & {
        value_.line_advance = value;
        return *this;
    }
    auto line_advance(TextLineAdvance value) && -> TextFormatBuilder {
        line_advance(value);
        return rstd::move(*this);
    }
    auto line_height(Option<Scalar> value) & -> TextFormatBuilder & {
        value_.line_height = rstd::move(value);
        return *this;
    }
    auto line_height(Option<Scalar> value) && -> TextFormatBuilder {
        line_height(rstd::move(value));
        return rstd::move(*this);
    }
    auto line_height(Scalar value) & -> TextFormatBuilder & {
        return line_height(Some(Scalar(value)));
    }
    auto line_height(Scalar value) && -> TextFormatBuilder {
        line_height(value);
        return rstd::move(*this);
    }
    auto line_height_alignment(TextLineHeightAlignment value) & -> TextFormatBuilder & {
        value_.line_height_alignment = value;
        return *this;
    }
    auto line_height_alignment(TextLineHeightAlignment value) && -> TextFormatBuilder {
        line_height_alignment(value);
        return rstd::move(*this);
    }
    auto character_spacing(Scalar value) & -> TextFormatBuilder & {
        value_.character_spacing = rstd::move(value);
        return *this;
    }
    auto character_spacing(Scalar value) && -> TextFormatBuilder {
        character_spacing(rstd::move(value));
        return rstd::move(*this);
    }
    auto spacing(TextSpacing value) & -> TextFormatBuilder & {
        value_.spacing = rstd::move(value);
        return *this;
    }
    auto spacing(TextSpacing value) && -> TextFormatBuilder {
        spacing(rstd::move(value));
        return rstd::move(*this);
    }
    auto max_lines(Index value) & -> TextFormatBuilder & {
        value_.max_lines = rstd::move(value);
        return *this;
    }
    auto max_lines(Index value) && -> TextFormatBuilder {
        max_lines(rstd::move(value));
        return rstd::move(*this);
    }
    auto overflow(TextOverflow value) & -> TextFormatBuilder & {
        value_.overflow = rstd::move(value);
        return *this;
    }
    auto overflow(TextOverflow value) && -> TextFormatBuilder {
        overflow(rstd::move(value));
        return rstd::move(*this);
    }
    auto auto_size(Option<TextAutoSize> value) & -> TextFormatBuilder & {
        value_.auto_size = rstd::move(value);
        return *this;
    }
    auto auto_size(Option<TextAutoSize> value) && -> TextFormatBuilder {
        auto_size(rstd::move(value));
        return rstd::move(*this);
    }
    auto source(Option<FontSource> value) & -> TextFormatBuilder & {
        value_.source = rstd::move(value);
        return *this;
    }
    auto source(Option<FontSource> value) && -> TextFormatBuilder {
        source(rstd::move(value));
        return rstd::move(*this);
    }
    auto line_breaks(TextLineBreaks value) & -> TextFormatBuilder & {
        value_.line_breaks = rstd::move(value);
        return *this;
    }
    auto line_breaks(TextLineBreaks value) && -> TextFormatBuilder {
        line_breaks(rstd::move(value));
        return rstd::move(*this);
    }
    auto whitespace(TextWhitespace value) & -> TextFormatBuilder & {
        value_.whitespace = rstd::move(value);
        return *this;
    }
    auto whitespace(TextWhitespace value) && -> TextFormatBuilder {
        whitespace(rstd::move(value));
        return rstd::move(*this);
    }
    auto build() const & -> TextFormat { return value_; }
    auto build() && -> TextFormat { return rstd::move(value_); }
};
} // namespace vectext
