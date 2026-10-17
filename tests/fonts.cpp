#include <rstd/test/gtest.hpp>

import vectext;
import vectext.tests;
import rstd.test;
using namespace vectext::tests;
using namespace rstd::prelude;
using namespace rstd::literals;
using namespace vectext;
TEST(VectextFonts, FontSpanOwnershipAndValidation) {
    auto text=String::make("A中文B"_str);
    TextFontSpan input[]{{{1,4},Some(true),{}},{{4,7},Some(true),{}}};
    auto spans=TextFontSpans::create(text.as_str(),slice<TextFontSpan>::from_raw_parts(input,usize(2))).unwrap();
    text=String(); input[0].bold=Some(false);
    EXPECT_TRUE(spans.matches("A中文B"_str)); EXPECT_EQ(spans.spans().len(),usize(1));
    EXPECT_EQ(spans.spans()[usize()].range.end,7u);
    EXPECT_TRUE((spans.style(1,{false,true})==TextFontStyle{true,true}));
    EXPECT_TRUE((spans.style(7,{false,true})==TextFontStyle{false,true}));
    auto copy=spans; EXPECT_TRUE(copy==spans);
    TextFontSpan bad[]{{{2,4},Some(true),{}},{{1,9},Some(true),{}},{{1,1},Some(true),{}},{{1,4},{},{}}};
    for (auto span:bad) EXPECT_EQ(TextFontSpans::create("A中文B"_str,slice<TextFontSpan>::from_raw_parts(&span,usize(1))).unwrap_err(),TextError::InvalidValue);
    TextFontSpan overlap[]{{{1,7},Some(true),{}},{{4,7},Some(false),{}}};
    EXPECT_EQ(TextFontSpans::create("A中文B"_str,slice<TextFontSpan>::from_raw_parts(overlap,usize(2))).unwrap_err(),TextError::InvalidValue);
    TextFontSpan regular{{0,1},Some(false),{}};
    auto normal=TextFontSpans::create("A"_str,slice<TextFontSpan>::from_raw_parts(&regular,usize(1))).unwrap();
    EXPECT_FALSE(normal.style(0,{true,false}).bold);
}
TEST(VectextFonts, FontSpansSelectNativeFacesAndRestore) {
    auto service=fixed_service(); TextRequest request{.text="ABCABCABC"_str,.family="DejaVu Sans"_str,.size=24};
    auto normal=service.layout(request).unwrap(); request.bold=true;
    auto bold=service.layout(request).unwrap(); request.bold=false;
    TextFontSpan span{{3,6},Some(true),{}}; request.font_spans=TextFontSpans::create(request.text,slice<TextFontSpan>::from_raw_parts(&span,usize(1))).unwrap();
    auto mixed=service.layout(request).unwrap(); EXPECT_TRUE(mixed.font_spans()==request.font_spans);
    for (usize i{};i<usize(9);++i) {
        auto &glyph=mixed.glyphs()[i]; auto &expected=(i>=usize(3)&&i<usize(6)?bold:normal).glyphs()[i];
        EXPECT_EQ(mixed.fonts()[usize(glyph.font)].id(),(i>=usize(3)&&i<usize(6)?bold:normal).fonts()[usize(expected.font)].id());
        EXPECT_EQ(glyph.advance,expected.advance);
        EXPECT_TRUE(mixed.caret(glyph.byte_start).is_ok());
    }
    EXPECT_EQ(mixed.runs().len(),usize(3));
    EXPECT_TRUE(normal.fonts()[usize()].id()!=bold.fonts()[usize()].id());
    EXPECT_EQ(mixed.caret(6).unwrap().bounds.x,mixed.glyphs()[usize(6)].position.x);
    request.text="mismatch"_str; EXPECT_EQ(service.layout(request).unwrap_err(),TextError::InvalidValue);
}
TEST(VectextFonts, FontSpansFixedSourceSynthesisAndCache) {
    auto collection=FontCollection::create({.system_fonts=false}).unwrap(); auto bytes=font_data("DejaVuSans.ttf"_str);
    auto source=collection.register_font(bytes.as_slice()).unwrap(); SystemTextService service(collection);
    TextRequest request{.text="WWW"_str,.size=24,.source=Some(source)};
    auto normal=service.layout(request).unwrap(); request.bold=true; auto bold=service.layout(request).unwrap();
    auto &a=normal.fonts()[usize()], &b=bold.fonts()[usize()];
    EXPECT_TRUE(a.id()!=b.id()); EXPECT_EQ(a.name(),b.name());
    EXPECT_TRUE(bold.glyphs()[usize()].advance>normal.glyphs()[usize()].advance);
    Scalar scales[]{1,2};
    for (auto scale:scales) {
        auto plain=a.rasterize(normal.glyphs()[usize()].glyph,scale).unwrap(), strong=b.rasterize(bold.glyphs()[usize()].glyph,scale).unwrap();
        EXPECT_TRUE(strong.width>plain.width && strong.height>plain.height); unsigned long total{},original{};
        for (auto value:strong.coverage) total+=value; for (auto value:plain.coverage) original+=value;
        EXPECT_TRUE(total>original); EXPECT_EQ(plain.origin,strong.origin);
    }
    collection.clear_cache(); auto again=service.layout(request).unwrap(); EXPECT_EQ(again.fonts()[usize()].id(),b.id());
    EXPECT_TRUE(a.rasterize(normal.glyphs()[usize()].glyph,1).is_ok());
    request.bold=false; TextFontSpan span{{1,2},Some(true),{}};
    request.font_spans=TextFontSpans::create(request.text,slice<TextFontSpan>::from_raw_parts(&span,usize(1))).unwrap();
    auto mixed=service.layout(request).unwrap();
    EXPECT_EQ(mixed.fonts()[usize(mixed.glyphs()[usize(1)].font)].id(),b.id());
    EXPECT_EQ(mixed.fonts()[usize(mixed.glyphs()[usize(2)].font)].id(),a.id());
    auto bold_bytes=font_data("DejaVuSans-Bold.ttf"_str); request.source=Some(collection.register_font(bold_bytes.as_slice()).unwrap());
    request.font_spans={}; auto native=service.layout(request).unwrap(); request.bold=true; auto native_again=service.layout(request).unwrap();
    EXPECT_EQ(native.fonts()[usize()].id(),native_again.fonts()[usize()].id());
}
TEST(VectextFonts, FontSpansReflowInlineObjectsAndNormalizedBytes) {
    auto service=fixed_service(); TextRequest request{.text="iiiiiiii"_str,.family="DejaVu Sans"_str,.size=24};
    auto normal=service.layout(request).unwrap(); request.width=Some(normal.logical_bounds().width+.01);
    TextFontSpan span{{2,6},Some(true),{}};
    request.font_spans=TextFontSpans::create(request.text,slice<TextFontSpan>::from_raw_parts(&span,usize(1))).unwrap();
    auto wrapped=service.layout(request).unwrap(); EXPECT_TRUE(wrapped.lines().len()>usize(1));
    for (Index i{};i<=8;++i) EXPECT_TRUE(wrapped.caret(i).is_ok());
    request.max_lines=1;request.overflow=TextOverflow::ThreeDots;
    auto clipped=service.layout(request).unwrap(); EXPECT_TRUE(clipped.hidden_range().is_some());
    EXPECT_TRUE(clipped.font_spans()==request.font_spans);
    request={.text="W\uFFFC中W"_str,.family="DejaVu Sans"_str,.size=24}; span.range={0,8};
    request.font_spans=TextFontSpans::create(request.text,slice<TextFontSpan>::from_raw_parts(&span,usize(1))).unwrap();
    TextInlineObject object{{1,4},1,1,0,{0,-1,1,1},TextInlineUnit::Em};
    request.inline_objects=TextInlineObjects::create(request.text,slice<TextInlineObject>::from_raw_parts(&object,usize(1))).unwrap();
    auto inline_layout=service.layout(request).unwrap(); EXPECT_EQ(inline_layout.inline_boxes().len(),usize(1));
    EXPECT_EQ(inline_layout.inline_boxes()[usize()].logical.width,24.); EXPECT_EQ(inline_layout.glyphs().len(),usize(3));
    EXPECT_EQ(inline_layout.missing_glyphs(),0u);
    request={.text=""_str,.family="DejaVu Sans"_str};
    Vec<u8> bytes;bytes.push(u8('A'));bytes.push(u8(0xff));bytes.push(u8('B')); request.bytes=Some(bytes.as_slice());request.invalid_text=InvalidText::Replace;
    span.range={1,4}; request.font_spans=TextFontSpans::create("A\uFFFDB"_str,slice<TextFontSpan>::from_raw_parts(&span,usize(1))).unwrap();
    auto normalized=service.layout(request).unwrap(); EXPECT_EQ(normalized.text(),"A\uFFFDB"_str); EXPECT_TRUE(normalized.font_spans()==request.font_spans);
}
TEST(VectextFonts, MemoryIdentityAndRefresh) {
    auto collection = fixed_collection();
    auto before = collection.revision();
    SystemTextService a(collection), b(collection);
    auto old = a.layout({.text = "Hello"_str, .family = "DejaVu Sans"_str}).unwrap();
    auto again = b.layout({.text = "World"_str, .family = "DejaVu Sans"_str}).unwrap();
    EXPECT_EQ(old.fonts()[usize()].id(), again.fonts()[usize()].id());
    collection.clear_cache();
    auto warm = b.layout({.text = "World"_str, .family = "DejaVu Sans"_str}).unwrap();
    EXPECT_EQ(old.fonts()[usize()].id(), warm.fonts()[usize()].id());
    collection.refresh().unwrap();
    EXPECT_TRUE(collection.revision() > before);
    auto fresh = a.layout({.text = "Hello"_str, .family = "DejaVu Sans"_str}).unwrap();
    EXPECT_TRUE(fresh.fonts()[usize()].id() != old.fonts()[usize()].id());
    EXPECT_EQ(old.revision(), before);
    EXPECT_TRUE(old.caret(5).is_ok());
    auto &g = old.glyphs()[usize()];
    EXPECT_TRUE(old.fonts()[usize(g.font)].rasterize(g.glyph, 1).is_ok());
}
TEST(VectextFonts, ErrorsAndStyles) {
    auto collection = FontCollection::create({.system_fonts = false}).unwrap();
    unsigned char corrupt[]{1, 2, 3, 4};
    EXPECT_EQ(
        collection.add_font(slice<unsigned char>::from_raw_parts(corrupt, usize(4))).unwrap_err(),
        TextError::Font);
    auto bytes = font_data("DejaVuSans.ttf"_str);
    EXPECT_EQ(collection.add_font(bytes.as_slice(), 1).unwrap_err(), TextError::Font);
    collection.add_font(bytes.as_slice()).unwrap();
    auto bold = font_data("DejaVuSans-Bold.ttf"_str);
    collection.add_font(bold.as_slice()).unwrap();
    SystemTextService service(collection);
    auto regular = service.layout({.text = "A"_str, .family = "DejaVu Sans"_str}).unwrap();
    auto heavy =
        service.layout({.text = "A"_str, .family = "DejaVu Sans"_str, .bold = true}).unwrap();
    EXPECT_TRUE(regular.fonts()[usize()].id() != heavy.fonts()[usize()].id());
    EXPECT_TRUE(heavy.fonts()[usize()].name()->contains("Bold"_str));
    EXPECT_EQ(service.layout({.text = "a\0b"_str}).unwrap_err(), TextError::InvalidUtf8);
    EXPECT_EQ(service.layout({.size = -1}).unwrap_err(), TextError::InvalidValue);
    EXPECT_EQ(service.layout({.size = 0.001}).unwrap_err(), TextError::InvalidValue);
    EXPECT_EQ(service.layout({.direction = static_cast<TextDirection>(99)}).unwrap_err(),
              TextError::InvalidValue);
    EXPECT_EQ(service.layout({.width = Some(-1.0)}).unwrap_err(), TextError::InvalidValue);
    EXPECT_EQ(service.layout({.line_spacing = -1}).unwrap_err(), TextError::InvalidValue);
}
TEST(VectextFonts, SystemSmoke) {
    SystemTextService service;
    auto value = service.layout({.text = "Hello 中文"_str});
    ASSERT_TRUE(value.is_ok());
    EXPECT_TRUE(value.unwrap().logical_bounds().width > 0);
}
#if !__is_target_os(macos) && !__is_target_os(windows)
TEST(VectextFonts, IsolatedFontconfig) {
    auto collection = isolated_collection();
    SystemTextService service(collection);
    auto layout = service.layout({.text = "ABC 中文"_str, .family = "DejaVu Sans"_str}).unwrap();
    EXPECT_EQ(layout.missing_glyphs(), 0u);
    EXPECT_EQ(layout.fonts().len(), usize(2));
    EXPECT_TRUE(collection.stats().font_bytes > 0);
}
#endif
TEST(VectextFonts, Budgets) {
    TextLimits limits;
    limits.font_instances = 1;
    auto collection = FontCollection::create({.system_fonts = false, .limits = limits}).unwrap();
    auto bytes = font_data("DejaVuSans.ttf"_str);
    collection.add_font(bytes.as_slice()).unwrap();
    SystemTextService service(collection);
    auto retained = service.layout({.text = "A"_str}).unwrap();
    EXPECT_EQ(service.layout({.text = "B"_str, .size = 20}).unwrap_err(), TextError::Limit);
    collection.clear_cache();
    EXPECT_EQ(service.layout({.text = "B"_str, .size = 20}).unwrap_err(), TextError::Limit);
    EXPECT_TRUE(service.layout({.text = "B"_str}).is_ok());
    EXPECT_EQ(
        retained.fonts()[usize()].rasterize(retained.glyphs()[usize()].glyph, 0.0).unwrap_err(),
        TextError::InvalidValue);
    limits.font_bytes = 100;
    auto small = FontCollection::create({.system_fonts = false, .limits = limits}).unwrap();
    EXPECT_EQ(small.add_font(bytes.as_slice()).unwrap_err(), TextError::Limit);
    EXPECT_EQ(small.stats().font_bytes, 0u);
}
TEST(VectextFonts, RasterBudgetAndMetrics) {
    TextLimits limits;
    limits.bitmap_extent = 32;
    limits.bitmap_bytes = 1024;
    auto collection = FontCollection::create({.system_fonts = false, .limits = limits}).unwrap();
    auto bytes = font_data("DejaVuSans.ttf"_str);
    collection.add_font(bytes.as_slice()).unwrap();
    SystemTextService service(collection);
    auto layout = service.layout({.text = "j "_str, .family = "DejaVu Sans"_str}).unwrap();
    auto bounds = layout.logical_bounds();
    auto &g = layout.glyphs()[usize()];
    auto &font = layout.fonts()[usize(g.font)];
    EXPECT_TRUE(font.rasterize(g.glyph, 1).unwrap().origin.x < 0);
    EXPECT_EQ(font.rasterize(g.glyph, 3).unwrap_err(), TextError::Limit);
    EXPECT_EQ(layout.logical_bounds(), bounds);
    auto space = layout.glyphs()[usize(1)];
    auto empty = font.rasterize(space.glyph, 1).unwrap();
    EXPECT_TRUE(empty.coverage.is_empty());
    EXPECT_TRUE(space.advance > 0);
}
TEST(VectextFonts, ColorOnlyGlyphIsExplicitlyUnsupported) {
    auto collection = FontCollection::create({.system_fonts = false}).unwrap();
    auto bytes = font_data("ColorLayers.ttf"_str);
    collection.add_font(bytes.as_slice()).unwrap();
    SystemTextService service(collection);
    auto layout = service.layout({.text = "A"_str}).unwrap();
    const auto &glyph = layout.glyphs()[usize()];
    EXPECT_EQ(layout.fonts()[usize(glyph.font)].rasterize(glyph.glyph, 1).unwrap_err(),
              TextError::Unsupported);
}
#if !__is_target_os(macos) && !__is_target_os(windows)
TEST(VectextFonts, FileSourcesRefreshAndBudgetLifetime) {
    auto collection = isolated_collection();
    SystemTextService service(collection);
    {
        auto old = service.layout({.text = "A"_str, .family = "DejaVu Sans"_str}).unwrap();
        auto bytes = collection.stats().font_bytes;
        EXPECT_TRUE(bytes > 0);
        collection.clear_cache();
        EXPECT_EQ(collection.stats().font_bytes, bytes);
        auto same = service.layout({.text = "B"_str, .family = "DejaVu Sans"_str}).unwrap();
        EXPECT_EQ(same.fonts()[usize()].id(), old.fonts()[usize()].id());
        collection.refresh().unwrap();
        auto fresh = service.layout({.text = "C"_str, .family = "DejaVu Sans"_str}).unwrap();
        EXPECT_TRUE(fresh.fonts()[usize()].id() != old.fonts()[usize()].id());
        EXPECT_EQ(collection.stats().font_bytes, bytes * 2);
        EXPECT_TRUE(old.fonts()[usize()].rasterize(old.glyphs()[usize()].glyph, 1).is_ok());
    }
    collection.clear_cache();
    EXPECT_EQ(collection.stats().font_bytes, 0u);
    EXPECT_EQ(collection.stats().instances, 0u);
}
#endif
TEST(VectextFonts, GlyphBudgetAndFallbackLimit) {
    TextLimits limits;
    limits.glyphs = 1;
    limits.fallback_attempts = 1;
    auto collection = FontCollection::create({.system_fonts = false, .limits = limits}).unwrap();
    auto latin = font_data("DejaVuSans.ttf"_str), cjk = font_data("NotoSansCJK-subset.ttf"_str);
    collection.add_font(latin.as_slice()).unwrap();
    collection.add_font(cjk.as_slice()).unwrap();
    SystemTextService service(collection);
    EXPECT_EQ(service.layout({.text = "ab"_str}).unwrap_err(), TextError::Limit);
    EXPECT_TRUE(service.layout({.text = "\nA\n"_str}).is_ok());
    auto missing = service.layout({.text = "中"_str, .family = "DejaVu Sans"_str}).unwrap();
    EXPECT_EQ(missing.missing_glyphs(), 1u);
}
TEST(VectextFonts, ExplicitSourceSurvivesRefresh) {
    auto collection = fixed_collection();
    auto bytes = font_data("DejaVuSans-Bold.ttf"_str);
    auto source = collection.register_font(bytes.as_slice()).unwrap();
    SystemTextService service(collection);
    auto selected = service.layout({.text = "A中"_str, .source = Some(source)}).unwrap();
    EXPECT_TRUE(selected.fonts()[usize()].name()->contains("Bold"_str));
    EXPECT_EQ(selected.missing_glyphs(), 0u);
    collection.refresh().unwrap();
    EXPECT_TRUE(service.layout({.text = "A"_str, .source = Some(source)}).is_ok());
    auto other = fixed_service();
    EXPECT_EQ(other.layout({.text = "A"_str, .source = Some(source)}).unwrap_err(),
              TextError::InvalidValue);
}
