//
//  AppDelegate.m
//  Leibniz
//
//  Created by Steve White on 9/10/14.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#import "AppDelegate.h"
#import "EmulatorView.h"
#import "FileStream.h"
#import "LeibnizFile.h"
#import "ListenerWindowController.h"
#import "NSWindow+AccessoryView.h"
#import "PowerButtonAccessoryController.h"

#include "docker.h"
#include "newton.h"
#include "runt.h"
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
void docker_connected(void *ctx);
void docker_disconnected(void *ctx);
void docker_install_progress(void *ctx, double progress);

@implementation AppDelegate

@synthesize window = _window;
@synthesize screenView = _screenView;
@synthesize buttonBarView = _buttonBarView;
@synthesize files = _files;

- (void) dealloc {
  [_files release], _files = nil;
  [super dealloc];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
  self.files = @[];
  
  [self setupTitlebarAccessory];
  [self createConsoleWindowAndFileStream];
  [self openDocument:self];
}

- (void) createConsoleWindowAndFileStream {
  _consoleWindow = [[ListenerWindowController alloc] init];
  _consoleWindow.window.title = NSLocalizedString(@"Console", @"Console");
  _consoleWindow.window.frameAutosaveName = @"Console";
  
  _fileStream = [[FileStream alloc] init];
  _fileStream.delegate = self;
}

- (void) closeListenerWindows {
  for (LeibnizFile *aFile in self.files) {
    if (aFile.listener != nil) {
      [aFile.listener close];
    }
  }
  self.files = @[];
}

- (void) createEmulatorWithROMFile:(NSString *)romFile {
  _newton = newton_new();
  newton_set_logfile(_newton, _fileStream.file);
  
  int success = newton_load_rom(_newton, [romFile fileSystemRepresentation]);
  if (success != 0) {
    newton_del(_newton);
    _newton = NULL;
    [self showErrorAlertWithTitle:NSLocalizedString(@"Unsupported file", @"Unsupported file")
                          message:NSLocalizedString(@"The file does not appear to be a supported ROM.", @"The file does not appear to be a supported ROM.")];
    return;
  }
  
  NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];
  uint32_t newtonLogFlags = (uint32_t)[userDefaults integerForKey:@"newtonLogFlags"];
  uint32_t runtLogFlags = (uint32_t)[userDefaults integerForKey:@"runtLogFlags"];
  
  newton_set_log_flags(_newton, newtonLogFlags, 1);
  runt_set_log_flags(newton_get_runt(_newton), runtLogFlags, 1);
  
  newton_set_newt_config(_newton, kConfigBit11 | kConfigBit3 | /*kDontPauseCPU |*/ kEnableStdout | kDefaultStdioOn | kEnableListener);
  newton_set_debugger_bits(_newton, 1);
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
  
  
  docker_set_callbacks(newton_get_docker(_newton), (__bridge void *)self, docker_connected, docker_disconnected, docker_install_progress);

  _emulatorQueue = dispatch_queue_create("org.swhite.leibniz.emulator", NULL);
}

- (void) beginEmulatingWithROMFile:(NSString *)romFile {
  if (_newton != NULL) {
    [self stopEmulator:^{
      newton_del(_newton);
      _newton = NULL;
      dispatch_async(dispatch_get_main_queue(), ^{
        [self.window close];
        [self closeListenerWindows];
        [self beginEmulatingWithROMFile:romFile];
      });
    }];
    return;
  }

  [self createEmulatorWithROMFile:romFile];
  [self runEmulator];
}

- (void) runEmulator {
  if (_emulatorQueue == NULL || _newton == NULL) {
    return;
  }
  
  dispatch_async(_emulatorQueue, ^{
    newton_emulate(_newton, INT32_MAX);
  });
}

- (void) stopEmulator:(void(^)())completion {
  if (_newton == NULL) {
    completion();
    return;
  }
  
  newton_stop(_newton);
  dispatch_async(_emulatorQueue, ^{
    completion();
  });
}

- (void) rebootNewtonWithStyle:(NewtonRebootStyle)style bootMode:(NewtonBootMode)bootMode {
  if (_newton == NULL) {
    return;
  }
  
  [self stopEmulator:^{
    newton_reboot(_newton, style);
    newton_set_bootmode(_newton, bootMode);
    dispatch_async(dispatch_get_main_queue(), ^{
      if (style == NewtonRebootStyleCold) {
        [self closeListenerWindows];
      }
      [self runEmulator];
    });
  }];
}


- (IBAction) openDocument:(id)sender {
  NSOpenPanel *openPanel = [NSOpenPanel openPanel];
  NSInteger result = [openPanel runModal];
  
  if (result == NSFileHandlingPanelOKButton) {
    NSString *romFile = [[openPanel URL] path];

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
      [self beginEmulatingWithROMFile: romFile];
    });
  }
}

- (void) setupTitlebarAccessory {
  PowerButton *button = nil;
  if ([NSWindow instancesRespondToSelector:@selector(addTitlebarAccessoryViewController:)] == YES) {
    PowerButtonAccessoryController *titlebarAccessory = [[PowerButtonAccessoryController alloc] init];
    button = titlebarAccessory.powerButton;
    [self.window addTitlebarAccessoryViewController:titlebarAccessory];
    [titlebarAccessory release];
  }
  else {
    button = [[PowerButton alloc] init];
    [button sizeToFit];
    CGFloat xPos = self.window.frame.size.width - button.frame.size.width - 10;
    [self.window addViewToTitleBar:button atXPosition:xPos];
  }
  
  [button setTarget:self];
  [button setAction:@selector(togglePowerSwitch:)];
}

