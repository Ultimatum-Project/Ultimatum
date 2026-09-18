#ifndef ZU4_MOBILE_LAYOUT_H
#define ZU4_MOBILE_LAYOUT_H
#import <UIKit/UIKit.h>
// Shared by the renderer, controls and sheets. Menus never change this geometry.
static inline CGRect zu4MapFrame(CGRect bounds, UIEdgeInsets safe) {
    CGRect a = UIEdgeInsetsInsetRect(bounds, safe);
    BOOL portrait = bounds.size.height > bounds.size.width;
    // Portrait reserves two finger-sized rows for the persistent party roster
    // between the world and the lower controls, including on short phones.
    CGFloat side = portrait ? MIN(a.size.width - 16, a.size.height - 386)
                            : MIN(a.size.height - 16, a.size.width - 350);
    side = MAX(100, side);
    return CGRectMake(portrait ? a.origin.x + (a.size.width - side) / 2 : a.origin.x + 184,
                      a.origin.y + (portrait ? 66 : 8), side, side);
}
static inline CGRect zu4SheetFrame(CGRect bounds, UIEdgeInsets safe) {
    CGRect a = UIEdgeInsetsInsetRect(bounds, safe), map = zu4MapFrame(bounds, safe);
    if (bounds.size.height > bounds.size.width)
        return CGRectMake(a.origin.x + 8, CGRectGetMaxY(map) + 8, a.size.width - 16,
                          MAX(80, CGRectGetMaxY(a) - CGRectGetMaxY(map) - 16));
    CGFloat x = MIN(CGRectGetMaxX(map) + 16, CGRectGetMaxX(a) - 340);
    return CGRectMake(x, a.origin.y + 8, MAX(100, CGRectGetMaxX(a) - x - 8), a.size.height - 16);
}
// Preserve the complete 320x200 opening artwork, but anchor its bottom to the
// world instead of vertically centering it in the entire phone screen.
static inline CGRect zu4TitleFrame(CGRect bounds, UIEdgeInsets safe) {
    CGRect area = UIEdgeInsetsInsetRect(bounds, safe), map = zu4MapFrame(bounds, safe);
    BOOL portrait = bounds.size.height > bounds.size.width;
    CGFloat width = portrait ? map.size.width : MIN(area.size.width - 16,
        (CGRectGetMaxY(map) - area.origin.y - 8) * 1.6);
    CGFloat height = width / 1.6;
    return CGRectMake(CGRectGetMidX(area) - width / 2,
                      CGRectGetMaxY(map) - height, width, height);
}
static inline CGFloat zu4ActionWidth(CGRect bounds, UIEdgeInsets safe) {
    CGFloat width = UIEdgeInsetsInsetRect(bounds, safe).size.width;
    return bounds.size.height > bounds.size.width ? MIN(76, MAX(64, (width - 172) / 2)) : 76;
}
static inline CGRect zu4ActionFrame(CGRect bounds, UIEdgeInsets safe, BOOL flip, int column, int row) {
    CGRect area = UIEdgeInsetsInsetRect(bounds, safe);
    CGFloat width = zu4ActionWidth(bounds, safe), span = width * 2 + 6;
    CGFloat x = flip ? area.origin.x + 8 : CGRectGetMaxX(area) - span - 8;
    return CGRectMake(x + (flip ? 1 - column : column) * (width + 6),
                      CGRectGetMaxY(area) - 217 + row * 53, width, 48);
}
static inline CGRect zu4DpadFrame(CGRect bounds, UIEdgeInsets safe, int size, BOOL flip) {
    CGRect area = UIEdgeInsetsInsetRect(bounds, safe), map = zu4MapFrame(bounds, safe);
    CGFloat available = bounds.size.height > bounds.size.width
        ? area.size.width - 24 - (zu4ActionWidth(bounds, safe) * 2 + 6)
        : MIN(CGRectGetMaxX(area) - CGRectGetMaxX(map) - 8, map.origin.x - area.origin.x - 8);
    CGFloat requested = size == 0 ? 44 : (size == 2 ? 64 : 52);
    CGFloat button = MAX(44, MIN(requested, floor((available - 10) / 3)));
    CGFloat span = button * 3 + 10;
    return CGRectMake(flip ? CGRectGetMaxX(area) - span - 8 : area.origin.x + 8,
                      CGRectGetMaxY(area) - span - 10, span, span);
}
static inline CGRect zu4DirectionFrame(CGRect bounds, UIEdgeInsets safe, int size, BOOL flip) {
    CGRect area = UIEdgeInsetsInsetRect(bounds, safe), dpad = zu4DpadFrame(bounds, safe, size, flip);
    CGFloat width = MIN(260, MIN(area.size.width * 0.48, area.size.width - dpad.size.width - 32));
    return CGRectMake(flip ? area.origin.x + 8 : CGRectGetMaxX(area) - width - 8,
                      CGRectGetMaxY(area) - 188, width, 180);
}
#endif
