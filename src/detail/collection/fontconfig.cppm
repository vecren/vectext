module vectext:detail.collection.fontconfig;
import :detail.state;
import :detail.collection.common;
import :detail.platform.common;
import vectext.ffi.fontconfig;
using namespace rstd::prelude;
namespace vectext::detail {
namespace fc = ffi::fontconfig;
auto candidates(Collection &c, TextRequest request) -> Result<Vec<Candidate>, TextError> {
    fc::Pattern pattern(fc::FcPatternCreate());
    auto family = terminated(request.family), language = terminated(request.language);
    if (!pattern ||
        !fc::FcPatternAddString(
            pattern.get(), fc::FC_FAMILY,
            reinterpret_cast<const fc::FcChar8 *>(family.as_slice().as_raw_ptr())) ||
        !fc::FcPatternAddString(
            pattern.get(), fc::FC_LANG,
            reinterpret_cast<const fc::FcChar8 *>(language.as_slice().as_raw_ptr())) ||
        !fc::FcPatternAddInteger(pattern.get(), fc::FC_WEIGHT,
                                 request.bold ? fc::FC_WEIGHT_BOLD : fc::FC_WEIGHT_REGULAR) ||
        !fc::FcPatternAddInteger(pattern.get(), fc::FC_SLANT,
                                 request.italic ? fc::FC_SLANT_ITALIC : fc::FC_SLANT_ROMAN) ||
        !fc::FcPatternAddDouble(pattern.get(), fc::FC_PIXEL_SIZE, request.size) ||
        !fc::FcPatternAddBool(pattern.get(), fc::FC_SCALABLE, fc::FcTrue) ||
        !fc::FcConfigSubstitute(c.config.get(), pattern.get(), fc::FcMatchPattern))
        return Err(TextError::Font);
    fc::FcDefaultSubstitute(pattern.get());
    fc::FontSet memory(fc::FcFontSetCreate());
    if (!memory)
        return Err(TextError::Font);
    for (usize i{}; i < c.sources.len(); ++i) {
        auto &source = c.sources[i];
        if (!source->application)
            continue;
        fc::Pattern copy(fc::FcPatternDuplicate(source->data.pattern.get()));
        if (!copy ||
            !fc::FcPatternAddInteger(copy.get(), "vectext-source",
                                     static_cast<int>(i.to_primitive())) ||
            !fc::FcFontSetAdd(memory.get(), copy.get()))
            return Err(TextError::Font);
        copy.release();
    }
    fc::FcFontSet *sets[3]{memory.get()};
    int count = 1;
    const decltype(fc::FcSetSystem) kinds[]{fc::FcSetSystem, fc::FcSetApplication};
    for (auto kind : kinds) {
        auto *set = fc::FcConfigGetFonts(c.config.get(), kind);
        if (set)
            sets[count++] = set;
    }
    fc::FcResult status;
    fc::FontSet sorted(fc::FcFontSetSort(c.config.get(), sets, count, pattern.get(), fc::FcFalse,
                                         nullptr, &status));
    if (!sorted || !sorted->nfont)
        return Err(TextError::Font);
    Vec<Candidate> output;
    if (request.source.is_some()) {
        if (request.source->collection != c.id)
            return Err(TextError::InvalidValue);
        for (auto &source : c.sources) {
            if (source->id != request.source->source)
                continue;
            fc::Pattern copy(fc::FcPatternDuplicate(source->data.pattern.get()));
            if (!copy)
                return Err(TextError::Font);
            output.push(Candidate{rstd::move(copy), {}, Some(source.clone())});
            break;
        }
        if (output.is_empty())
            return Err(TextError::InvalidValue);
    }
    for (int i = 0; i < sorted->nfont && output.len() < usize(c.limits.fallback_attempts); ++i) {
        fc::Pattern copy(fc::FcPatternDuplicate(sorted->fonts[i]));
        if (!copy)
            return Err(TextError::Font);
        int app{};
        Option<Arc<Source>> source;
        if (fc::FcPatternGetInteger(copy.get(), "vectext-source", 0, &app) == fc::FcResultMatch) {
            if (app < 0 || usize(app) >= c.sources.len())
                return Err(TextError::Font);
            source = Some(c.sources[usize(app)].clone());
            if (request.source.is_some() && (*source)->id == request.source->source)
                continue;
        }
        output.push(Candidate{rstd::move(copy), {}, rstd::move(source)});
    }
    return Ok(rstd::move(output));
}
auto candidate_font(Collection &c, Candidate &candidate, Scalar size)
    -> Result<Arc<Font>, TextError> {
    if (candidate.font.is_some())
        return Ok(candidate.font->clone());
    Option<Arc<Source>> source;
    if (candidate.source.is_some())
        source = Some(candidate.source->clone());
    else {
        fc::FcChar8 *file{};
        int index{};
        if (fc::FcPatternGetString(candidate.pattern.get(), fc::FC_FILE, 0, &file) !=
                fc::FcResultMatch ||
            fc::FcPatternGetInteger(candidate.pattern.get(), fc::FC_INDEX, 0, &index) !=
                fc::FcResultMatch ||
            index < 0)
            return Err(TextError::Font);
        auto path = string_view(reinterpret_cast<const char *>(file));
        for (auto &weak : c.all_sources) {
            auto entry = weak.upgrade();
            if (entry && entry->revision == c.counters.revision && entry->path.as_str() == path &&
                entry->data.index == static_cast<Index>(index)) {
                source = Some(rstd::move(entry));
                break;
            }
        }
        if (source.is_none()) {
            auto pathname = rstd::path::PathBuf::from(path);
            auto opened = rstd::fs::File::open(pathname.as_path());
            if (opened.is_err())
                return Err(TextError::Font);
            auto input = rstd::move(opened).unwrap();
            auto metadata = input.metadata();
            if (metadata.is_err())
                return Err(TextError::Font);
            auto length = metadata.unwrap().len().to_primitive();
            if (length > c.limits.font_bytes - c.source_bytes())
                return Err(TextError::Limit);
            Vec<u8> bytes;
            bytes.resize(usize(length), u8());
            if (input.read_exact_at(bytes.as_mut_slice().as_mut_ref(), u64()).is_err())
                return Err(TextError::Font);
            auto loaded = add_source(c, rstd::move(bytes), static_cast<Index>(index),
                                     String::make(path), false);
            if (loaded.is_err())
                return Err(loaded.unwrap_err());
            source = Some(rstd::move(loaded).unwrap());
        }
    }
    auto loaded = c.font(source->clone(), size);
    if (loaded.is_err())
        return Err(loaded.unwrap_err());
    candidate.font = Some(rstd::move(loaded).unwrap());
    return Ok(candidate.font->clone());
}

auto candidate_supports(const Candidate &candidate, Index cp) -> bool {
    fc::FcCharSet *charset{};
    return fc::FcPatternGetCharSet(candidate.pattern.get(), fc::FC_CHARSET, 0, &charset) !=
               fc::FcResultMatch ||
           fc::FcCharSetHasChar(charset, cp);
}
} // namespace vectext::detail
