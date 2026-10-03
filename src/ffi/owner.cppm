export module vectext.ffi.owner;
import rstd;

export namespace vectext::ffi {
template <class T, auto Release> class Owner {
    T *value_{};

  public:
    Owner() = default;
    explicit Owner(T *value) : value_(value) {}
    Owner(const Owner &) = delete;
    auto operator=(const Owner &) -> Owner & = delete;
    Owner(Owner &&other) noexcept : value_(other.release()) {}
    auto operator=(Owner &&other) noexcept -> Owner & {
        if (this != &other)
            reset(other.release());
        return *this;
    }
    ~Owner() { reset(); }
    auto get() const -> T * { return value_; }
    auto operator->() const -> T * { return value_; }
    explicit operator bool() const { return value_ != nullptr; }
    auto release() -> T * { return rstd::exchange(value_, nullptr); }
    void reset(T *value = nullptr) {
        if (value_)
            Release(value_);
        value_ = value;
    }
    auto put() -> T ** {
        reset();
        return &value_;
    }
};
} // namespace vectext::ffi
