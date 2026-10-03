module vectext;
import :detail.state;
import :detail.collection;
import :detail.layout;
import :detail.raster;
using namespace vectext::detail;
using namespace rstd::prelude;
namespace vectext {
struct FontCollection::Impl {
    Collection state;
    explicit Impl(FontCollectionOptions options)
        : state(options.system_fonts, String::make(options.config_file), options.limits) {}
};
FontCollection::FontCollection(rstd::sync::Arc<Impl> impl) : impl_(rstd::move(impl)) {}
FontCollection::FontCollection(const FontCollection &other) : impl_(other.impl_.clone()) {}
FontCollection::FontCollection(FontCollection &&) = default;
FontCollection::~FontCollection() = default;
auto FontCollection::operator=(const FontCollection &other) -> FontCollection & {
    if (this != &other)
        impl_ = other.impl_.clone();
    return *this;
}
auto FontCollection::operator=(FontCollection &&) -> FontCollection & = default;
auto FontCollection::create(FontCollectionOptions options) -> Result<FontCollection, TextError> {
    if (options.config_file->len() > usize(4096))
        return Err(TextError::Limit);
    if (rstd::str_::from_utf8(options.config_file->as_bytes()).is_err())
        return Err(TextError::InvalidUtf8);
    for (auto byte : options.config_file->as_bytes())
        if (byte == u8())
            return Err(TextError::InvalidValue);
    auto limits = options.limits;
    if (!limits.text_bytes || !limits.glyphs || !limits.fallback_attempts ||
        !limits.font_instances || !limits.bitmap_extent || !limits.font_bytes ||
        !limits.bitmap_bytes)
        return Err(TextError::InvalidValue);
    if (limits.text_bytes > 1048576 || limits.glyphs > 1048576 || limits.fallback_attempts > 4096 ||
        limits.font_instances > 16384 || limits.font_bytes > 1073741824 ||
        limits.bitmap_extent > 65536 || limits.bitmap_bytes > 1073741824)
        return Err(TextError::Limit);
    auto impl = Arc<Impl>::make(options);
    auto config = impl->state.make_config();
    if (config.is_err())
        return Err(config.unwrap_err());
    impl->state.config = rstd::move(config).unwrap();
    return Ok(FontCollection(rstd::move(impl)));
}
auto FontCollection::add_font(slice<unsigned char> bytes, Index face_index)
    -> Result<empty, TextError> {
    auto &state = impl_->state;
    auto guard = state.mutex.lock().unwrap();
    if (bytes.len().to_primitive() > state.limits.font_bytes - state.source_bytes())
        return Err(TextError::Limit);
    Vec<u8> owned;
    owned.reserve(bytes.len());
    for (auto byte : bytes)
        owned.push(u8(byte));
    auto added = add_source(state, rstd::move(owned), face_index, String(), true);
    if (added.is_err())
        return Err(added.unwrap_err());
    state.clear_cache();
    state.counters.revision = new_identity();
    return Ok(empty{});
}
auto FontCollection::refresh() -> Result<empty, TextError> {
    auto &state = impl_->state;
    auto guard = state.mutex.lock().unwrap();
    auto config = state.make_config();
    if (config.is_err())
        return Err(config.unwrap_err());
    state.config = rstd::move(config).unwrap();
    state.clear_cache();
    state.counters.revision = new_identity();
    return Ok(empty{});
}
void FontCollection::clear_cache() {
    auto &state = impl_->state;
    auto guard = state.mutex.lock().unwrap();
    state.clear_cache();
}
auto FontCollection::stats() const -> FontStats {
    auto &state = impl_->state;
    auto guard = state.mutex.lock().unwrap();
    auto stats = state.counters;
    stats.font_bytes = state.source_bytes();
    for (const auto &weak : state.instances)
        if (!weak.expired())
            ++stats.instances;
    return stats;
}
auto FontCollection::revision() const -> Serial { return stats().revision; }
SystemTextService::SystemTextService() {
    auto result = FontCollection::create();
    if (result.is_err())
        initialization_error_ = result.unwrap_err();
    else
        collection_ = Some(rstd::move(result).unwrap());
}
SystemTextService::SystemTextService(FontCollection collection)
    : collection_(Some(rstd::move(collection))) {}
SystemTextService::~SystemTextService() = default;
auto SystemTextService::layout(TextRequest request) -> Result<TextLayout, TextError> {
    if (collection_.is_none())
        return Err(initialization_error_);
    auto &state = collection_->impl_->state;
    auto guard = state.mutex.lock().unwrap();
    auto built = build_layout(state, request);
    if (built.is_err())
        return Err(built.unwrap_err());
    auto result = rstd::move(built).unwrap();
    for (auto &font : result.fonts) {
        auto rasterizer = Box<Rasterizer>::make(font.clone());
        result.data.fonts.push(
            FontInstance(font->id, font->source->name.clone(),
                         Box<FontRasterizer>::from_raw(
                             rstd::move(rasterizer).into_raw().template cast<FontRasterizer>())));
    }
    return TextLayout::create(rstd::move(result.data));
}
} // namespace vectext
