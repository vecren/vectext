module;
#include <string.h>
module vectext:platform.common;
import :types;
import :layout;
using namespace rstd::prelude;
namespace vectext {
struct GlyphMetrics {
    Scalar advance{};
    Rect ink;
};
struct GlyphOutlineBuilder {
    GlyphOutline outline;
    Option<TextError> error;
    bool open{};

    void push(GlyphOutlineCommand command) {
        if (error) return;
        if (outline.commands.len() >= usize(65536)) {
            error = Some(TextError::Limit);
            return;
        }
        const Point points[]{command.end, command.control1, command.control2};
        for (const auto point : points) {
            if (!finite(point.x) || !finite(point.y)) {
                error = Some(TextError::Font);
                return;
            }
        }
        outline.commands.push(rstd::move(command));
    }
    void move(Point to) {
        close();
        push({GlyphOutlineVerb::Move, to});
        open = true;
    }
    void line(Point to) { push({GlyphOutlineVerb::Line, to}); }
    void quadratic(Point control, Point to) { push({GlyphOutlineVerb::Quadratic, to, control}); }
    void cubic(Point first, Point second, Point to) { push({GlyphOutlineVerb::Cubic, to, first, second}); }
    void close() {
        if (open) push({GlyphOutlineVerb::Close});
        open = false;
    }
    auto finish() -> Result<GlyphOutline, TextError> {
        close();
        if (error) return Err(*error);
        return Ok(rstd::move(outline));
    }
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
