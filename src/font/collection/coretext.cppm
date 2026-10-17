module vectext:font.collection.coretext;
import :font.state;
import :font.collection.common;
import :platform.coretext;
using namespace rstd::prelude;
using namespace rstd::literals;

namespace vectext {
namespace cf = coretext::cf;
namespace ct = coretext::ct;
auto candidates(Collection &c, TextRequest request) -> Result<Vec<Candidate>, TextError> {
    Vec<Candidate> output;
    auto append_memory = [&](const Arc<Source> &source) {
        if (output.len() >= usize(c.limits.fallback_attempts))
            return;
        for (auto &entry : output)
            if (entry.source.is_some() && (*entry.source)->id == source->id)
                return;
        output.push(
            Candidate{coretext::retain(source->data.pattern.get()), {}, Some(source.clone())});
    };
    if (request.source.is_some()) {
        if (request.source->collection != c.id)
            return Err(TextError::InvalidValue);
        for (auto &source : c.sources)
            if (source->id == request.source->source) {
                append_memory(source);
                break;
            }
        if (output.is_empty())
            return Err(TextError::InvalidValue);
    }
    struct Ranked {
        unsigned score{};
        Arc<Source> source;
    };
    Vec<Ranked> memory;
    auto family = coretext::cf_string(request.family);
    if (!family)
        return Err(TextError::Font);
    for (auto &source : c.sources) {
        if (!source->application)
            continue;
        auto name = coretext::cf_string(source->data.family.as_str());
        bool match =
            name && cf::CFStringCompare(name.get(), family.get(), cf::kCFCompareCaseInsensitive) ==
                        cf::kCFCompareEqualTo;
        unsigned score = match ? 4 : 0;
        score += bool(source->data.traits & ct::kCTFontBoldTrait) == request.bold;
        score += bool(source->data.traits & ct::kCTFontItalicTrait) == request.italic;
        usize position{};
        while (position < memory.len() && memory[position].score >= score)
            ++position;
        memory.insert(position, Ranked{score, source.clone()});
    }
    for (auto &entry : memory)
        if (entry.score >= 4 || !c.config.system)
            append_memory(entry.source);
    Vec<coretext::Pattern> system;
    if (c.config.system && output.len() < usize(c.limits.fallback_attempts)) {
        auto matched = coretext::system_patterns(c.config, request);
        if (matched.is_err())
            return Err(matched.unwrap_err());
        system = rstd::move(matched).unwrap();
        if (!system.is_empty() && output.len() < usize(c.limits.fallback_attempts))
            output.push(Candidate{coretext::retain(system[usize()].get()), {}, {}});
    }
    for (auto &entry : memory)
        append_memory(entry.source);
    for (auto &pattern : system) {
        if (output.len() >= usize(c.limits.fallback_attempts))
            break;
        bool duplicate{};
        for (auto &entry : output)
            duplicate |= cf::CFEqual(entry.pattern.get(), pattern.get());
        if (!duplicate)
            output.push(Candidate{rstd::move(pattern), {}, {}});
    }
    if (output.is_empty())
        return Err(TextError::Font);
    return Ok(rstd::move(output));
}
auto candidate_font(Collection &c, Candidate &candidate, Scalar size, bool bold=false)
    -> Result<Arc<Font>, TextError> {
    if (candidate.font.is_some())
        return Ok(candidate.font->clone());
    Option<Arc<Source>> source;
    if (candidate.source.is_some())
        source = Some(candidate.source->clone());
    else {
        for (usize i{}; i < c.all_sources.len();) {
            auto entry = c.all_sources[i].upgrade();
            if (!entry) {
                c.all_sources.remove(i);
                continue;
            }
            ++i;
            if (!entry->application && entry->revision == c.counters.revision &&
                cf::CFEqual(entry->data.pattern.get(), candidate.pattern.get())) {
                source = Some(rstd::move(entry));
                break;
            }
        }
        if (source.is_none()) {
            coretext::SourceData data;
            data.pattern = coretext::retain(candidate.pattern.get());
            auto status = coretext::source_metadata(data);
            if (status.is_err())
                return Err(status.unwrap_err());
            auto added = Arc<Source>::make(rstd::move(data), String(), false);
            added->revision = c.counters.revision;
            c.all_sources.push(added.downgrade());
            source = Some(rstd::move(added));
        }
    }
    auto loaded = c.font(source->clone(), size,bold && !platform::source_bold((*source)->data));
    if (loaded.is_err())
        return Err(loaded.unwrap_err());
    candidate.font = Some(rstd::move(loaded).unwrap());
    return Ok(candidate.font->clone());
}
auto candidate_supports(const Candidate &candidate, Index cp) -> bool {
    coretext::Owned<cf::CFCharacterSetRef> charset(
        static_cast<cf::CFCharacterSetRef>(ct::CTFontDescriptorCopyAttribute(
            candidate.pattern.get(), ct::kCTFontCharacterSetAttribute)));
    return !charset || cf::CFCharacterSetIsLongCharacterMember(charset.get(), cp);
}
} // namespace vectext
