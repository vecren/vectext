module;
#include <string.h>
module vectext:detail.state;
import :font;
import :layout;
import :types;
import vectext.ffi.fontconfig;
import vectext.ffi.freetype;
using namespace rstd::prelude;
using namespace rstd::literals;

namespace vectext::detail {
namespace fc = ffi::fontconfig;
namespace ft = ffi::freetype;
using rstd::sync::Arc;
using rstd::sync::Weak;
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
struct Source {
    Vec<u8> bytes;
    Index index{};
    Serial revision{};
    String path, name;
    fc::Pattern pattern;
    Source(Vec<u8> bytes, Index index, String path, String name, fc::Pattern pattern)
        : bytes(rstd::move(bytes)), index(index), path(rstd::move(path)), name(rstd::move(name)),
          pattern(rstd::move(pattern)) {}
};
struct Font {
    Arc<Source> source;
    ft::Library library;
    ft::Face face;
    Scalar size{}, ascent{}, descent{}, height{};
    Serial id{new_identity()}, revision{};
    TextLimits limits;
    rstd::sync::Mutex<empty> mutex;
    Font(Arc<Source> source, Scalar size, Serial revision, TextLimits limits)
        : source(rstd::move(source)), size(size), revision(revision), limits(limits) {}
    auto initialize() -> Result<empty, TextError> {
        if (ft::FT_Init_FreeType(library.put()) ||
            ft::FT_New_Memory_Face(
                library.get(),
                reinterpret_cast<const unsigned char *>(source->bytes.as_slice().as_raw_ptr()),
                source->bytes.len().to_primitive(), source->index, face.put()) ||
            !ft::is_scalable(face.get()) ||
            ft::FT_Select_Charmap(face.get(), ft::FT_ENCODING_UNICODE))
            return Err(TextError::Font);
        auto status = set_size(1);
        if (status.is_err())
            return status;
        ascent = face->size->metrics.ascender / 64.0;
        descent = -face->size->metrics.descender / 64.0;
        height = face->size->metrics.height / 64.0;
        if (height < ascent + descent)
            height = ascent + descent;
        if (height <= 0)
            return Err(TextError::Font);
        return Ok(empty{});
    }
    auto set_size(Scalar scale) -> Result<empty, TextError> {
        if (ft::FT_Set_Char_Size(face.get(), 0, static_cast<ft::FT_F26Dot6>(size * scale * 64 + .5),
                                 72, 72))
            return Err(TextError::Font);
        return Ok(empty{});
    }
};
struct Collection {
    bool system;
    String config_path;
    TextLimits limits;
    FontStats counters{.revision = new_identity()};
    fc::Config config;
    Vec<Arc<Source>> sources;
    Vec<Weak<Source>> all_sources;
    int next_application{};
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
            bytes += source->bytes.len().to_primitive();
            ++i;
        }
        return bytes;
    }
    void clear_cache() {
        retained.clear();
        for (usize i{}; i < sources.len();) {
            if (!sources[i]->path.is_empty()) {
                sources.remove(i);
                continue;
            }
            ++i;
        }
    }
    auto make_config() -> Result<fc::Config, TextError> {
        fc::Config result;
        if (config_path.is_empty())
            result.reset(system ? fc::FcInitLoadConfigAndFonts() : fc::FcConfigCreate());
        else {
            result.reset(fc::FcConfigCreate());
            auto path = terminated(config_path.as_str());
            if (!result ||
                !fc::FcConfigParseAndLoad(
                    result.get(),
                    reinterpret_cast<const fc::FcChar8 *>(path.as_slice().as_raw_ptr()),
                    fc::FcTrue) ||
                !fc::FcConfigBuildFonts(result.get()))
                return Err(TextError::Font);
        }
        if (!result)
            return Err(TextError::Font);
        return Ok(rstd::move(result));
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
    fc::Pattern pattern;
    Option<Arc<Font>> font;
};
} // namespace vectext::detail
