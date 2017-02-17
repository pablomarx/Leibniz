//
//  PowerButtonAccessoryController.m
//  Leibniz
//
//  Created by Steve White on 2/16/17.
//  Copyright © 2017 Steve White. All rights reserved.
//

#import "PowerButtonAccessoryController.h"

asm(".weak_reference _OBJC_CLASS_$_NSTitlebarAccessoryViewController");
asm(".weak_reference _OBJC_METACLASS_$_NSTitlebarAccessoryViewController");

@interface PowerButtonAccessoryController()
@property (strong, nonatomic) PowerButton *powerButton;
@end

@implementation PowerButtonAccessoryController

- (PowerButton *) powerButton {
  if (_powerButton == nil) {
    _powerButton = [[PowerButton alloc] init];
  }
  return _powerButton;
}

- (void) loadView {
  self.layoutAttribute = NSLayoutAttributeRight;
  
  PowerButton *button = self.powerButton;
  button.autoresizingMask = NSViewMinYMargin | NSViewMaxYMargin;
  [button sizeToFit];
  NSRect buttonFrame = button.frame;
  
  NSRect wrapperFrame = buttonFrame;
  wrapperFrame.size.width += 10;
  
  NSView *wrapper = [[NSView alloc] initWithFrame:wrapperFrame];
  
  [wrapper addSubview:button];
  self.view = wrapper;
  self.powerButton = button;
}

@end
