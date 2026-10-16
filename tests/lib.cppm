export module vectext.tests;
import vectext;
using namespace rstd::prelude;
using namespace rstd::literals;

namespace {
auto data_path(ref<str> name) -> rstd::path::PathBuf {
    auto source = rstd::path::PathBuf::from(__FILE__ ""_str);
    auto directory = rstd::path::PathBuf::from(source.as_path().parent().unwrap());
    auto data = rstd::path::PathBuf::from("data"_str);
    auto file = rstd::path::PathBuf::from(name);
    directory.push(data.as_path());
    directory.push(file.as_path());
    return directory;
}
} // namespace

export namespace vectext::tests {
auto data_text(ref<str> name) -> String { return rstd::fs::read_to_string(data_path(name).as_path()).unwrap(); }
auto font_data(ref<str> name) -> Vec<unsigned char> {
    auto path = data_path(name);
    auto bytes = rstd::fs::read(path.as_path()).unwrap();
    Vec<unsigned char> result;
    result.reserve(bytes.len());
    for (auto byte : bytes)
        result.push(byte.get().to_primitive());
    return result;
}
auto fixed_collection() -> FontCollection {
    auto collection = FontCollection::create({.system_fonts = false}).unwrap();
    const ref<str> names[]{"DejaVuSans.ttf"_str, "DejaVuSans-Bold.ttf"_str,
                           "NotoSansCJK-subset.ttf"_str};
    for (auto name : names) {
        auto bytes = font_data(name);
        collection.add_font(bytes.as_slice()).unwrap();
    }
    return collection;
}
auto fixed_service() -> SystemTextService { return SystemTextService(fixed_collection()); }
auto isolated_collection() -> FontCollection {
    auto config = data_path("fonts.conf"_str);
    return FontCollection::create(
               {.system_fonts = false, .config_file = config.as_path().to_str().unwrap()})
        .unwrap();
}
} // namespace vectext::tests
