#include <rstd/test/gtest.hpp>
import vectext;
import vectext.tests;
import rstd.test;
using namespace rstd::prelude;
using namespace rstd::literals;
using namespace vectext;
using namespace vectext::tests;

TEST(VectextBoundaries, CombiningEmojiAndFlagsUseUtf8GraphemeIndices) {
    auto text = TextBoundaries::create("a\u0301👩‍🚀🇨🇳中"_str).unwrap();
    const Index expected[]{0, 3, 14, 22, 25};
    ASSERT_EQ(text.graphemes().len(), usize(5));
    for (usize i{}; i < usize(5); ++i) EXPECT_EQ(text.graphemes()[i], expected[i.to_primitive()]);
    EXPECT_FALSE(text.contains(1)); EXPECT_FALSE(text.contains(7));
    EXPECT_EQ(text.previous(14).unwrap(), 3u); EXPECT_EQ(text.next(3).unwrap(), 14u);
    EXPECT_EQ(text.snap(4, TextBoundaryBias::Before).unwrap(), 3u);
    EXPECT_EQ(text.snap(4, TextBoundaryBias::After).unwrap(), 14u);
    EXPECT_EQ(text.previous(0).unwrap(), 0u); EXPECT_EQ(text.next(25).unwrap(), 25u);
    EXPECT_EQ(text.next(26).unwrap_err(), TextError::InvalidIndex);
    auto copy = text; auto id = text.identity(); text = TextBoundaries{};
    EXPECT_EQ(copy.identity(), id); EXPECT_EQ(copy.text(), "a\u0301👩‍🚀🇨🇳中"_str);
}

TEST(VectextBoundaries, EmptyAndWordQueriesHaveExplicitRanges) {
    auto empty = TextBoundaries::create(""_str).unwrap();
    EXPECT_EQ(empty.graphemes().len(), usize(1)); EXPECT_EQ(empty.graphemes()[usize()], 0u);
    EXPECT_EQ(empty.word_at(0).unwrap(), (TextWord{}));
    EXPECT_EQ(TextBoundaries{}.snap(0, TextBoundaryBias::After).unwrap_err(), TextError::InvalidIndex);
    auto text = TextBoundaries::create("brown  dust 2"_str).unwrap();
    EXPECT_EQ(text.word_at(2).unwrap(), (TextWord{{0, 5}, true}));
    EXPECT_EQ(text.word_at(5).unwrap(), (TextWord{{5, 7}, false}));
    EXPECT_EQ(text.word_at(8).unwrap(), (TextWord{{7, 11}, true}));
    EXPECT_EQ(text.word_at(13).unwrap(), (TextWord{{12, 13}, true}));
    EXPECT_EQ(text.word_at(14).unwrap_err(), TextError::InvalidIndex);
}

TEST(VectextBoundaries, PreparedAnalysisSurvivesLayoutAndPreservesLegacyCaretQueries) {
    auto service = fixed_service(); auto prepared = TextBoundaries::create("a\u0301中"_str).unwrap();
    auto id = prepared.identity();
    auto layout = service.layout({.text = prepared.text(), .boundaries = &prepared}).unwrap();
    EXPECT_EQ(layout.boundaries().identity(), id); prepared = TextBoundaries{};
    EXPECT_EQ(layout.text(), "a\u0301中"_str); EXPECT_TRUE(layout.caret(1).is_ok());
    EXPECT_EQ(layout.caret_grapheme(1).unwrap_err(), TextError::InvalidIndex);
    EXPECT_TRUE(layout.caret_grapheme(3).is_ok());
    auto height = layout.logical_bounds().height;
    for (Scalar x{}; x < layout.logical_bounds().width; x += 1)
        EXPECT_TRUE(layout.boundaries().contains(layout.hit_grapheme({x, height / 2}).unwrap().byte_index));
    auto wrong = TextBoundaries::create("wrong"_str).unwrap();
    EXPECT_EQ(service.layout({.text = "text"_str, .boundaries = &wrong}).unwrap_err(), TextError::InvalidValue);
    auto absent = TextBoundaries{};
    EXPECT_EQ(service.layout({.text = "text"_str, .boundaries = &absent}).unwrap_err(), TextError::InvalidValue);
}

TEST(VectextBoundaries, Unicode17GraphemeBreakConformance) {
    auto source = data_text("GraphemeBreakTest.txt"_str); auto chars = source.as_str()->chars();
    String text; Vec<Index> expected; unsigned hex{}, digits{}, cases{}; bool comment{};
    auto number = [&] { if (digits) text.push(char32_t(hex)); hex = digits = 0; };
    auto line = [&] {
        number();
        if (!expected.is_empty()) {
            auto result = TextBoundaries::create(text.as_str()).unwrap();
            ASSERT_EQ(result.graphemes().len(), expected.len());
            for (usize i{}; i < expected.len(); ++i) EXPECT_EQ(result.graphemes()[i], expected[i]);
            ++cases;
        }
        text.clear(); expected.clear(); comment = false;
    };
    for (auto item = chars.next(); item.is_some(); item = chars.next()) {
        auto cp = item->to_primitive();
        if (cp == '\n') { line(); continue; }
        if (comment) continue;
        if (cp == '#') { number(); comment = true; continue; }
        if (cp == 0xf7) { number(); expected.push(Index(text.len().to_primitive())); }
        else if (cp == 0xd7) number();
        else if (cp >= '0' && cp <= '9') { hex = hex * 16 + cp - '0'; ++digits; }
        else if (cp >= 'A' && cp <= 'F') { hex = hex * 16 + cp - 'A' + 10; ++digits; }
        else number();
    }
    line(); EXPECT_GT(cases, 700u);
    rstd::io::println("Unicode 17 grapheme cases={}", cases);
}

TEST(VectextBoundaries, DefaultWordsClassifyCjkAndSkipEmojiWithoutDictionaries) {
    auto text = TextBoundaries::create("中国 カタカナ 👩‍🚀 12"_str).unwrap();
    EXPECT_EQ(text.word_at(0).unwrap(), (TextWord{{0, 3}, true}));
    EXPECT_EQ(text.word_at(3).unwrap(), (TextWord{{3, 6}, true}));
    EXPECT_EQ(text.word_at(7).unwrap(), (TextWord{{7, 19}, true}));
    EXPECT_EQ(text.word_at(20).unwrap(), (TextWord{{20, 31}, false}));
    EXPECT_EQ(text.word_at(32).unwrap(), (TextWord{{32, 34}, true}));
}
