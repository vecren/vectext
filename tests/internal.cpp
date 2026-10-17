module;
#include <rstd/test/gtest.hpp>
module vectext;
import :font.raster;
import :layout.build;
import :font.synthesis;
#if !__is_target_os(macos) && !__is_target_os(windows)
import vectext.ffi.freetype;
import :platform.freetype;
#endif
import rstd.test;
using namespace vectext;
using namespace rstd::prelude;
TEST(VectextInternal, SyntheticBoldCoverageAndLimits) {
    GlyphBitmap input{2,1,{},1,{}}; input.coverage.push(100);input.coverage.push(200);
    auto output=embolden_bitmap(rstd::move(input),.5,{}).unwrap();
    EXPECT_EQ(output.width,3u); EXPECT_EQ(output.height,2u);
    unsigned char expected[]{100,200,100,50,100,50};
    for (usize i{};i<usize(6);++i) EXPECT_EQ(output.coverage[i],expected[i.to_primitive()]);
    GlyphBitmap limited{2,1,{},1,{}}; limited.coverage.push(100);limited.coverage.push(200); TextLimits limits;limits.bitmap_bytes=5;
    EXPECT_EQ(embolden_bitmap(rstd::move(limited),.5,limits).unwrap_err(),TextError::Limit);
}
TEST(VectextInternal, SyntheticBoldMatchesCoverageEnvelope) {
    Scalar amounts[]{.25,1,1.75,4};
    for (auto amount:amounts) {
        GlyphBitmap input{3,4,{},1,{}};
        for (unsigned i{};i<12;++i) input.coverage.push(static_cast<unsigned char>((i*71+31)%256));
        auto original=Vec<unsigned char>::from(input.coverage.as_slice());auto output=embolden_bitmap(rstd::move(input),amount,{}).unwrap();
        for (Index y{};y<output.height;++y) for (Index x{};x<output.width;++x) {
            unsigned expected{};
            for (Index py{};py<4;++py) for (Index px{};px<3;++px) {
                if (x<px || y<py || x-px>__builtin_ceil(amount) || y-py>__builtin_ceil(amount)) continue;
                auto coverage=unsigned(original[usize(py*3+px)]);
                if (x-px>__builtin_floor(amount)) coverage=static_cast<unsigned>(coverage*(amount-__builtin_floor(amount))+.5);
                if (y-py>__builtin_floor(amount)) coverage=static_cast<unsigned>(coverage*(amount-__builtin_floor(amount))+.5);
                if (coverage>expected)expected=coverage;
            }
            EXPECT_EQ(output.coverage[usize(y*output.width+x)],expected);
        }
    }
}
#if !__is_target_os(macos) && !__is_target_os(windows)
namespace ft = vectext::ffi::freetype;
using vectext::freetype::bitmap_coverage;
TEST(VectextInternal, BitmapConversion) {
    unsigned char data[]{0, 1, 2, 3};
    ft::FT_Bitmap bitmap{};
    bitmap.width = 2;
    bitmap.rows = 2;
    bitmap.pitch = -2;
    bitmap.buffer = data;
    bitmap.pixel_mode = ft::FT_PIXEL_MODE_GRAY;
    bitmap.num_grays = 4;
    auto pixels = bitmap_coverage(bitmap, {}).unwrap();
    EXPECT_EQ(pixels[usize()], 170);
    EXPECT_EQ(pixels[usize(1)], 255);
    EXPECT_EQ(pixels[usize(2)], 0);
    bitmap.pitch = 2;
    EXPECT_EQ(bitmap_coverage(bitmap, {}).unwrap()[usize(1)], 85);
    unsigned char mono[]{0x80, 0x40};
    bitmap.buffer = mono;
    bitmap.pixel_mode = ft::FT_PIXEL_MODE_MONO;
    bitmap.pitch = 1;
    auto bits = bitmap_coverage(bitmap, {}).unwrap();
    EXPECT_EQ(bits[usize()], 255);
    EXPECT_EQ(bits[usize(1)], 0);
    EXPECT_EQ(bits[usize(3)], 255);
    bitmap.pixel_mode = ft::FT_PIXEL_MODE_BGRA;
    EXPECT_EQ(bitmap_coverage(bitmap, {}).unwrap_err(), TextError::Unsupported);
    bitmap.pixel_mode = ft::FT_PIXEL_MODE_GRAY;
    EXPECT_EQ(bitmap_coverage(bitmap, {}).unwrap_err(), TextError::Raster);
    TextLimits limits;
    limits.bitmap_bytes = 3;
    EXPECT_EQ(bitmap_coverage(bitmap, limits).unwrap_err(), TextError::Limit);
}
#endif
TEST(VectextInternal, RejectInvalidUtf8) {
    const unsigned char invalid[][4]{
        {0xc0, 0xaf, 0, 0}, {0xed, 0xa0, 0x80, 0}, {0xf4, 0x90, 0x80, 0x80}, {0xe4, 0xb8, 0, 0}};
    const unsigned lengths[]{2, 3, 4, 2};
    for (unsigned i = 0; i < 4; ++i) {
        auto bytes = slice<u8>::from_raw_parts(reinterpret_cast<const rstd::byte *>(invalid[i]),
                                               usize(lengths[i]));
        EXPECT_EQ(validate_text(bytes).unwrap_err(), TextError::InvalidUtf8);
    }
}

#if __is_target_os(windows)
TEST(VectextDirectWriteInternal, FallbackScaleAndCache) {
    using namespace rstd::literals;
    Collection collection(true, String(), {});
    collection.config = collection.make_config().unwrap();
    TextRequest request{.family = "Segoe UI"_str, .size = 32};
    auto choices = candidates(collection, request).unwrap();
    auto normal = candidate_font(collection, choices[usize()], request.size).unwrap();
    Candidate scaled{directwrite::dw::retain(choices[usize()].pattern.get()), {}, {}, 1.5};
    auto large = candidate_font(collection, scaled, request.size).unwrap();
    EXPECT_TRUE(normal->id != large->id);
    EXPECT_EQ(large->size, 48.0);
    EXPECT_TRUE(__builtin_fabs(large->ascent / normal->ascent - 1.5) < .0001);
    auto glyph = normal->glyph('F');
    auto a = normal->measure(glyph).unwrap(), b = large->measure(glyph).unwrap();
    EXPECT_TRUE(__builtin_fabs(b.advance / a.advance - 1.5) < .0001);
    EXPECT_TRUE(__builtin_fabs(b.ink.height / a.ink.height - 1.5) < .0001);
    auto small_bitmap = normal->rasterize(glyph, 1).unwrap();
    auto large_bitmap = large->rasterize(glyph, 1).unwrap();
    EXPECT_TRUE(large_bitmap.height > small_bitmap.height);
    EXPECT_TRUE(large_bitmap.width > small_bitmap.width);
    Candidate again{directwrite::dw::retain(choices[usize()].pattern.get()), {}, {}, 1.5};
    EXPECT_EQ(candidate_font(collection, again, request.size).unwrap()->id, large->id);
}
#endif
