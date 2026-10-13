module vectext:detail.collection;
import :detail.collection.common;
#if __is_target_os(macos)
import :detail.collection.coretext;
#else
import :detail.collection.fontconfig;
#endif
