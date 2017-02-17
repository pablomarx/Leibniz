//
//  PowerButton.m
//  Leibniz
//
//  Created by Steve White on 2/16/17.
//  Copyright © 2017 Steve White. All rights reserved.
//

#import "PowerButton.h"

@implementation PowerButton

- (instancetype) init {
  self = [super init];
  if (self != nil) {
    self.image = [NSImage imageNamed:@"button_power"];
    self.alternateImage = [NSImage imageNamed:@"button_power_on"];
    [self setButtonType:NSButtonTypeMomentaryChange];
    self.bordered = NO;
  }
  return self;
}

- (void)sizeToFit {
  NSRect frame = self.frame;
  frame.size = self.image.size;
  self.frame = frame;
}

@end
