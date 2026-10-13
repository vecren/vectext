module;
#include <rstd/test/gtest.hpp>
module vectext;
import :detail.raster;
import :detail.layout;
#if !__is_target_os(macos)
import vectext.ffi.freetype;
import :detail.platform.freetype;
#endif
import rstd.test;
using namespace vectext;
using namespace vectext::detail;
using namespace rstd::prelude;
#if !__is_target_os(macos)
namespace ft = vectext::ffi::freetype;
using vectext::detail::freetype::bitmap_coverage;
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
