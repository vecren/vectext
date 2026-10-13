module vectext:detail.platform;
#if __is_target_os(macos)
import :detail.platform.coretext;
namespace vectext::detail {
namespace platform = coretext;
}
#else
import :detail.platform.fontconfig;
namespace vectext::detail {
namespace platform = fontconfig;
}
#endif
