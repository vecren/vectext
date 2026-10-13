module vectext:detail.collection.common;
import :detail.state;
import :detail.platform;
using namespace rstd::prelude;
namespace vectext::detail {
auto add_source(Collection &c, Vec<u8> bytes, Index index, String path, bool application)
    -> Result<Arc<Source>, TextError> {
    if (bytes.is_empty())
        return Err(TextError::Font);
    if (bytes.len().to_primitive() > c.limits.font_bytes - c.source_bytes())
        return Err(TextError::Limit);
    auto parsed = platform::parse_source(rstd::move(bytes), index, path.as_str());
    if (parsed.is_err())
        return Err(parsed.unwrap_err());
    auto source = Arc<Source>::make(rstd::move(parsed).unwrap(), rstd::move(path), application);
    source->revision = c.counters.revision;
    c.all_sources.push(source.downgrade());
    c.sources.push(source.clone());
    return Ok(rstd::move(source));
}
} // namespace vectext::detail
