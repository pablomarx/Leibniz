//
//  AppDelegate.m
//  Leibniz
//
//  Created by Steve White on 9/10/14.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#import "AppDelegate.h"

#include "newton.h"
#include "runt.h"
#include "monitor.h"

#define DegreesToRadians(r)      ((r) * M_PI / 180.0)

@interface AppDelegate () {
  dispatch_queue_t _emulatorQueue;
  newton_t *_newton;
}

@property (weak) IBOutlet NSWindow *window;
@property (weak) IBOutlet EmulatorView *screenView;

- (IBAction) toggleNicdSwitch:(id)sender;
- (IBAction) toggleCardLockSwitch:(id)sender;
- (IBAction) togglePowerSwitch:(id)sender;

@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
  NSOpenPanel *openPanel = [NSOpenPanel openPanel];
  NSInteger result = [openPanel runModal];
  if (result == NSFileHandlingPanelOKButton) {
    NSString *romFile = [[openPanel URL] path];
    [self.window makeKeyAndOrderFront:self];
    [self beginEmulatingWithROMFile: romFile];
  }
  else {
    [NSApp terminate:self];
  }
}

- (void) beginEmulatingWithROMFile:(NSString *)romFile {
  _emulatorQueue = dispatch_queue_create("org.swhite.leibniz.emulator", NULL);
  dispatch_async(_emulatorQueue, ^{
    _newton = newton_new();
    newton_load_rom(_newton, [romFile fileSystemRepresentation]);
    newton_set_bootmode(_newton, NewtonBootModeDiagnostics);
    newton_emulate(_newton, INT32_MAX);
  });
}

- (IBAction) toggleNicdSwitch:(id)sender {
  BOOL on = ([sender state] == NSOnState ? 0 : 1);
  runt_switch_state(_newton->runt, 0, on);
  [sender setState:(!on ? NSOffState : NSOnState)];
}

- (IBAction) toggleCardLockSwitch:(id)sender {
  BOOL on = ([sender state] == NSOnState ? 0 : 1);
  runt_switch_state(_newton->runt, 1, on);
  [sender setState:(!on ? NSOffState : NSOnState)];
}

- (IBAction) togglePowerSwitch:(id)sender {
  BOOL on = ([sender state] == NSOnState ? 0 : 1);
  runt_switch_state(_newton->runt, 2, on);
  [sender setState:(!on ? NSOffState : NSOnState)];
}


- (void) updateEmulatorViewWithData:(NSData *)data size:(NSSize)size {
  dispatch_sync(dispatch_get_main_queue(), ^{
    CGContextRef context = CGBitmapContextCreate((void *)[data bytes], size.width, size.height, 8,  size.width, CGColorSpaceCreateWithName(kCGColorSpaceGenericGray), kCGBitmapAlphaInfoMask & kCGImageAlphaNone);
    CGImageRef imageRef = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    
    [self.screenView setEmulatorImage:imageRef];

    CGImageRelease(imageRef);;
  });
}

- (void) mouseDown:(NSEvent *)theEvent {
  NSPoint location = [theEvent locationInWindow];
  runt_touch_down(_newton->runt, floorf(location.x), floorf(location.y));
}

-(void)mouseDragged:(NSEvent *)theEvent {
  NSPoint location = [theEvent locationInWindow];
  runt_touch_down(_newton->runt, floorf(location.x), floorf(location.y));
}

- (void) mouseUp:(NSEvent *)theEvent {
  runt_touch_up(_newton->runt);
}

@end

@implementation BlahView

- (void) mouseDown:(NSEvent *)theEvent {
  [(AppDelegate *)[NSApp delegate] mouseDown:theEvent];
}

-(void)mouseDragged:(NSEvent *)theEvent {
  [(AppDelegate *)[NSApp delegate] mouseDragged:theEvent];
}

- (void) mouseUp:(NSEvent *)theEvent {
  [(AppDelegate *)[NSApp delegate] mouseUp:theEvent];
}

@end


@implementation EmulatorView {
  CALayer *_screenLayer;
}

- (id) initWithFrame:(NSRect)frameRect {
  self = [super initWithFrame:frameRect];
  if (self != nil) {
    self.wantsLayer = YES;
    self.layer.backgroundColor = [[NSColor whiteColor] CGColor];

    _screenLayer = [CALayer layer];
    _screenLayer.backgroundColor = [[NSColor whiteColor] CGColor];
    _screenLayer.frame = CGRectMake(-48, 48, 336, 240);
    _screenLayer.transform = CATransform3DMakeRotation(DegreesToRadians(270), 0, 0, 1);
    _screenLayer.contentsGravity = kCAGravityCenter;
    [self.layer addSublayer:_screenLayer];
  }
  return self;
}

- (void) setEmulatorImage:(CGImageRef)emulatorImage {
  _screenLayer.contents = (__bridge id)emulatorImage;
}


@end

void FlushDisplay(const char *display, int width, int height) {
  NSData *data = [[NSData alloc] initWithBytes:display length:width*height];
  [(AppDelegate *)[NSApp delegate] updateEmulatorViewWithData:data size:NSMakeSize(width, height)];
  data = nil;
}
