//
//  AppDelegate.m
//  Leibniz
//
//  Created by Steve White on 9/10/14.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#import "AppDelegate.h"
#import "EmulatorView.h"
#import "ListenerWindowController.h"

#include "newton.h"
#include "runt.h"
#include "monitor.h"
#include "HammerConfigBits.h"

int32_t leibniz_sys_open(void *ext, const char *cStrName, int mode);
int32_t leibniz_sys_close(void *ext, uint32_t fildes);
int32_t leibniz_sys_istty(void *ext, uint32_t fildes);
int32_t leibniz_sys_read(void *ext, uint32_t fildes, void *buf, uint32_t nbyte);
int32_t leibniz_sys_write(void *ext, uint32_t fildes, const void *buf, uint32_t nbyte);
int32_t leibniz_sys_set_input_notify(void *ext, uint32_t fildes, uint32_t addr);
void leibniz_system_panic(newton_t *newton, const char *msg);
void leibniz_debugstr(newton_t *newton, const char *msg);
void leibniz_undefined_opcode(newton_t *newton, uint32_t opcode);

@interface LeibnizFile : NSObject
@property (strong) ListenerWindowController *listener;
@property (strong) NSString *name;
@property (strong) NSArray *openDescriptors;
@property (assign) uint32_t notifyAddress;
@property (strong) NSMutableData *inputBuffer;
@end

@interface AppDelegate ()<ListenerWindowDelegate> {
  dispatch_queue_t _emulatorQueue;
  newton_t *_newton;
}

@property (weak) IBOutlet NSWindow *window;
@property (weak) IBOutlet EmulatorView *screenView;
@property (weak) IBOutlet NSView *buttonBarView;
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
    dispatch_async(dispatch_get_main_queue(), ^{
      [self beginEmulatingWithROMFile: romFile];
    });
  }
  else {
    [NSApp terminate:self];
  }
}

- (void) createEmulatorWithROMFile:(NSString *)romFile {
  _newton = newton_new();
  newton_load_rom(_newton, [romFile fileSystemRepresentation]);
  newton_set_newt_config(_newton, kConfigBit3 | kDontPauseCPU | kEnableStdout | kDefaultStdioOn | kEnableListener);
  newton_set_debugger_bits(_newton, 1);
  newton_set_bootmode(_newton, NewtonBootModeDiagnostics);
  newton_set_undefined_opcode(_newton, leibniz_undefined_opcode);
  newton_set_system_panic(_newton, leibniz_system_panic);
  newton_set_debugstr(_newton, leibniz_debugstr);
  newton_set_tapfilecntl_funtcions(_newton,
                   (__bridge void *)self,
                   leibniz_sys_open,
                   leibniz_sys_close,
                   leibniz_sys_istty,
                   leibniz_sys_read,
                   leibniz_sys_write,
                   leibniz_sys_set_input_notify);
  
  _emulatorQueue = dispatch_queue_create("org.swhite.leibniz.emulator", NULL);
}

- (void) beginEmulatingWithROMFile:(NSString *)romFile {
  [self createEmulatorWithROMFile:romFile];
  [self runEmulator];
}

- (void) runEmulator {
  dispatch_async(_emulatorQueue, ^{
    newton_emulate(_newton, INT32_MAX);
  });
}

- (IBAction) toggleNicdSwitch:(id)sender {
  BOOL on = ([sender state] == NSOnState ? 0 : 1);
  runt_switch_state(_newton->runt, RuntSwitchNicad, on);
  [sender setState:(!on ? NSOffState : NSOnState)];
}

- (IBAction) toggleCardLockSwitch:(id)sender {
  BOOL on = ([sender state] == NSOnState ? 0 : 1);
  runt_switch_state(_newton->runt, RuntSwitchCardLock, on);
  [sender setState:(!on ? NSOffState : NSOnState)];
}

- (IBAction) togglePowerSwitch:(id)sender {
  BOOL on = ([sender state] == NSOnState ? 0 : 1);
  runt_switch_state(_newton->runt, RuntSwitchPower, on);
  [sender setState:(!on ? NSOffState : NSOnState)];
}

- (void) tabletTouchedDownAtPoint:(NSPoint)point {
  runt_touch_down(_newton->runt, point.x, point.y);
}

- (void) tabletDraggedToPoint:(NSPoint)point {
  runt_touch_down(_newton->runt, point.x, point.y);
}

- (void) tabletTouchedUpAtPoint:(NSPoint)point {
  runt_touch_up(_newton->runt);
}

#pragma mark - Display
- (void) createDisplayWithWidth:(int)width height:(int)height {
  NSSize buttonBarSize = self.buttonBarView.bounds.size;
  NSSize contentSize = NSZeroSize;
  contentSize.width = MAX(width, buttonBarSize.width);
  
  NSRect screenFrame = NSZeroRect;
  screenFrame.origin.x = floorf((width - contentSize.width)/2);
  screenFrame.origin.y = buttonBarSize.height;
  screenFrame.size.width = width;
  screenFrame.size.height = height;
  self.screenView.frame = screenFrame;
  
  contentSize.height = NSMaxY(screenFrame);
  
  [self.window setContentSize:contentSize];
  [self.window makeKeyAndOrderFront:self];
}

- (void) updateEmulatorViewWithData:(const uint8_t *)data width:(uint16_t)width height:(uint16_t)height {
  [self.screenView updateWithFramebuffer:data width:width height:height];
}

