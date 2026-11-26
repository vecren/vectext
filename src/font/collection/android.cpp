module vectext;
import :font.collection.android;
import :font.collection.common;
import :platform.android;

namespace vectext {
using namespace rstd::prelude;

auto same_family(ref<str> a, ref<str> b) -> bool {
    if (a->len() != b->len())
        return false;
    auto lower = [](u8 value) {
        auto scalar = value.to_primitive();
        return scalar >= 'A' && scalar <= 'Z' ? scalar + ('a' - 'A') : scalar;
    };
    for (usize i{}; i < a->len(); ++i)
        if (lower(a->as_bytes()[i]) != lower(b->as_bytes()[i]))
            return false;
    return true;
}

auto candidates(Collection &c, TextRequest request) -> Result<Vec<Candidate>, TextError> {
    Vec<Candidate> output;
    auto append = [&](const Arc<Source> &source) {
        if (output.len() >= usize(c.limits.fallback_attempts))
            return;
        for (auto &entry : output)
            if (entry.source.is_some() && (*entry.source)->id == source->id)
                return;
        output.push(
            Candidate{android::copy_pattern(source->data.pattern), {}, Some(source.clone())});
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
        unsigned score;
        Arc<Source> source;
    };
    Vec<Ranked> memory;
    for (auto &source : c.sources) {
        if (!source->application)
            continue;
        auto score = same_family(source->data.family.as_str(), request.family) ? 4u : 0u;
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
        auto matched = android::system_match(c.config, request, 'A');
        if (matched.is_err())
            return Err(matched.unwrap_err());
        output.push(Candidate{rstd::move(matched).unwrap(), {}, {}});
    }
    for (auto &entry : memory)
        append(entry.source);
    if (output.is_empty())
        return Err(TextError::Font);
    return Ok(rstd::move(output));
}

auto candidate_font(Collection &c, Candidate &candidate, Scalar size, bool bold,
                    slice<FontVariation> variations) -> Result<Arc<Font>, TextError> {
    if (candidate.font.is_some())
        return Ok(candidate.font->clone());
    Option<Arc<Source>> source;
    if (candidate.source.is_some())
        source = Some(candidate.source->clone());
    else {
        for (auto &weak : c.all_sources) {
            auto entry = weak.upgrade();
            if (entry && entry->revision == c.counters.revision &&
                android::same_pattern(entry->data.pattern, candidate.pattern)) {
                source = Some(rstd::move(entry));
                break;
            }
        }
        if (source.is_none()) {
            auto opened = rstd::fs::File::open(
                rstd::path::PathBuf::from(candidate.pattern.path.as_str()).as_path());
            if (opened.is_err())
                return Err(TextError::Font);
            auto input = rstd::move(opened).unwrap();
            auto metadata = input.metadata();
            if (metadata.is_err())
                return Err(TextError::Font);
            auto length = metadata->len().to_primitive();
            if (length > c.limits.font_bytes - c.source_bytes())
                return Err(TextError::Limit);
            Vec<u8> bytes;
            bytes.resize(usize(length), u8());
            if (input.read_exact_at(bytes.as_mut_slice().as_mut_ref(), u64()).is_err())
                return Err(TextError::Font);
            auto added = add_source(c, rstd::move(bytes), candidate.pattern.index,
                                    String::make(candidate.pattern.path.as_str()), false);
            if (added.is_err())
                return Err(added.unwrap_err());
            source = Some(rstd::move(added).unwrap());
            (*source)->data.pattern = android::copy_pattern(candidate.pattern);
        }
    }
    auto loaded = c.font(source->clone(), size, bold && !platform::source_bold((*source)->data),
                         variations);
    if (loaded.is_err())
        return Err(loaded.unwrap_err());
    candidate.font = Some(rstd::move(loaded).unwrap());
    return Ok(candidate.font->clone());
}

auto candidate_supports(const Candidate &, Index) -> bool { return true; }

auto fallback_candidate(Collection &c, Vec<Candidate> &choices, TextRequest request, Index cp)
    -> Result<Option<Candidate>, TextError> {
    if (!c.system || choices.len() >= usize(c.limits.fallback_attempts))
        return Ok(Option<Candidate>{});
    auto matched = android::system_match(c.config, request, cp);
    if (matched.is_err())
        return Err(matched.unwrap_err());
    for (auto &entry : choices)
        if (android::same_pattern(entry.pattern, *matched))
            return Ok(Option<Candidate>{});
    Candidate candidate{rstd::move(matched).unwrap(), {}, {}};
    auto loaded = candidate_font(c, candidate, request.size, request.bold);
    if (loaded.is_err()) {
        if (loaded.unwrap_err() == TextError::Unsupported)
            return Ok(Option<Candidate>{});
        return Err(loaded.unwrap_err());
    }
    return Ok(Some(rstd::move(candidate)));
}
} // namespace vectext
