#include <rstd/test/gtest.hpp>
import rstd.test;
import vectext;

namespace {
using namespace vectext;
using namespace rstd::prelude;
using namespace rstd::literals;

static_assert(__is_same(decltype(TextFormatBuilder{}.size(20).bold(true)), TextFormatBuilder));

TEST(TextFormatBuilder, PreparedResultsAndExternalStringsSurviveTheBuilder) {
    auto family = String::make("sample family"_str);
    auto language = String::make("en"_str);
    TextFormat first, second;
    {
        Index position = 2;
        auto breaks =
            TextLineBreaks::create("a b"_str, slice<Index>::from_raw_parts(&position, usize(1)))
                .unwrap();
        auto builder = TextFormatBuilder{}
                           .family(family.as_str())
                           .language(language.as_str())
                           .size(20)
                           .source(Some(FontSource{4, 9}))
                           .line_metrics(TextLineMetrics::PrimaryFont)
                           .line_advance(TextLineAdvance::FontHeight)
                           .line_breaks(rstd::move(breaks));
        static_assert(__is_same(decltype(builder.bold(true)), TextFormatBuilder &));
        first = builder.build();
        second = rstd::move(builder).size(30).bold(true).build();
    }
    EXPECT_TRUE(first.valid());
    EXPECT_TRUE(second.valid());
    EXPECT_EQ(first.size, 20);
    EXPECT_EQ(second.size, 30);
    EXPECT_FALSE(first.bold);
    EXPECT_TRUE(second.bold);
    EXPECT_EQ(second.family, family.as_str());
    EXPECT_EQ(second.language, language.as_str());
    EXPECT_EQ(second.source, Some(FontSource{4, 9}));
    EXPECT_TRUE(second.line_breaks.matches("a b"_str));
    ASSERT_EQ(second.line_breaks.positions().len(), usize(1));
    EXPECT_EQ(second.line_breaks.positions()[usize()], 2u);
    EXPECT_EQ(first.line_breaks, second.line_breaks);
    EXPECT_EQ(first.line_metrics, TextLineMetrics::PrimaryFont);
    EXPECT_EQ(second.line_metrics, TextLineMetrics::PrimaryFont);
    EXPECT_EQ(first.line_advance, TextLineAdvance::FontHeight);
    EXPECT_EQ(second.line_advance, TextLineAdvance::FontHeight);
    EXPECT_EQ(TextFormatBuilder{}.build().line_advance, TextLineAdvance::LineBox);
}

TEST(TextFormatBuilder, FinalValuesCanBeRepairedAndOptionalSettingsCleared) {
    auto builder = TextFormatBuilder{}
                       .size(-1)
                       .auto_size(Some(TextAutoSize{10, 30, 1}))
                       .source(Some(FontSource{1, 2}));
    EXPECT_FALSE(builder.build().valid());
    auto format =
        builder.size(18).auto_size(None<TextAutoSize>()).source(None<FontSource>()).build();
    EXPECT_TRUE(format.valid());
    EXPECT_TRUE(format.auto_size.is_none());
    EXPECT_TRUE(format.source.is_none());
}
} // namespace
