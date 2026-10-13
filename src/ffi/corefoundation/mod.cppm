module;
#include <CoreFoundation/CoreFoundation.h>
export module vectext.ffi.corefoundation;
import vectext.ffi.owner;
export namespace vectext::ffi::corefoundation {
using ::CFArrayCreate;
using ::CFArrayGetCount;
using ::CFArrayGetValueAtIndex;
using ::CFArrayRef;
using ::CFCharacterSetIsLongCharacterMember;
using ::CFCharacterSetRef;
using ::CFDataCreateWithBytesNoCopy;
using ::CFDataRef;
using ::CFDictionaryCreate;
using ::CFDictionaryRef;
using ::CFEqual;
using ::CFIndex;
using ::CFNumberCreate;
using ::CFNumberRef;
using ::CFRelease;
using ::CFRetain;
using ::CFStringCompare;
using ::CFStringCreateWithBytes;
using ::CFStringGetCString;
using ::CFStringGetLength;
using ::CFStringGetMaximumSizeForEncoding;
using ::CFStringRef;
using ::kCFAllocatorDefault;
using ::kCFAllocatorNull;
using ::kCFCompareCaseInsensitive;
using ::kCFCompareEqualTo;
using ::kCFNumberSInt32Type;
using ::kCFStringEncodingUTF8;
using ::kCFTypeArrayCallBacks;
using ::kCFTypeDictionaryKeyCallBacks;
using ::kCFTypeDictionaryValueCallBacks;
using ::UniChar;
} // namespace vectext::ffi::corefoundation