#pragma mark - Actions
- (IBAction) togglePowerSwitch:(id)sender {
  runt_switch_toggle(_newton->runt, RuntSwitchPower);
}

- (IBAction) showConsole:(id)sender {
  [_consoleWindow showWindow:sender];
}

- (IBAction) diagnosticsReboot:(id)sender {
  [self rebootNewtonWithStyle:NewtonRebootStyleCold bootMode:NewtonBootModeDiagnostics];
}

- (IBAction) coldReboot:(id)sender {
  [self rebootNewtonWithStyle:NewtonRebootStyleCold bootMode:NewtonBootModeNormal];
}

- (IBAction) warmReboot:(id)sender {
  [self rebootNewtonWithStyle:NewtonRebootStyleWarm bootMode:NewtonBootModeNormal];
}

- (IBAction) insertSRAMCard:(id)sender {
  BOOL inserted = ([sender state] == NSOnState ? 0 : 1);
  [sender setState:(!inserted ? NSOffState : NSOnState)];

  pcmcia_t *pcmcia = newton_get_pcmcia(_newton);
  pcmcia_set_card_inserted(pcmcia, inserted);
}

#pragma mark - Tablet

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
  
  NSString *okayButton = nil;
  NSString *cancelButton = nil;
  if (_newton != NULL) {
    okayButton = NSLocalizedString(@"Continue", @"Continue");
    cancelButton = NSLocalizedString(@"Stop", @"Stop");
  }
  
  NSInteger result = NSRunAlertPanel(title,
                                     @"%@",
                                     okayButton,
                                     cancelButton,
                                     nil,
                                     message);
  if (result == NSOKButton) {
    if (_newton != NULL) {
      [self runEmulator];
    }
    else {
      [self showOpenROMPanel];
    }
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
    file = [[[LeibnizFile alloc] init] autorelease];
    file.name = name;
    file.openDescriptors = @[];
    self.files = [self.files arrayByAddingObject:file];

    if (isTTY == YES) {
      ListenerWindowController *listener = [[[ListenerWindowController alloc] init] autorelease];
      listener.window.title = [name substringFromIndex:1];
      listener.window.frameAutosaveName = [name substringFromIndex:1];
      listener.delegate = self;
      [listener showWindow:self];
      file.listener = listener;
    }
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
  [string release];
  return (int32_t)[data length];
}

- (int32_t) readForFileWithDescriptor:(uint32_t)fp intoBuffer:(void *)buffer maxLength:(uint32_t)maxLength {
  LeibnizFile *file = [self fileWithDescriptor:fp];
  if (file == nil) {
    return -1;
  }
  
  NSData *inputBuffer = file.inputBuffer;
  if (inputBuffer == nil || [inputBuffer length] == 0) {
    if (file.listener == nil) {
      return -1;
    }
    return 0;
  }
  
  const uint8_t *input = [inputBuffer bytes];
  int32_t length = MIN(maxLength, (int32_t)[inputBuffer length]);
  memcpy(buffer, input, length);
  
  [file.inputBuffer replaceBytesInRange:NSMakeRange(0, length) withBytes:NULL length:0];
  if ([file.inputBuffer length] == 0 && file.notifyAddress != 0) {
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
  NSData *data = [[NSData alloc] initWithBytes:buf length:nbyte];
  __block int32_t result = 0;
  dispatch_sync(dispatch_get_main_queue(), ^{
    AppDelegate *self = (__bridge AppDelegate *)ext;
    result = [self writeData:data toFileWithDescriptor:fildes];
  });
  [data release];
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

#pragma mark -
- (void) fileStream:(FileStream *)fileStream wroteData:(NSData *)data {
  NSString *string = [[NSString alloc] initWithData:data encoding:NSASCIIStringEncoding];
  [_consoleWindow appendOutput:string];
  [string release];
}

- (void) fileStreamClosed:(FileStream *)fileStream {
  
}

#pragma mark -
- (void) showOpenPackagePanel {
  NSOpenPanel *openPanel = [NSOpenPanel openPanel];
  openPanel.title = NSLocalizedString(@"Select a package to install", @"Select a package to install");
  
  NSInteger result = [openPanel runModal];
  
  if (result == NSFileHandlingPanelOKButton) {
    NSString *packageFile = [[openPanel URL] path];
    docker_install_package_at_path(newton_get_docker(_newton), [packageFile fileSystemRepresentation]);
  }
  
  [self runEmulator];
}

- (void) dockerConnected {
  // We'll stop the emulator so the connection doesn't
  // time out.
  [self stopEmulator:^{
    dispatch_async(dispatch_get_main_queue(), ^{
      [self showOpenPackagePanel];
    });
  }];
}

- (void) dockerDisconnected {
  NSLog(@"%s", __PRETTY_FUNCTION__);
}

void docker_connected(void *ctx) {
  AppDelegate *self = (__bridge AppDelegate *)ctx;
  [self dockerConnected];
}

void docker_disconnected(void *ctx) {
  AppDelegate *self = (__bridge AppDelegate *)ctx;
  [self dockerDisconnected];
}

void docker_install_progress(void *ctx, double progress) {
  NSLog(@"%s %f", __PRETTY_FUNCTION__, progress);
}


@end

