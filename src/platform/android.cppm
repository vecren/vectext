module vectext:platform.android;
import :font;
import :platform.freetype;
import :platform.common;

namespace vectext::android {
using namespace rstd::prelude;
struct Config {
    bool system{};
};
struct Pattern {
    String path;
    Index index{};
    rstd::uint16_t weight{};
    bool italic{};
    Vec<FontVariation> axes;
};
struct SourceData {
    Vec<u8> bytes;
    Index index{};
    String name, family;
    bool bold{}, italic{};
    Pattern pattern;
};
using Face = freetype::Face;

auto make_config(bool system, ref<str> path) -> Result<Config, TextError>;
auto parse_source(Config &, Vec<u8> bytes, Index index, ref<str> path)
    -> Result<SourceData, TextError>;
auto system_match(const Config &, TextRequest request, Index cp) -> Result<Pattern, TextError>;
auto copy_pattern(const Pattern &pattern) -> Pattern;
auto same_pattern(const Pattern &a, const Pattern &b) -> bool;
auto source_bytes(const SourceData &source) -> Serial;
auto source_bold(const SourceData &source) -> bool;
auto source_axes(const SourceData &source) -> Result<Vec<FontAxis>, TextError>;
auto initialize(Face &face, const SourceData &source, Scalar size, TextLimits limits,
                slice<FontVariation> variations)
    -> Result<empty, TextError>;
} // namespace vectext::android
