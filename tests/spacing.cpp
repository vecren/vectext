#include <rstd/test/gtest.hpp>
import vectext;
import vectext.tests;
import rstd.test;
using namespace vectext;
using namespace vectext::tests;
using namespace rstd::prelude;
using namespace rstd::literals;

namespace {
auto near(Scalar a, Scalar b) -> bool { return __builtin_fabs(a - b) < 1e-9; }

TEST(CharacterSpacing, AdvancesQueriesAndDefaultAreConsistent) {
    auto service = fixed_service();
    TextRequest request{.text = "AAA"_str, .wrap = TextWrap::None};
    auto original = service.layout(request).unwrap();
    request.character_spacing = 0;
    auto zero = service.layout(request).unwrap();
    EXPECT_EQ(zero.logical_bounds(), original.logical_bounds());
    EXPECT_EQ(zero.ink_bounds(), original.ink_bounds());
    for (Index i{}; i < 3; ++i) {
        EXPECT_EQ(zero.glyphs()[usize(i)].position, original.glyphs()[usize(i)].position);
        EXPECT_EQ(zero.glyphs()[usize(i)].advance, original.glyphs()[usize(i)].advance);
    }
    const Scalar gaps[]{4, -1};
    for (auto gap : gaps) {
        request.character_spacing = gap;
        auto spaced = service.layout(request).unwrap();
        EXPECT_EQ(spaced.character_spacing(), gap);
        EXPECT_TRUE(near(spaced.logical_bounds().width, original.logical_bounds().width + 2 * gap));
        for (Index i{}; i < 3; ++i) {
            auto &glyph = spaced.glyphs()[usize(i)];
            EXPECT_TRUE(near(glyph.position.x, original.glyphs()[usize(i)].position.x + i * gap));
            EXPECT_TRUE(near(glyph.advance, original.glyphs()[usize(i)].advance + (i < 2 ? gap : 0)));
            EXPECT_TRUE(near(spaced.caret(i).unwrap().bounds.x, glyph.position.x));
            EXPECT_EQ(spaced.hit({glyph.position.x, 1}).unwrap().byte_index, i);
        }
        EXPECT_TRUE(near(spaced.caret(3).unwrap().bounds.x, spaced.logical_bounds().width));
        auto boxes = spaced.range_boxes({0, 3}).unwrap();
        ASSERT_EQ(boxes.len(), usize(1));
        EXPECT_TRUE(near(boxes[usize()].bounds.width, spaced.logical_bounds().width));
    }
    EXPECT_EQ(original.character_spacing(), Scalar(0));
}

TEST(CharacterSpacing, WrapAlignmentAndNewlinesExcludeTrailingSpacing) {
    auto service = fixed_service();
    auto a = service.layout({.text = "A"_str}).unwrap().logical_bounds().width;
    TextRequest request{.text = "AAA"_str, .width = Some(a * 2 + 4), .character_spacing = 4};
    auto wrapped = service.layout(request).unwrap();
    ASSERT_EQ(wrapped.lines().len(), usize(2));
    EXPECT_EQ(wrapped.lines()[usize()].byte_end, Index(2));
    EXPECT_TRUE(near(wrapped.lines()[usize()].logical.width, a * 2 + 4));
    EXPECT_TRUE(near(wrapped.lines()[usize(1)].logical.width, a));
    request.width = Some(a);
    auto singles = service.layout(request).unwrap();
    ASSERT_EQ(singles.lines().len(), usize(3));
    for (const auto &line : singles.lines()) EXPECT_TRUE(near(line.logical.width, a));
    request.text = "AA\nA\n"_str;
    request.width = Some(Scalar(100));
    request.alignment = TextAlignment::Center;
    auto aligned = service.layout(request).unwrap();
    ASSERT_EQ(aligned.lines().len(), usize(3));
    EXPECT_TRUE(near(aligned.lines()[usize()].logical.x, (100 - 2 * a - 4) / 2));
    EXPECT_TRUE(near(aligned.lines()[usize(1)].logical.x, (100 - a) / 2));
    EXPECT_EQ(aligned.lines()[usize(2)].logical.width, Scalar(0));
}

TEST(CharacterSpacing, TabsKeepStopsAndDoNotAddTheirOwnSpacing) {
    auto service = fixed_service();
    TextRequest request{.text = "A\tB"_str, .wrap = TextWrap::None};
    auto base = service.layout(request).unwrap();
    request.character_spacing = 2;
    auto stops = service.layout(request).unwrap();
    EXPECT_EQ(stops.glyphs()[usize(2)].position.x, base.glyphs()[usize(2)].position.x);
    EXPECT_EQ(stops.logical_bounds().width, base.logical_bounds().width);
    EXPECT_TRUE(near(stops.glyphs()[usize(1)].advance, base.glyphs()[usize(1)].advance - 2));
    request.tabs = TextTabs::Ignore;
    auto ignored = service.layout(request).unwrap();
    EXPECT_EQ(ignored.glyphs()[usize(1)].advance, Scalar(0));
    EXPECT_TRUE(near(ignored.glyphs()[usize(2)].position.x, base.glyphs()[usize()].advance + 2));
}

TEST(CharacterSpacing, InlineObjectsAndScalarBoundariesShareTheSameAdvances) {
    auto service = fixed_service();
    auto text = "A\uFFFCé"_str;
    TextInlineObject object{{1, 4}, 10, 8, 2, {0, -8, 10, 10}};
    auto objects = TextInlineObjects::create(text, slice<TextInlineObject>::from_raw_parts(&object, usize(1))).unwrap();
    TextRequest request{.text = text, .wrap = TextWrap::None, .inline_objects = objects};
    auto base = service.layout(request).unwrap();
    request.character_spacing = 3;
    auto spaced = service.layout(request).unwrap();
    EXPECT_TRUE(near(spaced.logical_bounds().width, base.logical_bounds().width + 6));
    EXPECT_TRUE(near(spaced.inline_boxes()[usize()].logical.x, base.inline_boxes()[usize()].logical.x + 3));
    EXPECT_EQ(spaced.inline_boxes()[usize()].logical.width, Scalar(13));
    EXPECT_TRUE(near(spaced.glyphs()[usize(1)].position.x, base.glyphs()[usize(1)].position.x + 6));
    EXPECT_TRUE(near(spaced.caret(4).unwrap().bounds.x, spaced.glyphs()[usize(1)].position.x));
    EXPECT_EQ(spaced.caret(5).unwrap_err(), TextError::InvalidIndex);
    auto boxes = spaced.range_boxes({0, 6}).unwrap();
    ASSERT_EQ(boxes.len(), usize(1));
    EXPECT_TRUE(near(boxes[usize()].bounds.width, spaced.logical_bounds().width));
    auto combining = service.layout({.text = "e\u0301A"_str, .character_spacing = 2}).unwrap();
    EXPECT_EQ(combining.glyphs().len(), usize(3));
    EXPECT_EQ(combining.caret(2).unwrap_err(), TextError::InvalidIndex);
    EXPECT_TRUE(combining.caret(3).is_ok());
}

TEST(CharacterSpacing, EllipsisFitsIncludingInternalSpacingAndNoTrailingGap) {
    auto service = fixed_service();
    auto a = service.layout({.text = "A"_str}).unwrap().logical_bounds().width;
    auto dot = service.layout({.text = "."_str}).unwrap().logical_bounds().width;
    Scalar gap = 2, width = 2 * a + 3 * dot + 4 * gap;
    auto layout = service.layout({.text = "AAAAAAAAAAAAAAAAAAAA"_str, .width = Some(width),
        .max_lines = 1, .overflow = TextOverflow::ThreeDots, .character_spacing = gap}).unwrap();
    EXPECT_EQ(layout.hidden_range(), Some(TextRange{2, 20}));
    ASSERT_EQ(layout.glyphs().len(), usize(5));
    EXPECT_TRUE(near(layout.logical_bounds().width, width));
    EXPECT_EQ(layout.glyphs()[usize(4)].advance, dot);
    EXPECT_EQ(layout.glyphs()[usize(3)].advance, dot + gap);
    EXPECT_TRUE(layout.glyphs()[usize(4)].synthetic);
    auto boxes = layout.range_boxes({0, 20}).unwrap();
    ASSERT_EQ(boxes.len(), usize(1));
    EXPECT_TRUE(near(boxes[usize()].bounds.width, 2 * (a + gap)));
}

TEST(CharacterSpacing, InvalidValuesAndNonmonotoneAdvancesAreRejected) {
    auto service = fixed_service();
    const Scalar invalid[]{__builtin_nan(""), __builtin_inf(), -__builtin_inf()};
    for (auto value : invalid) {
        EXPECT_EQ(service.layout({.text = "AA"_str, .character_spacing = value}).unwrap_err(), TextError::InvalidValue);
        TextFormat format; format.character_spacing = value; EXPECT_FALSE(format.valid());
    }
    EXPECT_EQ(service.layout({.text = "AA"_str, .character_spacing = 1048577}).unwrap_err(), TextError::Limit);
    EXPECT_EQ(service.layout({.text = "AA"_str, .character_spacing = -100}).unwrap_err(), TextError::Unsupported);
    EXPECT_TRUE(service.layout({.text = "A"_str, .character_spacing = -100}).is_ok());
    auto empty = service.layout({.text = ""_str, .character_spacing = 5}).unwrap();
    EXPECT_EQ(empty.logical_bounds().width, Scalar(0));
    EXPECT_TRUE(empty.caret(0).is_ok());
}
}
