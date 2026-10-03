#include <rstd/test/gtest.hpp>

import vectext;
import vectext.tests;
import rstd.test;
using namespace vectext::tests;
using namespace rstd::prelude;
using namespace rstd::literals;
using namespace vectext;
TEST(VectextLayout, CodepointsAndAffinity) {
    auto service = fixed_service();
    auto combined = service.layout({.text = "a\u0301b"_str, .width = Some(0.0)}).unwrap();
    EXPECT_EQ(combined.glyphs().len(), usize(3));
    EXPECT_TRUE(combined.caret(1).is_ok());
    EXPECT_EQ(combined.caret(2).unwrap_err(), TextError::InvalidIndex);
    const auto &line = combined.lines()[usize()];
    auto up = combined.caret(line.byte_end, TextAffinity::Upstream).unwrap().bounds;
    auto down = combined.caret(line.byte_end).unwrap().bounds;
    EXPECT_TRUE(up.y < down.y);
    auto hit = combined.hit({up.x, up.y + 1}).unwrap();
    EXPECT_EQ(hit.byte_index, line.byte_end);
    EXPECT_EQ(hit.affinity, TextAffinity::Upstream);
    auto plain = service.layout({.text = "ffi"_str}).unwrap();
    EXPECT_EQ(plain.glyphs().len(), usize(3));
    for (Index i = 0; i <= 3; ++i)
        EXPECT_TRUE(plain.caret(i).is_ok());
    auto broken = service.layout({.text = "ffi"_str, .width = Some(1.0)}).unwrap();
    EXPECT_EQ(broken.lines().len(), usize(3));
}
TEST(VectextLayout, ExplicitBreaksAndTabs) {
    auto service = fixed_service();
    auto lines = service.layout({.text = "a\r\nb\rc\n"_str}).unwrap();
    EXPECT_EQ(lines.lines().len(), usize(4));
    EXPECT_EQ(lines.lines()[usize(1)].byte_start, 3u);
    EXPECT_EQ(lines.lines()[usize(3)].byte_start, 7u);
    for (Index i = 0; i <= 7; ++i)
        EXPECT_TRUE(lines.caret(i).is_ok());
    auto spaces = service.layout({.text = "        "_str}).unwrap();
    auto tabs = service.layout({.text = "a\tb"_str}).unwrap();
    EXPECT_EQ(tabs.missing_glyphs(), 0u);
    EXPECT_EQ(tabs.glyphs()[usize(2)].position.x, spaces.logical_bounds().width);
    EXPECT_FALSE(tabs.glyphs()[usize(1)].drawable);
}
TEST(VectextLayout, AlignmentAndDirection) {
    auto service = fixed_service();
    auto center = service
                      .layout({.text = "abc\nx"_str,
                               .width = Some(100.0),
                               .alignment = TextAlignment::Center,
                               .line_spacing = 4})
                      .unwrap();
    EXPECT_TRUE(center.lines()[usize(1)].logical.x > center.lines()[usize()].logical.x);
    EXPECT_EQ(center.lines()[usize(1)].logical.y, center.lines()[usize()].logical.height + 4);
    auto gap =
        center.hit({center.lines()[usize()].logical.x, center.lines()[usize()].logical.height + 1})
            .unwrap();
    EXPECT_EQ(gap.byte_index, 0u);
    EXPECT_FALSE(gap.inside);
    EXPECT_EQ(
        service.layout({.text = "abc"_str, .direction = TextDirection::RightToLeft}).unwrap_err(),
        TextError::Unsupported);
    auto left =
        service.layout({.text = "abc"_str, .direction = TextDirection::LeftToRight}).unwrap();
    auto automatic = service.layout({.text = "abc"_str}).unwrap();
    EXPECT_EQ(left.logical_bounds(), automatic.logical_bounds());
    auto natural = service.layout({.text = "a"_str, .width = Some(100.0)}).unwrap();
    auto left_box =
        service.layout({.text = "a"_str, .width = Some(100.0), .alignment = TextAlignment::Left})
            .unwrap();
    EXPECT_TRUE(natural.logical_bounds().width < 100);
    EXPECT_EQ(left_box.logical_bounds().width, 100.0);
    auto end =
        service.layout({.text = "a\n"_str, .width = Some(100.0), .alignment = TextAlignment::End})
            .unwrap();
    EXPECT_EQ(end.caret(2).unwrap().bounds.x, 100.0);
}
TEST(VectextLayout, LimitedBreakRules) {
    auto service = fixed_service();
    auto word =
        service.layout({.text = "abcdefghij"_str, .width = Some(20.0), .wrap = TextWrap::Word})
            .unwrap();
    EXPECT_EQ(word.lines().len(), usize(1));
    EXPECT_TRUE(word.logical_bounds().width > 20);
    auto characters = service.layout({.text = "中文中文"_str, .width = Some(20.0)}).unwrap();
    EXPECT_EQ(characters.lines().len(), usize(4));
    auto hyphen =
        service.layout({.text = "a-bbbb"_str, .width = Some(24.0), .wrap = TextWrap::Word})
            .unwrap();
    EXPECT_EQ(hyphen.lines().len(), usize(2));
    EXPECT_EQ(hyphen.lines()[usize()].byte_end, 2u);
    auto zero = service.layout({.text = "abc"_str, .width = Some(0.0)}).unwrap();
    EXPECT_EQ(zero.lines().len(), usize(3));
}
TEST(VectextLayout, LimitsAndLongParagraph) {
    TextLimits limits;
    limits.text_bytes = 8;
    auto collection = FontCollection::create({.system_fonts = false, .limits = limits}).unwrap();
    auto bytes = font_data("DejaVuSans.ttf"_str);
    collection.add_font(bytes.as_slice()).unwrap();
    SystemTextService limited(collection);
    EXPECT_EQ(limited.layout({.text = "123456789"_str}).unwrap_err(), TextError::Limit);
    auto service = fixed_service();
    String text;
    for (unsigned i = 0; i < 2000; ++i)
        text.push_str("abc "_str);
    auto layout = service.layout({.text = text.as_str(), .width = Some(24.0)}).unwrap();
    EXPECT_TRUE(layout.lines().len() > usize(1000));
    EXPECT_EQ(layout.glyphs().len(), usize(8000));
    EXPECT_TRUE(layout.caret(8000).is_ok());
    auto &g = layout.glyphs()[usize()];
    EXPECT_EQ(layout.fonts()[usize(g.font)].rasterize(0xffffffff, 1).unwrap_err(),
              TextError::InvalidIndex);
}
TEST(VectextLayout, QueryRoundTrip) {
    auto service = fixed_service();
    const ref<str> pieces[]{"a"_str, "中"_str, "ffi"_str, " "_str, "\t"_str, "\n"_str, "(x)"_str};
    unsigned random = 17;
    for (unsigned sample = 0; sample < 40; ++sample) {
        String text;
        for (unsigned i = 0; i < 20; ++i) {
            random = random * 1664525 + 1013904223;
            text.push_str(pieces[(random >> 16) % 7]);
        }
        auto result = service.layout({.text = text.as_str(), .width = Some(double(sample % 60))});
        ASSERT_TRUE(result.is_ok());
        auto layout = rstd::move(result).unwrap();
        for (auto &line : layout.lines()) {
            auto hit = layout
                           .hit({line.logical.x + line.logical.width / 2,
                                 line.logical.y + line.logical.height / 2})
                           .unwrap();
            auto caret = layout.caret(hit.byte_index, hit.affinity);
            EXPECT_TRUE(caret.is_ok());
            EXPECT_EQ(caret.unwrap().bounds.y, line.logical.y);
        }
    }
}
