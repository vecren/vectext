#include <rstd/test/gtest.hpp>

import vectext;
import vectext.tests;
import rstd.test;
using namespace vectext;
using namespace vectext::tests;
using namespace rstd::prelude;
using namespace rstd::literals;

TEST(VectextPlatform, CollectionFacesAndOwnedInput) {
    auto collection = FontCollection::create({.system_fonts = false}).unwrap();
    FontSource regular, bold;
    {
        auto bytes = font_data("DejaVuFaces.ttc"_str);
        regular = collection.register_font(bytes.as_slice(), 0).unwrap();
        bold = collection.register_font(bytes.as_slice(), 1).unwrap();
        EXPECT_EQ(collection.register_font(bytes.as_slice(), 2).unwrap_err(), TextError::Font);
    }
    SystemTextService service(collection);
    auto a = service.layout({.text = "A"_str, .source = Some(regular)}).unwrap();
    auto b = service.layout({.text = "A"_str, .source = Some(bold)}).unwrap();
    EXPECT_TRUE(a.fonts()[usize()].id() != b.fonts()[usize()].id());
    EXPECT_TRUE(b.fonts()[usize()].name()->contains("Bold"_str));
    EXPECT_TRUE(b.fonts()[usize()].rasterize(b.glyphs()[usize()].glyph, 1).is_ok());
    collection.refresh().unwrap();
    EXPECT_TRUE(service.layout({.text = "B"_str, .source = Some(bold)}).is_ok());
}

TEST(VectextPlatform, SupplementaryCodepoint) {
    auto service = fixed_service();
    auto layout =
        service.layout({.text = "A\U0001d538B"_str, .family = "DejaVu Sans"_str}).unwrap();
    ASSERT_EQ(layout.glyphs().len(), usize(3));
    EXPECT_EQ(layout.missing_glyphs(), 0u);
    auto glyph = layout.glyphs()[usize(1)];
    EXPECT_EQ(glyph.byte_start, 1u);
    EXPECT_EQ(glyph.byte_end, 5u);
    EXPECT_TRUE(layout.caret(1).is_ok());
    EXPECT_EQ(layout.caret(2).unwrap_err(), TextError::InvalidIndex);
    EXPECT_TRUE(layout.caret(5).is_ok());
    auto bitmap = layout.fonts()[usize(glyph.font)].rasterize(glyph.glyph, 1).unwrap();
    EXPECT_TRUE(!bitmap.coverage.is_empty());
}

TEST(VectextPlatform, BitmapDirectionBaselineAndScale) {
    auto service = fixed_service();
    auto layout =
        service.layout({.text = "Fjg "_str, .family = "DejaVu Sans"_str, .size = 64}).unwrap();
    auto bounds = layout.logical_bounds();
    auto f = layout.glyphs()[usize()];
    auto &font = layout.fonts()[usize(f.font)];
    auto bitmap = font.rasterize(f.glyph, 1).unwrap();
    Serial top{}, bottom{};
    for (Index y = 0; y < bitmap.height / 4; ++y)
        for (Index x = 0; x < bitmap.width; ++x) {
            top += bitmap.coverage[usize(y) * usize(bitmap.width) + usize(x)];
            bottom +=
                bitmap.coverage[usize(bitmap.height - 1 - y) * usize(bitmap.width) + usize(x)];
        }
    EXPECT_TRUE(top > bottom * 2);
    EXPECT_TRUE(bitmap.origin.y < 0);
    auto j = layout.glyphs()[usize(1)];
    EXPECT_TRUE(layout.fonts()[usize(j.font)].rasterize(j.glyph, 1).unwrap().origin.x < 0);
    auto g = layout.glyphs()[usize(2)];
    auto descender = layout.fonts()[usize(g.font)].rasterize(g.glyph, 1).unwrap();
    EXPECT_TRUE(descender.origin.y + descender.height > 0);
    auto fractional = font.rasterize(f.glyph, 1.5).unwrap();
    EXPECT_TRUE(fractional.width > bitmap.width && fractional.height > bitmap.height);
    EXPECT_EQ(fractional.scale, 1.5);
    EXPECT_TRUE(__builtin_fabs(fractional.origin.y / 1.5 - bitmap.origin.y) <= 1);
    EXPECT_EQ(layout.logical_bounds(), bounds);
    EXPECT_EQ(font.rasterize(0xffffffffu, 1).unwrap_err(), TextError::InvalidIndex);
    auto space = layout.glyphs()[usize(3)];
    EXPECT_TRUE(space.advance > 0);
    EXPECT_TRUE(
        layout.fonts()[usize(space.font)].rasterize(space.glyph, 2).unwrap().coverage.is_empty());
}

