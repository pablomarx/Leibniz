//
//  AppDelegate.h
//  Leibniz
//
//  Created by Steve White on 9/10/14.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "FileStream.h"
#import "ListenerWindowController.h"
#import "EmulatorView.h"
#include "newton.h"

@interface AppDelegate : NSObject <NSApplicationDelegate, FileStreamDelegate, ListenerWindowDelegate> {
  NSWindow *_window;
  EmulatorView *_screenView;
  NSView *_buttonBarView;

  dispatch_queue_t _emulatorQueue;
  newton_t *_newton;
  NSArray *_files;
  
  FileStream *_fileStream;
  ListenerWindowController *_consoleWindow;
}

@property (assign) IBOutlet NSWindow *window;
@property (assign) IBOutlet EmulatorView *screenView;
@property (assign) IBOutlet NSView *buttonBarView;
@property (strong) NSArray *files;

- (IBAction) togglePowerSwitch:(id)sender;
- (IBAction) showConsole:(id)sender;
- (IBAction) diagnosticsReboot:(id)sender;
- (IBAction) warmReboot:(id)sender;
- (IBAction) coldReboot:(id)sender;
- (IBAction) insertSRAMCard:(id)sender;

@end
