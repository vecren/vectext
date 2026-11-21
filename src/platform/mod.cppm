module vectext:platform;
#if __is_target_environment(android)
import :platform.android;
namespace vectext {
namespace platform = android;
}
#elif __is_target_os(macos)
import :platform.coretext;
namespace vectext {
namespace platform = coretext;
}
#elif __is_target_os(windows)
import :platform.directwrite;
namespace vectext {
namespace platform = directwrite;
}
#else
import :platform.fontconfig;
namespace vectext {
namespace platform = fontconfig;
}
#endif