TEST(VectextPlatform, ExplicitSourceCountsTowardFallbackLimit) {
    TextLimits limits;
    limits.fallback_attempts = 1;
    auto collection = FontCollection::create({.system_fonts = false, .limits = limits}).unwrap();
    auto latin = font_data("DejaVuSans.ttf"_str), cjk = font_data("NotoSansCJK-subset.ttf"_str);
    auto source = collection.register_font(latin.as_slice()).unwrap();
    collection.add_font(cjk.as_slice()).unwrap();
    SystemTextService service(collection);
    EXPECT_EQ(service.layout({.text = "中"_str, .source = Some(source)}).unwrap().missing_glyphs(),
              1u);
}

#if __is_target_os(macos)
TEST(VectextCoreText, ConfigurationAndSystemIsolation) {
    EXPECT_EQ(FontCollection::create({.config_file = "fonts.conf"_str}).unwrap_err(),
              TextError::Unsupported);
    auto isolated = FontCollection::create({.system_fonts = false}).unwrap();
    SystemTextService empty(isolated);
    EXPECT_EQ(empty.layout({.text = "A"_str}).unwrap_err(), TextError::Font);
    auto bytes = font_data("DejaVuFaces.ttc"_str);
    isolated.add_font(bytes.as_slice()).unwrap();
    EXPECT_EQ(empty.layout({.text = "中"_str, .family = "Helvetica"_str}).unwrap().missing_glyphs(),
              1u);
}

TEST(VectextCoreText, SystemCacheRefreshAndGenerics) {
    auto collection = FontCollection::create().unwrap();
    SystemTextService service(collection);
    auto old =
        service.layout({.text = "Hello 中文"_str, .family = "sans-serif"_str, .language = "zh"_str})
            .unwrap();
    EXPECT_EQ(old.missing_glyphs(), 0u);
    auto warm =
        service.layout({.text = "Hello 中文"_str, .family = "sans-serif"_str, .language = "zh"_str})
            .unwrap();
    EXPECT_EQ(old.fonts()[usize()].id(), warm.fonts()[usize()].id());
    EXPECT_EQ(collection.stats().font_bytes, 0u);
    collection.clear_cache();
    auto same =
        service.layout({.text = "Hello"_str, .family = "sans-serif"_str, .language = "zh"_str})
            .unwrap();
    EXPECT_EQ(old.fonts()[usize()].id(), same.fonts()[usize()].id());
    collection.refresh().unwrap();
    auto fresh =
        service.layout({.text = "Hello"_str, .family = "sans-serif"_str, .language = "zh"_str})
            .unwrap();
    EXPECT_TRUE(old.fonts()[usize()].id() != fresh.fonts()[usize()].id());
    EXPECT_TRUE(old.fonts()[usize()].rasterize(old.glyphs()[usize()].glyph, 1).is_ok());
    auto serif = service.layout({.text = "A"_str, .family = "serif"_str}).unwrap();
    auto mono = service.layout({.text = "iW"_str, .family = "monospace"_str}).unwrap();
    EXPECT_TRUE(serif.fonts()[usize()].name() != fresh.fonts()[usize()].name());
    EXPECT_TRUE(__builtin_fabs(mono.glyphs()[usize()].advance - mono.glyphs()[usize(1)].advance) <
                .01);
    auto bold = service.layout({.text = "A"_str, .family = "Helvetica"_str, .bold = true}).unwrap();
    EXPECT_TRUE(bold.fonts()[usize()].name()->contains("Bold"_str));
    auto italic = service.layout({.text = "A"_str, .family = "Times"_str, .italic = true}).unwrap();
    EXPECT_TRUE(italic.fonts()[usize()].name()->contains("Italic"_str));
}

TEST(VectextCoreText, SystemInstanceBudgetAndColorGlyph) {
    TextLimits limits;
    limits.font_instances = 1;
    auto collection = FontCollection::create({.limits = limits}).unwrap();
    SystemTextService service(collection);
    auto kept = service.layout({.text = "A"_str, .family = "Helvetica"_str}).unwrap();
    EXPECT_EQ(service.layout({.text = "中"_str, .family = "Helvetica"_str}).unwrap_err(),
              TextError::Limit);
    EXPECT_TRUE(service.layout({.text = "B"_str, .family = "Helvetica"_str}).is_ok());
    EXPECT_TRUE(kept.fonts()[usize()].rasterize(kept.glyphs()[usize()].glyph, 1).is_ok());
    SystemTextService color;
    auto emoji =
        color.layout({.text = "\U0001f600"_str, .family = "Apple Color Emoji"_str}).unwrap();
    ASSERT_EQ(emoji.missing_glyphs(), 0u);
    auto g = emoji.glyphs()[usize()];
    EXPECT_EQ(emoji.fonts()[usize(g.font)].rasterize(g.glyph, 1).unwrap_err(),
              TextError::Unsupported);
}
#endif
