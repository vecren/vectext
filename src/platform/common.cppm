module;
#include <string.h>
module vectext:platform.common;
import :types;
using namespace rstd::prelude;
namespace vectext {
struct GlyphMetrics {
    Scalar advance{};
    Rect ink;
};
inline auto string_view(const char *s) -> ref<str> {
    return ref<str>::from_raw_parts_unchecked(reinterpret_cast<const rstd::byte *>(s),
                                              usize(strlen(s)));
}
inline auto terminated(ref<str> s) -> Vec<char> {
    Vec<char> out;
    out.reserve(s->len() + usize(1));
    for (auto b : s->as_bytes())
        out.push(static_cast<char>(b.to_primitive()));
    out.push('\0');
    return out;
}
} // namespace vectext
