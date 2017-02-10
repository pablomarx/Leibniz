//
//  AppDelegate.m
//  Leibniz
//
//  Created by Steve White on 9/10/14.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#import "AppDelegate.h"
#import "ListenerWindowController.h"

#include "newton.h"
#include "runt.h"
#include "monitor.h"
#include "HammerConfigBits.h"

#define DegreesToRadians(r)    ((r) * M_PI / 180.0)

int32_t leibniz_sys_open(void *ext, const char *cStrName, int mode);
int32_t leibniz_sys_close(void *ext, uint32_t fildes);
int32_t leibniz_sys_istty(void *ext, uint32_t fildes);
int32_t leibniz_sys_read(void *ext, uint32_t fildes, void *buf, uint32_t nbyte);
int32_t leibniz_sys_write(void *ext, uint32_t fildes, const void *buf, uint32_t nbyte);
int32_t leibniz_sys_set_input_notify(void *ext, uint32_t fildes, uint32_t addr);


@interface LeibnizFile : NSObject
@property (strong) ListenerWindowController *listener;
@property (strong) NSString *name;
@property (strong) NSArray *openDescriptors;
@property (assign) uint32_t notifyAddress;
@end

@interface AppDelegate () {
  dispatch_queue_t _emulatorQueue;
  newton_t *_newton;
}

@property (weak) IBOutlet NSWindow *window;
@property (weak) IBOutlet EmulatorView *screenView;
@property (strong) NSArray *files;

- (IBAction) toggleNicdSwitch:(id)sender;
- (IBAction) toggleCardLockSwitch:(id)sender;
- (IBAction) togglePowerSwitch:(id)sender;

@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
  self.files = @[];
  
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
    newton_set_newt_config(_newton, kConfigBit3 | kDontPauseCPU | kStopOnThrows | kEnableStdout | kDefaultStdioOn | kEnableListener);
    newton_set_debugger_bits(_newton, 1);
    newton_set_bootmode(_newton, NewtonBootModeDiagnostics);
    newton_set_tapfilecntl_funtcions(_newton,
                     (__bridge void *)self,
                     leibniz_sys_open,
                     leibniz_sys_close,
                     leibniz_sys_istty,
                     leibniz_sys_read,
                     leibniz_sys_write,
                     leibniz_sys_set_input_notify);
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

#pragma mark - TapFileCntl
- (LeibnizFile *) fileWithDescriptor:(uint32_t)fp {
  for (LeibnizFile *aFile in self.files) {
    if ([aFile.openDescriptors containsObject:@(fp)] == YES) {
      return aFile;
    }
  }
  return nil;
}

- (int32_t) openFileNamed:(NSString *)name mode:(int)mode {
  BOOL isTTY = [name hasPrefix:@"%"];
  if (isTTY == NO && [name isEqualToString:@"bootNoCalibrate"] == NO) {
    return -1;
  }
  NSLog(@"%s %@ %i", __PRETTY_FUNCTION__, name, mode);
  int32_t fp = 0;
  LeibnizFile *file = nil;
  for (LeibnizFile *aFile in self.files) {
    if ([aFile.name isEqualToString:name] == YES) {
      file = aFile;
    }
    for (NSNumber *aDescriptor in aFile.openDescriptors) {
      fp = MAX(fp, [aDescriptor intValue]);
    }
  }
  
  fp = fp + 1;
  
  if (file == nil) {
    file = [[LeibnizFile alloc] init];
    file.name = name;
    file.openDescriptors = @[];
    if (isTTY == YES) {
      ListenerWindowController *listener = [[ListenerWindowController alloc] init];
      listener.window.title = [name substringFromIndex:1];
      [listener showWindow:self];
      file.listener = listener;
    }
    self.files = [self.files arrayByAddingObject:file];
  }
  file.openDescriptors = [file.openDescriptors arrayByAddingObject:@(fp)];
  
  return fp;
}

- (int32_t) closeFileWithDescriptor:(uint32_t)fp {
  LeibnizFile *file = [self fileWithDescriptor:fp];
  if (file == nil) {
    return -1;
  }
  
  NSMutableArray *openDescriptors = [file.openDescriptors mutableCopy];
  [openDescriptors removeObject:@(fp)];
  file.openDescriptors = openDescriptors;
  
  if ([openDescriptors count] == 0) {
    NSMutableArray *files = [self.files mutableCopy];
    [files removeObject:file];
    self.files = files;
    
    [file.listener close];
  }
  
  return 0;
}

- (int32_t) writeData:(NSData *)data toFileWithDescriptor:(uint32_t)fp {
  LeibnizFile *file = [self fileWithDescriptor:fp];
  if (file == nil) {
    return 0;
  }
  
  NSString *string = [[NSString alloc] initWithData:data encoding:NSASCIIStringEncoding];
  [file.listener appendOutput:string];
  return (int32_t)[data length];
}

- (int32_t) setInputNotifyAddress:(uint32_t)address forFileWithDescriptor:(uint32_t)fp {
  LeibnizFile *file = [self fileWithDescriptor:fp];
  if (file == nil) {
    return 0;
  }
  file.notifyAddress = address;
  return 1;
}

int32_t leibniz_sys_open(void *ext, const char *cStrName, int mode) {
  __block int32_t result = -1;
  dispatch_sync(dispatch_get_main_queue(), ^{
    NSString *name = [NSString stringWithCString:cStrName encoding:NSASCIIStringEncoding];
    AppDelegate *self = (__bridge AppDelegate *)ext;
    result = [self openFileNamed:name mode:mode];
  });
  return result;
}

int32_t leibniz_sys_close(void *ext, uint32_t fildes) {
  __block int32_t result = -1;
  dispatch_sync(dispatch_get_main_queue(), ^{
    AppDelegate *self = (__bridge AppDelegate *)ext;
    result = [self closeFileWithDescriptor:fildes];
  });
  return result;
}

int32_t leibniz_sys_istty(void *ext, uint32_t fildes) {
  __block int32_t result = 1;
  /*
   dispatch_sync(dispatch_get_main_queue(), ^{
   AppDelegate *self = (__bridge AppDelegate *)ext;
   result = [self fileWithFPIsRegular:fildes];
   });
   */
  return result;
}

int32_t leibniz_sys_read(void *ext, uint32_t fildes, void *buf, uint32_t nbyte) {
  return -1;
}

int32_t leibniz_sys_write(void *ext, uint32_t fildes, const void *buf, uint32_t nbyte) {
  NSData *data = [NSData dataWithBytes:buf length:nbyte];
  __block int32_t result = 0;
  dispatch_sync(dispatch_get_main_queue(), ^{
    AppDelegate *self = (__bridge AppDelegate *)ext;
    result = [self writeData:data toFileWithDescriptor:fildes];
  });
  return (nbyte - result);
}

int32_t leibniz_sys_set_input_notify(void *ext, uint32_t fildes, uint32_t addr) {
  __block int32_t result = 0;
  dispatch_sync(dispatch_get_main_queue(), ^{
    AppDelegate *self = (__bridge AppDelegate *)ext;
    result = [self setInputNotifyAddress:addr forFileWithDescriptor:fildes];
  });
  return result;
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

@implementation LeibnizFile
@end
