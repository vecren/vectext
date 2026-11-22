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
using ::CGPathApply;
using ::CGPathElement;
using ::CGPathGetPathBoundingBox;
using ::CGPathIsEmpty;
using ::CGPathRef;
using ::CGPathRelease;
using ::CGPoint;
using ::CGSize;
using ::kCGImageAlphaNone;
using ::kCGPathElementAddCurveToPoint;
using ::kCGPathElementAddLineToPoint;
using ::kCGPathElementAddQuadCurveToPoint;
using ::kCGPathElementCloseSubpath;
using ::kCGPathElementMoveToPoint;
} // namespace vectext::ffi::coregraphics
