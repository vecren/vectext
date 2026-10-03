#include <rstd/test/gtest.hpp>

import vectext;
import vectext.tests;
import rstd.test;
using namespace vectext::tests;
using namespace rstd::prelude;
using namespace rstd::literals;
using namespace vectext;
TEST(VectextPerformance, ColdWarmAndRaster) {
    auto start = rstd::time::Instant::now();
    auto collection = fixed_collection();
    SystemTextService service(collection);
    auto load_time = start.elapsed().as_secs_f64();
    TextRequest request{
        .text = "Hello 中文 basic text"_str, .family = "DejaVu Sans"_str, .width = Some(120.0)};
    start = rstd::time::Instant::now();
    auto layout = service.layout(request).unwrap();
    auto cold = start.elapsed().as_secs_f64();
    auto loads = collection.stats().font_loads;
    constexpr unsigned samples = 100;
    start = rstd::time::Instant::now();
    for (unsigned i = 0; i < samples; ++i) {
        auto result = service.layout(request);
        ASSERT_TRUE(result.is_ok());
    }
    auto warm = start.elapsed().as_secs_f64();
    EXPECT_EQ(collection.stats().font_loads, loads);
    auto &g = layout.glyphs()[usize()];
    auto &font = layout.fonts()[usize(g.font)];
    start = rstd::time::Instant::now();
    Serial bytes{};
    for (unsigned i = 0; i < samples; ++i) {
        auto bitmap = font.rasterize(g.glyph, 1).unwrap();
        bytes += bitmap.coverage.len().to_primitive();
    }
    auto raster = start.elapsed().as_secs_f64();
    rstd::io::println("perf fixed-fonts size=16 width=120 samples={} font-load-ms={:.3f} "
                      "cold-layout-ms={:.3f} warm-layout-mean-ms={:.3f} raster-mean-ms={:.3f} "
                      "raster-bytes={} font-loads={} font-hits={} source-bytes={}",
                      samples, load_time * 1000, cold * 1000, warm * 1000 / samples,
                      raster * 1000 / samples, bytes, loads, collection.stats().font_hits,
                      collection.stats().font_bytes);
}
