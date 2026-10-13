export module vectext:font;
import :layout;
import :types;
using namespace rstd::prelude;
using namespace rstd::literals;

export namespace vectext {
struct TextLimits {
    Index text_bytes{1048576}, glyphs{1048576}, fallback_attempts{128}, font_instances{1024};
    Index bitmap_extent{8192};
    Serial font_bytes{268435456}, bitmap_bytes{16777216};
};
struct FontCollectionOptions {
    bool system_fonts{true};
    // Fontconfig configuration; nonempty values are unsupported on macOS and Windows.
    ref<str> config_file{""_str};
    TextLimits limits;
};
// font_bytes counts owned font data, excluding native system font caches.
struct FontStats {
    Serial revision{}, font_bytes{}, instances{}, font_loads{}, font_hits{}, layouts{};
};
class FontCollection {
    struct Impl;
    rstd::sync::Arc<Impl> impl_;
    friend class SystemTextService;
    explicit FontCollection(rstd::sync::Arc<Impl> impl);

  public:
    static auto create(FontCollectionOptions options = {}) -> Result<FontCollection, TextError>;
    FontCollection(const FontCollection &other);
    FontCollection(FontCollection &&);
    auto operator=(const FontCollection &other) -> FontCollection &;
    auto operator=(FontCollection &&) -> FontCollection &;
    ~FontCollection();
    auto register_font(slice<unsigned char> bytes, Index face_index = 0)
        -> Result<FontSource, TextError>;
    auto add_font(slice<unsigned char> bytes, Index face_index = 0) -> Result<empty, TextError>;
    auto refresh() -> Result<empty, TextError>;
    void clear_cache();
    auto stats() const -> FontStats;
    auto revision() const -> Serial;
};
class SystemTextService final : public TextService {
    Option<FontCollection> collection_;
    TextError initialization_error_{TextError::Font};

  public:
    SystemTextService();
    explicit SystemTextService(FontCollection collection);
    ~SystemTextService();
    auto layout(TextRequest request) -> Result<TextLayout, TextError> override;
    auto collection() const -> Result<FontCollection, TextError> {
        if (collection_.is_none())
            return Err(initialization_error_);
        return Ok(FontCollection(*collection_));
    }
};
} // namespace vectext
