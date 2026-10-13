module vectext:font.state;
import :font;
import :layout;
import :types;
import :platform;
import :platform.common;
using namespace rstd::prelude;
using namespace rstd::literals;

namespace vectext {
using rstd::sync::Arc;
using rstd::sync::Weak;
struct Source {
    platform::SourceData data;
    Serial id{new_identity()}, revision{};
    String path;
    bool application{};
    Source(platform::SourceData data, String path, bool application)
        : data(rstd::move(data)), path(rstd::move(path)), application(application) {}
};
struct Font {
    Arc<Source> source;
    platform::Face native;
    Scalar size{}, ascent{}, descent{}, height{};
    Serial id{new_identity()}, revision{};
    TextLimits limits;
    rstd::sync::Mutex<empty> mutex;
    Font(Arc<Source> source, Scalar size, Serial revision, TextLimits limits)
        : source(rstd::move(source)), size(size), revision(revision), limits(limits) {}
    auto initialize() -> Result<empty, TextError> {
        auto result = platform::initialize(native, source->data, size, limits);
        if (result.is_err())
            return result;
        ascent = native.ascent;
        descent = native.descent;
        height = native.height;
        if (!finite(ascent) || !finite(descent) || !finite(height) || ascent < 0 || descent < 0 ||
            height <= 0)
            return Err(TextError::Font);
        return Ok(empty{});
    }
    auto glyph(Index codepoint) -> Index {
        auto guard = mutex.lock().unwrap();
        return native.glyph(codepoint);
    }
    auto measure(Index glyph) -> Result<GlyphMetrics, TextError> {
        auto guard = mutex.lock().unwrap();
        return native.measure(glyph);
    }
    auto rasterize(Index glyph, Scalar scale) -> Result<GlyphBitmap, TextError> {
        auto guard = mutex.lock().unwrap();
        return native.rasterize(glyph, scale);
    }
};
struct Collection {
    Serial id{new_identity()};
    bool system;
    String config_path;
    TextLimits limits;
    FontStats counters{.revision = new_identity()};
    platform::Config config;
    Vec<Arc<Source>> sources;
    Vec<Weak<Source>> all_sources;
    Vec<Weak<Font>> instances;
    Vec<Arc<Font>> retained;
    rstd::sync::Mutex<empty> mutex;
    Collection(bool system, String path, TextLimits limits)
        : system(system), config_path(rstd::move(path)), limits(limits) {}
    auto source_bytes() -> Serial {
        Serial bytes{};
        for (usize i{}; i < all_sources.len();) {
            auto source = all_sources[i].upgrade();
            if (!source) {
                all_sources.remove(i);
                continue;
            }
            bytes += platform::source_bytes(source->data);
            ++i;
        }
        return bytes;
    }
    void clear_cache() {
        retained.clear();
        for (usize i{}; i < sources.len();) {
            if (!sources[i]->application) {
                sources.remove(i);
                continue;
            }
            ++i;
        }
    }
    auto make_config() -> Result<platform::Config, TextError> {
        return platform::make_config(system, config_path.as_str());
    }
    auto font(Arc<Source> source, Scalar size) -> Result<Arc<Font>, TextError> {
        Serial alive{};
        for (usize i{}; i < instances.len();) {
            auto instance = instances[i].upgrade();
            if (!instance) {
                instances.remove(i);
                continue;
            }
            ++alive;
            if (instance->revision == counters.revision && instance->size == size &&
                Arc<Source>::ptr_eq(instance->source, source)) {
                ++counters.font_hits;
                return Ok(rstd::move(instance));
            }
            ++i;
        }
        if (alive >= limits.font_instances) {
            retained.clear();
            alive = 0;
            for (auto &weak : instances)
                if (!weak.expired())
                    ++alive;
            if (alive >= limits.font_instances)
                return Err(TextError::Limit);
        }
        auto result = Arc<Font>::make(rstd::move(source), size, counters.revision, limits);
        auto status = result->initialize();
        if (status.is_err())
            return Err(status.unwrap_err());
        instances.push(result.downgrade());
        retained.push(result.clone());
        ++counters.font_loads;
        return Ok(rstd::move(result));
    }
};
struct Candidate {
    platform::Pattern pattern;
    Option<Arc<Font>> font;
    Option<Arc<Source>> source;
    Scalar scale{1};
};
} // namespace vectext
