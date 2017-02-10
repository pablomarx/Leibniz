//
//  EmulatorView.m
//  Leibniz
//
//  Created by Steve White on 9/10/14.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#import "EmulatorView.h"

#define DegreesToRadians(r)  ((r) * M_PI / 180.0)

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
      _screenLayer.contentsGravity = kCAGravityCenter;
      [self.layer addSublayer:_screenLayer];
  }
  return self;
}

- (void) setFrame:(NSRect)frame {
  _screenLayer.transform = CATransform3DIdentity;
  [super setFrame:frame];

  _screenLayer.frame = self.bounds;
  _screenLayer.transform = CATransform3DMakeRotation(DegreesToRadians(270), 0, 0, 1);
}

- (void) setEmulatorImage:(CGImageRef)emulatorImage {
  _screenLayer.contents = (__bridge id)emulatorImage;
}


@end
