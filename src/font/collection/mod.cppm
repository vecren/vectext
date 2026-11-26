module vectext:font.collection;
import :font.collection.common;
#if __is_target_environment(android)
import :font.collection.android;
#elif __is_target_os(macos)
import :font.collection.coretext;
#elif __is_target_os(windows)
import :font.collection.directwrite;
#else
import :font.collection.fontconfig;
#endif
import :font.state;
using namespace rstd::prelude;
namespace vectext {
struct ResolvedGlyph {
    Arc<Font> font;
    Index glyph{};
    bool missing{};
    GlyphMetrics metrics;
};
struct FontResolver {
    Vec<Candidate> choices;
    Arc<Font> primary;
    TextRequest request;
    auto resolve(Collection &collection, Index cp) -> Result<ResolvedGlyph, TextError> {
        for (auto &candidate : choices) {
            if (!candidate_supports(candidate, cp))
                continue;
            auto loaded = candidate_font(collection, candidate, request.size,request.bold);
            if (loaded.is_err())
                return Err(loaded.unwrap_err());
            auto font = rstd::move(loaded).unwrap();
            auto glyph = font->glyph(cp);
            if (!glyph)
                continue;
            auto measured = font->layout_metrics(glyph);
            if (measured.is_err()) {
                if (measured.unwrap_err() == TextError::Unsupported)
                    continue;
                return Err(measured.unwrap_err());
            }
            return Ok(ResolvedGlyph{rstd::move(font), glyph, false, measured.unwrap()});
        }
#if __is_target_os(windows) || __is_target_environment(android)
        auto fallback = fallback_candidate(collection, choices, request, cp);
        if (fallback.is_err())
            return Err(fallback.unwrap_err());
        if (fallback->is_some()) {
            auto loaded = candidate_font(collection, **fallback, request.size,request.bold);
            if (loaded.is_err())
                return Err(loaded.unwrap_err());
            auto font = rstd::move(loaded).unwrap();
            auto glyph = font->glyph(cp);
            choices.push(rstd::move(**fallback));
            if (glyph) {
                auto measured = font->layout_metrics(glyph);
                if (measured.is_ok())
                    return Ok(ResolvedGlyph{rstd::move(font), glyph, false, measured.unwrap()});
                if (measured.unwrap_err() != TextError::Unsupported)
                    return Err(measured.unwrap_err());
            }
        }
#endif
        auto measured = primary->layout_metrics(0);
        if (measured.is_err())
            return Err(measured.unwrap_err());
        return Ok(ResolvedGlyph{primary.clone(), 0, true, measured.unwrap()});
    }
};
auto resolver(Collection &collection, TextRequest request) -> Result<FontResolver, TextError> {
    if (!request.variations.is_empty() &&
        (request.source.is_none() || collection.registered(*request.source).is_none()))
        return Err(TextError::InvalidValue);
    auto matched = candidates(collection, request);
    if (matched.is_err())
        return Err(matched.unwrap_err());
    auto choices = rstd::move(matched).unwrap();
    auto primary = candidate_font(collection, choices[usize()], request.size, request.bold,
                                  request.variations);
    if (primary.is_err())
        return Err(primary.unwrap_err());
    return Ok(FontResolver{rstd::move(choices), rstd::move(primary).unwrap(), request});
}
} // namespace vectext
