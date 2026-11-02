#include <rstd/test/gtest.hpp>
import vectext;
import vectext.tests;
import rstd.test;
using namespace vectext;
using namespace vectext::tests;
using namespace rstd::prelude;
using namespace rstd::literals;

namespace {
TEST(SpacingUnits, Float32RetainsTheDeclaredOperationOrder) {
    TextSpacing percent{TextSpacingUnit::PercentEm, 1, TextSpacingArithmetic::Float32};
    EXPECT_EQ(percent.resolve(52, 80).unwrap(), Scalar(41.599998474121094));
    EXPECT_EQ(percent.resolve(26, 80).unwrap(), Scalar(20.799999237060547));
    EXPECT_EQ(percent.resolve(52, 20).unwrap(), Scalar(10.399999618530273));
    EXPECT_EQ(percent.resolve(0, 80).unwrap(), Scalar(0));
    TextSpacing logical; EXPECT_EQ(logical.resolve(52, -1).unwrap(), Scalar(-1));
    TextSpacing em{TextSpacingUnit::Em}; EXPECT_EQ(em.resolve(26, .5).unwrap(), Scalar(13));
    TextSpacing ordinary{TextSpacingUnit::PercentEm}; EXPECT_EQ(ordinary.resolve(52, 80).unwrap(), Scalar(41.6));
    const TextSpacing invalid[]{{TextSpacingUnit(99)}, {TextSpacingUnit::Em, -1}, {TextSpacingUnit::Em, 1, TextSpacingArithmetic(99)}};
    for (auto value : invalid) EXPECT_EQ(value.resolve(16, 1).unwrap_err(), TextError::InvalidValue);
    EXPECT_EQ(percent.resolve(-1, 1).unwrap_err(), TextError::InvalidValue);
    EXPECT_EQ(percent.resolve(16, __builtin_nan("")).unwrap_err(), TextError::InvalidValue);
    EXPECT_EQ(percent.resolve(16, 1e300).unwrap_err(), TextError::Limit);
}

TEST(SpacingUnits, ResolvedAdvancesDriveQueriesAndAlignment) {
    auto service = fixed_service(); TextRequest request{.text = "AAA"_str, .size = 26, .width = Some(Scalar(200)),
        .wrap = TextWrap::None, .alignment = TextAlignment::Center, .character_spacing = 20,
        .spacing = {TextSpacingUnit::PercentEm, 1, TextSpacingArithmetic::Float32}};
    auto relative = service.layout(request).unwrap(); auto gap = request.spacing.resolve(26, 20).unwrap();
    request.character_spacing = gap; request.spacing = {}; auto logical = service.layout(request).unwrap();
    EXPECT_EQ(relative.logical_bounds(), logical.logical_bounds()); EXPECT_EQ(relative.ink_bounds(), logical.ink_bounds());
    EXPECT_EQ(relative.character_spacing(), Scalar(20)); EXPECT_EQ(relative.resolved_character_spacing(), gap);
    for (Index i{}; i < 3; ++i) {
        EXPECT_EQ(relative.glyphs()[usize(i)].position, logical.glyphs()[usize(i)].position);
        EXPECT_EQ(relative.glyphs()[usize(i)].advance, logical.glyphs()[usize(i)].advance);
        EXPECT_EQ(relative.caret(i).unwrap().bounds, logical.caret(i).unwrap().bounds);
    }
    auto caret = relative.caret(1).unwrap(); EXPECT_EQ(relative.hit({caret.bounds.x, caret.bounds.y + 1}).unwrap().byte_index, Index(1));
    EXPECT_EQ(relative.range_boxes({0, 3}).unwrap(), logical.range_boxes({0, 3}).unwrap());
}

TEST(SpacingUnits, ZeroFontKeepsGlyphIdentityFallbackAndEmptyRasterization) {
    auto service = fixed_service(); auto positive = service.layout({.text = "A中e\u0301\tB\nC"_str}).unwrap();
    TextRequest request{.text = positive.text(), .size = 0, .character_spacing = 80,
        .spacing = {TextSpacingUnit::PercentEm, 1, TextSpacingArithmetic::Float32}};
    auto zero = service.layout(request).unwrap(); EXPECT_EQ(zero.logical_bounds().width, Scalar(0));
    EXPECT_EQ(zero.logical_bounds().height, Scalar(0)); EXPECT_EQ(zero.missing_glyphs(), Index(0));
    EXPECT_EQ(zero.glyphs().len(), positive.glyphs().len()); EXPECT_GE(zero.fonts().len(), usize(2));
    EXPECT_EQ(zero.glyphs()[usize()].glyph, positive.glyphs()[usize()].glyph);
    EXPECT_NE(zero.fonts()[usize()].id(), positive.fonts()[usize()].id());
    for (const auto &glyph : zero.glyphs()) { EXPECT_FALSE(glyph.drawable); EXPECT_EQ(glyph.advance, Scalar(0)); }
    auto bitmap = zero.fonts()[usize()].rasterize(zero.glyphs()[usize()].glyph, 2).unwrap();
    EXPECT_EQ(bitmap.width, Index(0)); EXPECT_EQ(bitmap.height, Index(0)); EXPECT_TRUE(bitmap.coverage.is_empty()); EXPECT_EQ(bitmap.scale, Scalar(2));
    EXPECT_EQ(zero.caret(4).unwrap().bounds.height, Scalar(0)); EXPECT_EQ(zero.caret(2).unwrap_err(), TextError::InvalidIndex);
    EXPECT_FALSE(zero.hit({0, 0}).unwrap().inside); EXPECT_TRUE(zero.range_boxes({0, 4}).unwrap().is_empty());
    auto again = service.layout(request).unwrap(); EXPECT_EQ(again.fonts()[usize()].id(), zero.fonts()[usize()].id());
    EXPECT_EQ(service.layout({.size = .001}).unwrap_err(), TextError::InvalidValue);
    TextFormat format; format.size = 0; EXPECT_TRUE(format.valid());
}

TEST(SpacingUnits, ZeroFontPreservesLogicalInlineObjectsAndCollapsesEmObjects) {
    auto service = fixed_service(); auto text = "A\uFFFCB"_str;
    TextInlineObject object{{1, 4}, 10, 8, 2, {1, -8, 8, 10}};
    auto prepare = [&] { return TextInlineObjects::create(text, slice<TextInlineObject>::from_raw_parts(&object, usize(1))).unwrap(); };
    TextRequest request{.text = text, .size = 0, .inline_objects = prepare(), .character_spacing = 80, .spacing = {TextSpacingUnit::PercentEm}};
    auto logical = service.layout(request).unwrap(); EXPECT_EQ(logical.logical_bounds().width, Scalar(10)); EXPECT_EQ(logical.logical_bounds().height, Scalar(10));
    EXPECT_EQ(logical.caret(4).unwrap().bounds.x, Scalar(10));
    auto boxes = logical.range_boxes({1, 4}).unwrap(); ASSERT_EQ(boxes.len(), usize(1)); EXPECT_EQ(boxes[usize()].bounds.width, Scalar(10));
    object.unit = TextInlineUnit::Em; request.inline_objects = prepare(); auto em = service.layout(request).unwrap();
    EXPECT_EQ(em.logical_bounds().width, Scalar(0)); EXPECT_EQ(em.logical_bounds().height, Scalar(0));
    EXPECT_EQ(em.inline_boxes()[usize()].logical.width, Scalar(0)); EXPECT_TRUE(em.range_boxes({1, 4}).unwrap().is_empty());
    request.text = "A\tB"_str; request.inline_objects = {}; auto tabs = service.layout(request).unwrap();
    EXPECT_EQ(tabs.logical_bounds().width, Scalar(0)); EXPECT_TRUE(tabs.caret(3).is_ok());
}

TEST(SpacingUnits, RelativeEllipsisUsesTheSameResolvedSpacing) {
    auto service = fixed_service(); TextRequest request{.text = "AAAAAAAAAAAAAAAAAAAA"_str, .width = Some(Scalar(60)),
        .max_lines = 1, .overflow = TextOverflow::ThreeDots, .character_spacing = 10, .spacing = {TextSpacingUnit::PercentEm}};
    auto relative = service.layout(request).unwrap(); request.character_spacing = relative.resolved_character_spacing(); request.spacing = {};
    auto logical = service.layout(request).unwrap(); EXPECT_EQ(relative.hidden_range(), logical.hidden_range());
    EXPECT_EQ(relative.logical_bounds(), logical.logical_bounds()); EXPECT_EQ(relative.glyphs().len(), logical.glyphs().len());
    for (usize i{}; i < relative.glyphs().len(); ++i) EXPECT_EQ(relative.glyphs()[i].position, logical.glyphs()[i].position);
}
}
