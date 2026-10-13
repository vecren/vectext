module;
#include <CoreGraphics/CoreGraphics.h>
export module vectext.ffi.coregraphics;
import vectext.ffi.owner;
export namespace vectext::ffi::coregraphics {
using ::CGAffineTransformMakeScale;
using ::CGBitmapContextCreate;
using ::CGColorSpaceCreateDeviceGray;
using ::CGColorSpaceRef;
using ::CGColorSpaceRelease;
using ::CGContextAddPath;
using ::CGContextFillPath;
using ::CGContextRef;
using ::CGContextRelease;
using ::CGContextSetAllowsAntialiasing;
using ::CGContextSetGrayFillColor;
using ::CGContextSetShouldAntialias;
using ::CGContextTranslateCTM;
using ::CGGlyph;
using ::CGPathGetPathBoundingBox;
using ::CGPathIsEmpty;
using ::CGPathRef;
using ::CGPathRelease;
using ::CGSize;
using ::kCGImageAlphaNone;
} // namespace vectext::ffi::coregraphics
