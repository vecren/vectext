module vectext:font.synthesis;
import :font.types;
import :layout;
using namespace rstd::prelude;
namespace vectext {
auto embolden_bitmap(GlyphBitmap bitmap, Scalar strength, TextLimits limits) -> Result<GlyphBitmap,TextError> {
    if (!bitmap.width || !bitmap.height) return Ok(rstd::move(bitmap));
    auto amount=strength*bitmap.scale;
    if (!finite(amount) || amount <= 0 || amount > limits.bitmap_extent) return Err(TextError::Limit);
    auto extra=static_cast<Index>(__builtin_ceil(amount));
    auto width=Serial(bitmap.width)+extra, height=Serial(bitmap.height)+extra;
    if (width > limits.bitmap_extent || height > limits.bitmap_extent || width*height > limits.bitmap_bytes) return Err(TextError::Limit);
    auto radius=static_cast<Index>(__builtin_floor(amount)); auto fraction=amount-radius;
    auto filter=[radius,fraction](Index length,Index total,auto read,auto write) {
        Vec<Index> queue; queue.reserve(usize(length)); usize first{};
        for (Index i{};i<total;++i) {
            if (i < length) {
                auto value=read(i);
                while (queue.len()>first && read(queue[queue.len()-usize(1)])<=value) queue.pop();
                queue.push(Index(i));
            }
            while (first<queue.len() && i-queue[first]>radius) ++first;
            unsigned value=first<queue.len() ? read(queue[first]) : 0;
            if (fraction>0 && i>radius && i-radius-1<length) {
                auto edge=static_cast<unsigned>(read(i-radius-1)*fraction+.5);
                if (edge>value) value=edge;
            }
            write(i,static_cast<unsigned char>(value));
        }
    };
    Vec<unsigned char> horizontal, pixels;
    horizontal.resize(usize(width*bitmap.height),0); pixels.resize(usize(width*height),0);
    auto target_width=static_cast<Index>(width), target_height=static_cast<Index>(height);
    for (Index y{};y<bitmap.height;++y)
        filter(bitmap.width,target_width,[&](Index x){return bitmap.coverage[usize(Serial(y)*bitmap.width+x)];},
            [&](Index x,unsigned char value){horizontal[usize(Serial(y)*target_width+x)]=value;});
    for (Index x{};x<target_width;++x)
        filter(bitmap.height,target_height,[&](Index y){return horizontal[usize(Serial(y)*target_width+x)];},
            [&](Index y,unsigned char value){pixels[usize(Serial(y)*target_width+x)]=value;});
    bitmap.width=target_width; bitmap.height=target_height; bitmap.coverage=rstd::move(pixels);
    return Ok(rstd::move(bitmap));
}
} // namespace vectext
