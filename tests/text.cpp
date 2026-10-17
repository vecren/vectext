#include <rstd/test/gtest.hpp>

import vectext;
import vectext.tests;
import rstd.test;
using namespace vectext::tests;
using namespace rstd::prelude;
using namespace rstd::literals;

namespace {
using namespace vectext;
auto near(Scalar a, Scalar b, Scalar tolerance = 0.01) -> bool {
    return __builtin_fabs(a - b) < tolerance;
}
auto make_layout(ref<str> value, Option<Scalar> width = None()) -> TextLayout {
    auto service = fixed_service();
    auto result = service.layout({.text = value, .width = width});
    if (result.is_err())
        rstd::io::eprintln("layout failed error={} bytes={} text={}",
                           static_cast<int>(result.unwrap_err()), value->len(), value);
    return rstd::move(result).unwrap();
}
void basic_and_lifetime() {
    auto layout = make_layout("Hello"_str);
    ASSERT_TRUE((layout.glyphs().len() == usize(5)));
    ASSERT_TRUE((layout.lines().len() == usize(1)));
    ASSERT_TRUE((layout.logical_bounds().width > 0 && layout.logical_bounds().height > 0));
    ASSERT_TRUE((layout.missing_glyphs() == 0));
    ASSERT_TRUE((layout.caret(0).unwrap().bounds.x < layout.caret(5).unwrap().bounds.x));
    ASSERT_TRUE((layout.hit({-10, 0}).unwrap().byte_index == 0));
    ASSERT_TRUE((!layout.hit({-10, 0}).unwrap().inside));
    ASSERT_TRUE((layout.hit({1000, 0}).unwrap().byte_index == 5));
    ASSERT_TRUE((layout.caret(6).unwrap_err() == TextError::InvalidIndex));
    ASSERT_TRUE((layout.hit({__builtin_nan(""), 0}).unwrap_err() == TextError::InvalidValue));
    ASSERT_TRUE((layout.hit({2000000, 0}).unwrap().byte_index == 5));
    const auto &g = layout.glyphs()[usize(0)];
    auto face = FontInstance(layout.fonts()[usize(g.font)]);
    auto pixels = face.rasterize(g.glyph, 1).unwrap();
    ASSERT_TRUE((pixels.width > 0 && pixels.height > 0));
    ASSERT_TRUE((pixels.coverage.len() == usize(pixels.width) * usize(pixels.height)));
    bool coverage{};
    for (auto pixel : pixels.coverage)
        coverage |= pixel != 0;
    ASSERT_TRUE((coverage));
    ASSERT_TRUE((pixels.origin.y < 0 && pixels.scale == 1));
    auto larger = face.rasterize(g.glyph, 2).unwrap();
    ASSERT_TRUE((larger.width > pixels.width && larger.height > pixels.height));
    ASSERT_TRUE((near(larger.origin.y / 2, pixels.origin.y, 1.5)));
    ASSERT_TRUE((face.rasterize(g.glyph, 0).unwrap_err() == TextError::InvalidValue));
    ASSERT_TRUE((face.rasterize(g.glyph, 65).unwrap_err() == TextError::InvalidValue));
    auto clone = layout;
    ASSERT_TRUE((clone.id() == layout.id()));
    layout = make_layout("Replacement"_str);
    ASSERT_TRUE((clone.text() == "Hello"_str));
    ASSERT_TRUE((clone.caret(5).is_ok()));
    ASSERT_TRUE((face.rasterize(g.glyph, 1).is_ok()));
    auto service = fixed_service();
    auto a = service.layout({.text = "ABC"_str}).unwrap();
    auto b = service.layout({.text = "XYZ"_str}).unwrap();
    ASSERT_TRUE((a.id() != b.id()));
    ASSERT_TRUE((a.fonts()[usize(0)].id() == b.fonts()[usize(0)].id()));
    ASSERT_TRUE((a.glyphs()[usize(0)].glyph != b.glyphs()[usize(0)].glyph));
}
void lines_and_codepoints() {
    auto empty = make_layout(""_str);
    ASSERT_TRUE((empty.lines().len() == usize(1) && empty.glyphs().is_empty()));
    ASSERT_TRUE((empty.logical_bounds().height > 0 && empty.logical_bounds().width == 0));
    ASSERT_TRUE((empty.caret(0).is_ok()));
    auto newline = make_layout("\n\n"_str);
    ASSERT_TRUE((newline.lines().len() == usize(3)));
    for (Index i = 0; i < 3; ++i) {
        ASSERT_TRUE((newline.lines()[usize(i)].byte_start == i));
        ASSERT_TRUE((newline.lines()[usize(i)].byte_end == i));
        ASSERT_TRUE((newline.lines()[usize(i)].glyph_start == newline.lines()[usize(i)].glyph_end));
    }
    auto paragraphs = make_layout("a\n\nb\n"_str);
    ASSERT_TRUE((paragraphs.lines().len() == usize(4)));
    ASSERT_TRUE((paragraphs.lines()[usize(2)].byte_start == 3));
    ASSERT_TRUE((paragraphs.glyphs()[usize(1)].line == 2));
    auto combining = make_layout("e\u0301"_str);
    ASSERT_TRUE((combining.missing_glyphs() == 0));
    ASSERT_TRUE((combining.caret(1).is_ok()));
    ASSERT_TRUE((combining.caret(2).unwrap_err() == TextError::InvalidIndex));
    ASSERT_TRUE((combining.caret(3).is_ok()));
    ASSERT_TRUE((combining.glyphs().len() == usize(2)));
    ASSERT_TRUE((combining.glyphs()[usize()].byte_end == 1));
    ASSERT_TRUE((combining.glyphs()[usize(1)].byte_start == 1));
    auto wide = make_layout("alpha beta gamma delta"_str);
    auto narrow = make_layout("alpha beta gamma delta"_str, Some(Scalar(60)));
    ASSERT_TRUE((wide.lines().len() == usize(1)));
    ASSERT_TRUE((narrow.lines().len() > usize(1)));
    ASSERT_TRUE((narrow.logical_bounds().height > wide.logical_bounds().height));
    ASSERT_TRUE((narrow.logical_bounds().width <= 60));
    for (Index i = 0; i < narrow.lines().len().to_primitive(); ++i) {
        const auto &line = narrow.lines()[usize(i)];
        ASSERT_TRUE((line.glyph_start <= line.glyph_end));
        if (i)
            ASSERT_TRUE((line.baseline > narrow.lines()[usize(i - 1)].baseline));
        for (Index j = line.glyph_start; j < line.glyph_end; ++j) {
            const auto &g = narrow.glyphs()[usize(j)];
            ASSERT_TRUE(
                (g.line == i && g.byte_start >= line.byte_start && g.byte_end <= line.byte_end));
            ASSERT_TRUE((near(g.position.y, line.baseline)));
        }
    }
    auto service = fixed_service();
    auto nowrap = service
                      .layout({.text = "alpha beta gamma delta"_str,
                               .width = Some(Scalar(10)),
                               .wrap = TextWrap::None})
                      .unwrap();
    ASSERT_TRUE((nowrap.lines().len() == usize(1) &&
                 near(nowrap.logical_bounds().width, wide.logical_bounds().width)));
    auto unbroken =
        service
            .layout({.text = "abcdefghij"_str, .width = Some(Scalar(20)), .wrap = TextWrap::Word})
            .unwrap();
    auto broken = service.layout({.text = "abcdefghij"_str, .width = Some(Scalar(20))}).unwrap();
    ASSERT_TRUE((unbroken.lines().len() == usize(1) && broken.lines().len() > usize(1)));
}
void fallback() {
    auto mixed = make_layout("ABC 中文"_str);
    ASSERT_TRUE((mixed.missing_glyphs() == 0));
    ASSERT_TRUE((mixed.fonts().len() >= usize(2)));
    for (const auto &font : mixed.fonts()) {
        rstd::io::println("fallback: {}", font.name());
    }
    for (const auto &g : mixed.glyphs()) {
        ASSERT_TRUE(
            (g.byte_start < g.byte_end && g.byte_end <= mixed.text()->len().to_primitive()));
        if (g.drawable)
            ASSERT_TRUE((mixed.fonts()[usize(g.font)].rasterize(g.glyph, 1).is_ok()));
    }
    auto missing = make_layout("\xf4\x8f\xbf\xbd"_str);
    ASSERT_TRUE((missing.missing_glyphs() > 0));
    for (const auto &g : missing.glyphs()) {
        ASSERT_TRUE((g.missing && g.glyph == 0));
        ASSERT_TRUE((missing.fonts()[usize(g.font)].rasterize(g.glyph, 1).is_ok()));
    }
}
struct RetainedGlyph {
    FontInstance font;
    Index glyph;
};
auto retain_glyph() -> RetainedGlyph {
    auto layout = make_layout("A"_str);
    const auto &glyph = layout.glyphs()[usize(0)];
    return {FontInstance(layout.fonts()[usize(glyph.font)]), glyph.glyph};
}
void font_lifetime_and_eviction() {
    auto retained = retain_glyph();
    ASSERT_TRUE((retained.font.rasterize(retained.glyph, 1).is_ok()));
    auto service = fixed_service();
    auto old = service.layout({.text = "A"_str}).unwrap();
    auto id = old.fonts()[usize(0)].id();
    for (int size = 20; size < 280; ++size)
        ASSERT_TRUE((service.layout({.text = "X"_str, .size = Scalar(size)}).is_ok()));
    ASSERT_TRUE((old.fonts()[usize(0)].id() == id));
    ASSERT_TRUE((old.fonts()[usize(0)].rasterize(old.glyphs()[usize(0)].glyph, 1).is_ok()));
}
struct Concurrent {
    TextLayout layout;
    bool ok{true};
};
void query_and_raster(Concurrent &state) {
    for (int i = 0; i < 100; ++i) {
        state.ok &= state.layout.hit({5, 5}).is_ok() && state.layout.caret(0).is_ok();
        const auto &g = state.layout.glyphs()[usize(0)];
        state.ok &= state.layout.fonts()[usize(g.font)].rasterize(g.glyph, 1 + (i % 2)).is_ok();
    }
}
void concurrent_readers() {
    auto layout = make_layout("Threaded text"_str);
    Concurrent a{layout}, b{layout};
    auto spawned = rstd::thread::spawn([&a] { query_and_raster(a); });
    ASSERT_TRUE(spawned.is_ok());
    auto thread = rstd::move(spawned).unwrap();
    query_and_raster(b);
    ASSERT_TRUE(rstd::move(thread).join().is_ok());
    ASSERT_TRUE((a.ok && b.ok));
}
TEST(Vectext, BasicAndLifetime) { basic_and_lifetime(); }
TEST(Vectext, RangeBoxes) {
    auto service = fixed_service();
    auto layout = service.layout({.text = "a中 b\ncd"_str, .width = Some(120.0), .alignment = TextAlignment::Right}).unwrap();
    auto boxes = layout.range_boxes({1, 8}).unwrap();
    ASSERT_EQ(boxes.len(), usize(2));
    auto left = layout.caret(1).unwrap().bounds.x;
    auto right = layout.caret(6).unwrap().bounds.x;
    EXPECT_EQ(boxes[usize()].line, 0u);
    EXPECT_EQ(boxes[usize()].bounds, (Rect{left, 0, right - left, layout.lines()[usize()].logical.height}));
    EXPECT_EQ(boxes[usize()].baseline, layout.lines()[usize()].baseline);
    EXPECT_EQ(boxes[usize(1)].bounds.x, layout.caret(8).unwrap().bounds.x - layout.glyphs()[usize(4)].advance);
    EXPECT_EQ(boxes[usize(1)].bounds.width, layout.glyphs()[usize(4)].advance);
    EXPECT_TRUE(layout.range_boxes({6, 6}).unwrap().is_empty());
    EXPECT_TRUE(layout.range_boxes({6, 7}).unwrap().is_empty());
    EXPECT_EQ(layout.range_boxes({2, 4}).unwrap_err(), TextError::InvalidIndex);
    EXPECT_EQ(layout.range_boxes({4, 3}).unwrap_err(), TextError::InvalidIndex);
    EXPECT_EQ(layout.range_boxes({0, 20}).unwrap_err(), TextError::InvalidIndex);
    EXPECT_EQ(layout.range_boxes({9, 9}).unwrap().len(), usize());
}
TEST(Vectext, RangeBoxesWrapAndHidden) {
    auto service = fixed_service();
    TextRequest request{.text = "abc def ghi"_str, .width = Some(32.0)};
    auto layout = service.layout(request).unwrap();
    auto boxes = layout.range_boxes({0, 11}).unwrap();
    ASSERT_EQ(boxes.len(), layout.lines().len());
    for (usize i{}; i < boxes.len(); ++i) {
        EXPECT_EQ(boxes[i].line, i.to_primitive());
        EXPECT_TRUE(near(boxes[i].bounds.width, layout.lines()[i].logical.width));
        EXPECT_EQ(boxes[i].bounds.y, layout.lines()[i].logical.y);
    }
    request.max_lines = 1; request.overflow = TextOverflow::ThreeDots;
    auto clipped = service.layout(request).unwrap();
    ASSERT_TRUE(clipped.hidden_range().is_some());
    auto cut = clipped.hidden_range()->start;
    EXPECT_TRUE(clipped.range_boxes({cut, 11}).unwrap().is_empty());
    auto visible = clipped.range_boxes({0, 11}).unwrap();
    ASSERT_EQ(visible.len(), usize(1));
    EXPECT_TRUE(near(visible[usize()].bounds.width, clipped.caret(cut, TextAffinity::Upstream).unwrap().bounds.x));
    EXPECT_TRUE(visible[usize()].bounds.width < clipped.logical_bounds().width);
    EXPECT_TRUE(service.layout({.text = ""_str}).unwrap().range_boxes({0, 0}).unwrap().is_empty());
}
TEST(Vectext, LinesAndCodepoints) { lines_and_codepoints(); }
TEST(Vectext, InlineObjectPreparation) {
    auto text = String::make("aé\uFFFC\uFFFC"_str);
    TextInlineObject entries[]{{{3, 6}, 12, 10, 2, {1, -14, 10, 12}}, {{6, 9}, .5, .7, .2, {0, -.7, .5, .9}, TextInlineUnit::Em}};
    auto values = slice<TextInlineObject>::from_raw_parts(entries, usize(2));
    auto prepared = TextInlineObjects::create(text.as_str(), values).unwrap();
    auto clone = prepared; EXPECT_EQ(clone.objects().as_raw_ptr(), prepared.objects().as_raw_ptr());
    text = String::make("changed"_str); entries[0].advance = 30;
    EXPECT_TRUE(prepared.matches("aé\uFFFC\uFFFC"_str)); EXPECT_EQ(prepared.objects()[usize()].advance, 12.0);
    EXPECT_TRUE(prepared.overlaps({4, 6})); EXPECT_FALSE(prepared.overlaps({0, 3})); EXPECT_FALSE(prepared.overlaps({3, 3}));
    auto em = prepared.objects()[usize(1)].scaled(20).unwrap();
    EXPECT_EQ(em.advance, 10.0); EXPECT_EQ(em.ascent, 14.0); EXPECT_EQ(em.descent, 4.0); EXPECT_EQ(em.ink, (Rect{0, -14, 10, 18}));
    entries[0].range = {2, 5}; EXPECT_TRUE(TextInlineObjects::create("aé\uFFFC\uFFFC"_str, values).is_err());
    entries[0].range = {3, 6}; entries[1].range = {3, 6}; EXPECT_TRUE(TextInlineObjects::create("aé\uFFFC\uFFFC"_str, values).is_err());
    entries[1].range = {6, 10}; EXPECT_TRUE(TextInlineObjects::create("aé\uFFFC\uFFFC"_str, values).is_err());
    entries[1].range = {6, 9}; entries[0].ascent = -1; EXPECT_TRUE(TextInlineObjects::create("aé\uFFFC\uFFFC"_str, values).is_err());
    entries[0].ascent = 1; entries[0].advance = __builtin_inf(); EXPECT_TRUE(TextInlineObjects::create("aé\uFFFC\uFFFC"_str, values).is_err());
    entries[0].advance = 1; entries[0].ink.width = -1; EXPECT_TRUE(TextInlineObjects::create("aé\uFFFC\uFFFC"_str, values).is_err());
    entries[0].ink.width = 1; entries[0].unit = TextInlineUnit(99); EXPECT_TRUE(TextInlineObjects::create("aé\uFFFC\uFFFC"_str, values).is_err());
    EXPECT_TRUE(TextInlineObjects::create("anything"_str, {}).unwrap().matches("else"_str));
}
TEST(Vectext, InlineObjectLayoutAndQueries) {
    auto service = fixed_service(); TextInlineObject object{{1, 4}, 30, 25, 10, {-2, -31, 32, 35}};
    auto prepared = TextInlineObjects::create("A\uFFFCB"_str, slice<TextInlineObject>::from_raw_parts(&object, usize(1))).unwrap();
    auto layout = service.layout({.text = "A\uFFFCB"_str, .inline_objects = prepared}).unwrap();
    EXPECT_EQ(layout.inline_objects().objects().as_raw_ptr(), prepared.objects().as_raw_ptr());
    ASSERT_EQ(layout.inline_boxes().len(), usize(1)); EXPECT_EQ(layout.glyphs().len(), usize(2)); EXPECT_EQ(layout.runs().len(), usize(2));
    EXPECT_EQ(layout.missing_glyphs(), 0u);
    auto box = layout.inline_boxes()[usize()]; auto left = layout.caret(1).unwrap().bounds.x;
    EXPECT_EQ(box.logical, (Rect{left, 0, 30, 35})); EXPECT_EQ(box.ink, (Rect{left - 2, -6, 32, 35}));
    EXPECT_EQ(layout.baseline(), 25.0); EXPECT_EQ(layout.logical_bounds().height, 35.0); EXPECT_EQ(layout.ink_bounds().y, -6.0);
    EXPECT_TRUE(near(layout.caret(4).unwrap().bounds.x - left, 30));
    EXPECT_EQ(layout.hit({left + 1, 10}).unwrap().byte_index, 1u); EXPECT_EQ(layout.hit({left + 29, 10}).unwrap().byte_index, 4u);
    EXPECT_EQ(layout.caret(2).unwrap_err(), TextError::InvalidIndex);
    EXPECT_EQ(layout.range_boxes({1, 4}).unwrap()[usize()].bounds, box.logical);
    EXPECT_EQ(layout.range_boxes({0, 5}).unwrap().len(), usize(1));
    auto only = object; only.range = {0, 3};
    auto single = TextInlineObjects::create("\uFFFC"_str, slice<TextInlineObject>::from_raw_parts(&only, usize(1))).unwrap();
    auto image = service.layout({.text = "\uFFFC"_str, .inline_objects = single}).unwrap();
    EXPECT_TRUE(image.glyphs().is_empty()); EXPECT_TRUE(image.runs().is_empty()); EXPECT_EQ(image.missing_glyphs(), 0u);
    EXPECT_EQ(image.caret(3).unwrap().bounds.x, 30.0);
    EXPECT_TRUE(service.layout({.text = "different"_str, .inline_objects = prepared}).is_err());
}
TEST(Vectext, InlineObjectWrapAlignmentAndHidden) {
    auto service = fixed_service(); TextInlineObject object{{1, 4}, 30, 25, 10, {-2, -31, 32, 35}};
    auto prepared = TextInlineObjects::create("A\uFFFCB"_str, slice<TextInlineObject>::from_raw_parts(&object, usize(1))).unwrap();
    TextRequest request{.text = "A\uFFFCB"_str, .width = Some(31.0), .alignment = TextAlignment::Right, .inline_objects = prepared};
    auto layout = service.layout(request).unwrap();
    ASSERT_EQ(layout.lines().len(), usize(3)); ASSERT_EQ(layout.inline_boxes().len(), usize(1));
    auto box = layout.inline_boxes()[usize()]; EXPECT_EQ(box.line, 1u); EXPECT_EQ(box.logical.x, 1.0);
    EXPECT_EQ(box.logical.y, layout.lines()[usize(1)].logical.y);
    EXPECT_EQ(layout.caret(1).unwrap().bounds.x, 1.0);
    request.max_lines = 1; request.overflow = TextOverflow::ThreeDots;
    auto hidden = service.layout(request).unwrap(); EXPECT_TRUE(hidden.inline_boxes().is_empty());
    EXPECT_TRUE(hidden.range_boxes({1, 4}).unwrap().is_empty()); EXPECT_TRUE(hidden.hidden_range()->start <= 1);
    request.width = None(); request.max_lines = 0; request.alignment = TextAlignment::Center; request.text = "A\n\uFFFC"_str;
    object.range = {2, 5}; request.inline_objects = TextInlineObjects::create(request.text, slice<TextInlineObject>::from_raw_parts(&object, usize(1))).unwrap();
    auto centered = service.layout(request).unwrap(); EXPECT_EQ(centered.inline_boxes()[usize()].logical.x, 0.0);
    EXPECT_TRUE(centered.lines()[usize()].logical.x > 0);
}
TEST(Vectext, InlineObjectEmAndNormalizedBytes) {
    auto service = fixed_service(); TextInlineObject object{{3, 6}, .5, .8, .2, {0, -1, .5, 1.2}, TextInlineUnit::Em};
    auto prepared = TextInlineObjects::create("中\uFFFC"_str, slice<TextInlineObject>::from_raw_parts(&object, usize(1))).unwrap();
    TextRequest request{.text = "中\uFFFC"_str, .size = 20, .inline_objects = prepared};
    auto first = service.layout(request).unwrap(); request.size = 40; auto second = service.layout(request).unwrap();
    EXPECT_EQ(first.inline_boxes()[usize()].logical.width, 10.0); EXPECT_EQ(second.inline_boxes()[usize()].logical.width, 20.0);
    EXPECT_EQ(second.inline_boxes()[usize()].ink.height, first.inline_boxes()[usize()].ink.height * 2);
    EXPECT_EQ(second.inline_objects().objects().as_raw_ptr(), prepared.objects().as_raw_ptr());
    object.range = {3, 6}; auto normalized = TextInlineObjects::create("\uFFFD\uFFFC"_str, slice<TextInlineObject>::from_raw_parts(&object, usize(1))).unwrap();
    Vec<u8> bytes; bytes.push(u8(0xff)); bytes.push(u8(0xef)); bytes.push(u8(0xbf)); bytes.push(u8(0xbc));
    auto layout = service.layout({.bytes = Some(bytes.as_slice()), .invalid_text = InvalidText::Replace, .inline_objects = normalized}).unwrap();
    EXPECT_EQ(layout.text(), "\uFFFD\uFFFC"_str); EXPECT_EQ(layout.inline_boxes()[usize()].object, 0u);
    object.advance = 1048576; request.inline_objects = TextInlineObjects::create(request.text, slice<TextInlineObject>::from_raw_parts(&object, usize(1))).unwrap();
    EXPECT_EQ(service.layout(request).unwrap_err(), TextError::Limit);
}
TEST(Vectext, Fallback) { fallback(); }
TEST(Vectext, FontLifetimeAndEviction) { font_lifetime_and_eviction(); }
TEST(Vectext, ConcurrentReaders) { concurrent_readers(); }
} // namespace
