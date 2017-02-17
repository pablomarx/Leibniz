#import <AppKit/AppKit.h>

@interface NSWindow (AccessoryView)

-(void)addViewToTitleBar:(NSView*)viewToAdd atXPosition:(CGFloat)x;
-(CGFloat)heightOfTitleBar;

@end
