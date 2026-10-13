module vectext:detail.platform;
#if __is_target_os(macos)
import :detail.platform.coretext;
namespace vectext::detail {
namespace platform = coretext;
}
#elif __is_target_os(windows)
import :detail.platform.directwrite;
namespace vectext::detail {
namespace platform = directwrite;
}
#else
import :detail.platform.fontconfig;
namespace vectext::detail {
namespace platform = fontconfig;
}
#endif
