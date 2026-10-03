module vectext:detail.collection;
import :detail.state;
import :font;
import :types;
import vectext.ffi.fontconfig;
import vectext.ffi.freetype;
using namespace rstd::prelude;
using namespace rstd::literals;
namespace vectext::detail {
auto add_source(Collection &c, Vec<u8> bytes, Index index, String path, bool application)
    -> Result<Arc<Source>, TextError> {
    if (bytes.is_empty())
        return Err(TextError::Font);
    if (application && c.next_application == 2147483647)
        return Err(TextError::Limit);
    if (bytes.len().to_primitive() > c.limits.font_bytes - c.source_bytes())
        return Err(TextError::Limit);
    ft::Library library;
    ft::Face face;
    if (ft::FT_Init_FreeType(library.put()) ||
        ft::FT_New_Memory_Face(
            library.get(), reinterpret_cast<const unsigned char *>(bytes.as_slice().as_raw_ptr()),
            bytes.len().to_primitive(), index, face.put()) ||
        !ft::is_scalable(face.get()) || ft::FT_Select_Charmap(face.get(), ft::FT_ENCODING_UNICODE))
        return Err(TextError::Font);
    auto name = String::make(string_view(face->family_name ? face->family_name : "Unknown"));
    if (face->style_name) {
        name.push_ascii(' ');
        name.push_str(string_view(face->style_name));
    }
    auto filename = terminated(path.as_str());
    fc::Pattern pattern(fc::FcFreeTypeQueryFace(
        face.get(), reinterpret_cast<const fc::FcChar8 *>(filename.as_slice().as_raw_ptr()), index,
        nullptr));
    if (!pattern)
        return Err(TextError::Font);
    if (application &&
        !fc::FcPatternAddInteger(pattern.get(), "vectext-source", c.next_application))
        return Err(TextError::Font);
    auto source = Arc<Source>::make(rstd::move(bytes), index, rstd::move(path), rstd::move(name),
                                    rstd::move(pattern));
    source->revision = c.counters.revision;
    if (application)
        ++c.next_application;
    c.all_sources.push(source.downgrade());
    c.sources.push(source.clone());
    return Ok(rstd::move(source));
}
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
    for (auto &source : c.sources) {
        int index{};
        if (fc::FcPatternGetInteger(source->pattern.get(), "vectext-source", 0, &index) ==
            fc::FcResultMatch) {
            fc::Pattern copy(fc::FcPatternDuplicate(source->pattern.get()));
            if (!copy || !fc::FcFontSetAdd(memory.get(), copy.get()))
                return Err(TextError::Font);
            copy.release();
        }
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
    for (int i = 0; i < sorted->nfont && static_cast<Index>(i) < c.limits.fallback_attempts; ++i) {
        fc::Pattern copy(fc::FcPatternDuplicate(sorted->fonts[i]));
        if (!copy)
            return Err(TextError::Font);
        output.push(Candidate{rstd::move(copy), {}});
    }
    return Ok(rstd::move(output));
}
auto candidate_font(Collection &c, Candidate &candidate, Scalar size)
    -> Result<Arc<Font>, TextError> {
    if (candidate.font.is_some())
        return Ok(candidate.font->clone());
    int app{};
    Option<Arc<Source>> source;
    if (fc::FcPatternGetInteger(candidate.pattern.get(), "vectext-source", 0, &app) ==
        fc::FcResultMatch) {
        for (auto &entry : c.sources) {
            int id{};
            if (fc::FcPatternGetInteger(entry->pattern.get(), "vectext-source", 0, &id) ==
                    fc::FcResultMatch &&
                id == app) {
                source = Some(entry.clone());
                break;
            }
        }
        if (source.is_none())
            return Err(TextError::Font);
    } else {
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
                entry->index == static_cast<Index>(index)) {
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
} // namespace vectext::detail
