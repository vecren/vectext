export module vectext.error;

export namespace vectext {
enum class TextError {
    InvalidValue,
    InvalidUtf8,
    InvalidIndex,
    Limit,
    Font,
    Layout,
    Raster,
    Unsupported
};
}
