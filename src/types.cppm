export module vectext:types;
import rstd;

export namespace vectext {
using Scalar = double;
using Serial = rstd::uint64_t;
using Index = rstd::uint32_t;

inline auto finite(Scalar value) -> bool { return __builtin_isfinite(value); }

struct Point {
    Scalar x{}, y{};
    friend auto operator==(const Point &, const Point &) -> bool = default;
};
struct Size {
    Scalar width{}, height{};
    auto valid() const -> bool {
        return finite(width) && finite(height) && width >= 0 && height >= 0;
    }
    friend auto operator==(const Size &, const Size &) -> bool = default;
};
struct Rect {
    Scalar x{}, y{}, width{}, height{};
    auto valid() const -> bool {
        return finite(x) && finite(y) && Size{width, height}.valid() && finite(x + width) &&
               finite(y + height);
    }
    auto contains(Point point) const -> bool {
        return point.x >= x && point.y >= y && point.x < x + width && point.y < y + height;
    }
    friend auto operator==(const Rect &, const Rect &) -> bool = default;
};
inline auto new_identity() -> Serial {
    static Serial next = 2;
    auto result = __atomic_fetch_add(&next, 2, __ATOMIC_RELAXED);
    if (!result)
        rstd::panic("vectext identity exhausted");
    return result;
}
} // namespace vectext