void newton_display_open(int width, int height) {
  dispatch_async(dispatch_get_main_queue(), ^{
    [(AppDelegate *)[NSApp delegate] createDisplayWithWidth:width height:height];
  });
}

void newton_display_set_framebuffer(const uint8_t *display, int width, int height) {
  [(AppDelegate *)[NSApp delegate] updateEmulatorViewWithData:display width:width height:height];
}

#pragma mark - Errors
- (void) showErrorAlertWithTitle:(NSString *)title
             message:(NSString *)message
{
  if ([NSThread isMainThread] == NO) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [self showErrorAlertWithTitle:title message:message];
    });
    return;
  }
  
  NSInteger result = NSRunAlertPanel(title,
                     @"%@",
                     NSLocalizedString(@"Continue", @"Continue"),
                     NSLocalizedString(@"Stop", @"Stop"),
                     nil,
                     message);
  if (result == NSOKButton) {
    [self runEmulator];
  }
}

void leibniz_system_panic(newton_t *newton, const char *msg) {
  NSString *message = [NSString stringWithCString:msg encoding:NSASCIIStringEncoding];
  [(AppDelegate *)[NSApp delegate] showErrorAlertWithTitle:NSLocalizedString(@"System Panic", @"System Panic")
                           message:message];
}

void leibniz_debugstr(newton_t *newton, const char *msg) {
	NSString *message = [NSString stringWithCString:msg encoding:NSASCIIStringEncoding];
	[(AppDelegate *)[NSApp delegate] showErrorAlertWithTitle:NSLocalizedString(@"DebugStr", @"DebugStr")
													 message:message];
}

void leibniz_undefined_opcode(newton_t *newton, uint32_t opcode) {
  NSString *message = [NSString stringWithFormat:@"0x%08x", opcode];
  [(AppDelegate *)[NSApp delegate] showErrorAlertWithTitle:NSLocalizedString(@"Undefined Opcode", @"Undefined Opcode")
                           message:message];
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
  if (isTTY == NO) {
    return -1;
  }
  
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
      listener.delegate = self;
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

- (int32_t) readForFileWithDescriptor:(uint32_t)fp intoBuffer:(void *)buffer maxLength:(uint32_t)maxLength {
  LeibnizFile *file = [self fileWithDescriptor:fp];
  if (file == nil) {
    return -1;
  }
  
  NSData *inputBuffer = file.inputBuffer;
  if (inputBuffer == nil || [inputBuffer length] == 0) {
    return 0;
  }
  
  const uint8_t *input = [inputBuffer bytes];
  int32_t length = MIN(maxLength, (int32_t)[inputBuffer length]);
  memcpy(buffer, input, length);
  
  [file.inputBuffer replaceBytesInRange:NSMakeRange(0, length) withBytes:NULL length:0];
  if ([file.inputBuffer length] == 0) {
    newton_file_input_notify(_newton, file.notifyAddress, 0);
  }
  
  return length;
}

- (int32_t) setInputNotifyAddress:(uint32_t)address forFileWithDescriptor:(uint32_t)fp {
  LeibnizFile *file = [self fileWithDescriptor:fp];
  if (file == nil) {
    return -1;
  }
  
  file.notifyAddress = address;
  return 0;
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
  __block int32_t result = -1;
  dispatch_sync(dispatch_get_main_queue(), ^{
    AppDelegate *self = (__bridge AppDelegate *)ext;
    LeibnizFile *file = [self fileWithDescriptor:fildes];
    if (file == nil) {
      result = -1;
    }
    result = (file.listener != nil);
  });
  return result;
}

int32_t leibniz_sys_read(void *ext, uint32_t fildes, void *buf, uint32_t nbyte) {
  __block int32_t result = 0;
  dispatch_sync(dispatch_get_main_queue(), ^{
    AppDelegate *self = (__bridge AppDelegate *)ext;
    result = [self readForFileWithDescriptor:fildes intoBuffer:buf maxLength:nbyte];
  });
  return result;
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

#pragma mark -
- (LeibnizFile *) fileForListenerWindow:(ListenerWindowController *)listener {
  for (LeibnizFile *aFile in self.files) {
    if (aFile.listener == listener) {
      return aFile;
    }
  }
  return nil;
}

- (NSInteger) listenerWindow:(ListenerWindowController *)listener deleteForProposedLength:(NSInteger)characterCount {
  LeibnizFile *file = [self fileForListenerWindow:listener];
  if (file == nil) {
    return 0;
  }
  
  NSInteger inputBufferLen = [file.inputBuffer length];
  
  NSRange deletionRange;
  deletionRange.length = MIN(inputBufferLen, characterCount);
  deletionRange.location = inputBufferLen - deletionRange.length;
  [file.inputBuffer replaceBytesInRange:deletionRange withBytes:NULL length:0];
  
  return deletionRange.length;
}

- (void) listenerWindow:(ListenerWindowController *)listener insertedText:(NSString *)text {
  LeibnizFile *file = [self fileForListenerWindow:listener];
  if (file == nil) {
    return;
  }
  
  [file.inputBuffer appendData:[text dataUsingEncoding:NSASCIIStringEncoding]];
  
  NSRange newLineRange = [text rangeOfString:@"\n"];
  if (newLineRange.location != NSNotFound) {
    newton_file_input_notify(_newton, file.notifyAddress, 1);
  }
}

@end

@implementation LeibnizFile

- (instancetype) init {
  self = [super init];
  if (self != nil) {
  self.inputBuffer = [NSMutableData data];
  }
  return self;
}

@end
