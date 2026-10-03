module vectext:detail.queries;
import :layout;
import :types;
using namespace rstd::prelude;
namespace vectext::detail {
struct Stop {
    Index byte;
    Scalar x;
};
struct QueryLine {
    Rect bounds;
    Index start, end;
};
struct Queries final : TextQueries {
    Vec<Stop> stops;
    Vec<QueryLine> lines;
    auto hit(Point p) const -> Result<TextHit, TextError> override {
        usize low{}, high = lines.len();
        while (low < high) {
            auto mid = (low + high) / usize(2);
            if (lines[mid].bounds.y + lines[mid].bounds.height <= p.y)
                low = mid + usize(1);
            else
                high = mid;
        }
        auto li = low == lines.len() ? low - usize(1) : low;
        if (li > usize() && p.y < lines[li].bounds.y) {
            const auto &previous = lines[li - usize(1)].bounds;
            if (p.y - (previous.y + previous.height) <= lines[li].bounds.y - p.y)
                --li;
        }
        const auto &line = lines[li];
        Index left = line.start, right = line.end;
        while (left < right) {
            auto mid = left + (right - left) / 2;
            if (stops[usize(mid)].x < p.x)
                left = mid + 1;
            else
                right = mid;
        }
        Index best = left == line.end ? left - 1 : left;
        if (best > line.start && __builtin_fabs(p.x - stops[usize(best - 1)].x) <=
                                     __builtin_fabs(p.x - stops[usize(best)].x))
            --best;
        auto affinity = TextAffinity::Downstream;
        if (best + 1 == line.end && li + usize(1) < lines.len() &&
            stops[usize(best)].byte == stops[usize(lines[li + usize(1)].start)].byte)
            affinity = TextAffinity::Upstream;
        return Ok(TextHit{stops[usize(best)].byte, line.bounds.contains(p), affinity});
    }
    auto caret(Index byte) const -> Result<TextCaret, TextError> override {
        return caret(byte, TextAffinity::Downstream);
    }
    auto caret(Index byte, TextAffinity affinity) const -> Result<TextCaret, TextError> override {
        usize low{}, high = stops.len();
        while (low < high) {
            auto mid = (low + high) / usize(2);
            if (stops[mid].byte < byte ||
                (stops[mid].byte == byte && affinity == TextAffinity::Downstream))
                low = mid + usize(1);
            else
                high = mid;
        }
        if (affinity == TextAffinity::Downstream) {
            if (low == usize())
                return Err(TextError::InvalidIndex);
            --low;
        }
        if (low == stops.len() || stops[low].byte != byte)
            return Err(TextError::InvalidIndex);
        usize begin{}, end = lines.len();
        while (begin < end) {
            auto mid = (begin + end) / usize(2);
            if (usize(lines[mid].end) <= low)
                begin = mid + usize(1);
            else
                end = mid;
        }
        const auto &line = lines[begin];
        return Ok(TextCaret{{stops[low].x, line.bounds.y, 0, line.bounds.height}});
    }
};
} // namespace vectext::detail
