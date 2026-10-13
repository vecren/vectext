module vectext:detail.collection.directwrite;
import :detail.state;
import :detail.collection.common;
import :detail.platform.directwrite;
using namespace rstd::prelude;
namespace vectext::detail {
namespace dw = directwrite::dw;
auto candidates(Collection &c, TextRequest request) -> Result<Vec<Candidate>, TextError> {
    Vec<Candidate> output;
    auto append = [&](const Arc<Source> &source) {
        if (output.len() >= usize(c.limits.fallback_attempts))
            return;
        for (auto &entry : output)
            if (entry.source.is_some() && (*entry.source)->id == source->id)
                return;
        output.push(Candidate{dw::retain(source->data.pattern.get()), {}, Some(source.clone())});
    };
    if (request.source.is_some()) {
        if (request.source->collection != c.id)
            return Err(TextError::InvalidValue);
        for (auto &source : c.sources)
            if (source->id == request.source->source) {
                append(source);
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
    auto family = directwrite::wide(request.family);
    for (auto &source : c.sources) {
        if (!source->application)
            continue;
        auto name = directwrite::wide(source->data.family.as_str());
        unsigned score =
            dw::equal_name(name.as_slice().as_raw_ptr(), family.as_slice().as_raw_ptr()) ? 4 : 0;
        score += source->data.bold == request.bold;
        score += source->data.italic == request.italic;
        usize position{};
        while (position < memory.len() && memory[position].score >= score)
            ++position;
        memory.insert(position, Ranked{score, source.clone()});
    }
    for (auto &entry : memory)
        if (entry.score >= 4 || !c.system)
            append(entry.source);
    if (c.system && output.len() < usize(c.limits.fallback_attempts)) {
        auto matched = directwrite::system_match(c.config, request, 0);
        if (matched.is_err())
            return Err(matched.unwrap_err());
        output.push(Candidate{rstd::move(matched->pattern), {}, {}});
    }
    for (auto &entry : memory)
        append(entry.source);
    if (output.is_empty())
        return Err(TextError::Font);
    return Ok(rstd::move(output));
}
auto candidate_font(Collection &c, Candidate &candidate, Scalar size)
    -> Result<Arc<Font>, TextError> {
    if (candidate.font.is_some())
        return Ok(candidate.font->clone());
    auto effective_size = size * candidate.scale;
    if (!finite(effective_size) || effective_size < 1.0 / 64 || effective_size > 16384)
        return Err(TextError::Limit);
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
                directwrite::same(entry->data.pattern, candidate.pattern)) {
                source = Some(rstd::move(entry));
                break;
            }
        }
        if (source.is_none()) {
            directwrite::SourceData data;
            data.context = Some(c.config.context->clone());
            data.pattern = dw::retain(candidate.pattern.get());
            auto metadata = directwrite::source_metadata(data);
            if (metadata.is_err())
                return Err(metadata.unwrap_err());
            auto added = Arc<Source>::make(rstd::move(data), String(), false);
            added->revision = c.counters.revision;
            c.all_sources.push(added.downgrade());
            source = Some(rstd::move(added));
        }
    }
    auto loaded = c.font(source->clone(), effective_size);
    if (loaded.is_err())
        return Err(loaded.unwrap_err());
    candidate.font = Some(rstd::move(loaded).unwrap());
    return Ok(candidate.font->clone());
}
auto candidate_supports(const Candidate &candidate, Index cp) -> bool {
    unsigned short glyph{};
    return dw::glyph_indices(candidate.pattern.get(), &cp, 1, &glyph) >= 0 && glyph != 0;
}
auto fallback_candidate(Collection &c, Vec<Candidate> &choices, TextRequest request, Index cp)
    -> Result<Option<Candidate>, TextError> {
    if (!c.system || choices.len() >= usize(c.limits.fallback_attempts))
        return Ok(Option<Candidate>{});
    auto matched = directwrite::system_match(c.config, request, cp);
    if (matched.is_err())
        return Err(matched.unwrap_err());
    if (!matched->pattern)
        return Ok(Option<Candidate>{});
    for (auto &entry : choices)
        if (directwrite::same(entry.pattern, matched->pattern))
            return Ok(Option<Candidate>{});
    return Ok(Some(Candidate{rstd::move(matched->pattern), {}, {}, matched->scale}));
}
} // namespace vectext::detail
