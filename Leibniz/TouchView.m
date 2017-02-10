//
//  TouchView.m
//  Leibniz
//
//  Created by Steve White on 9/10/14.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#import "TouchView.h"

@implementation TouchView

- (void) mouseDown:(NSEvent *)theEvent {
  [self.delegate mouseDown:theEvent];
}

-(void)mouseDragged:(NSEvent *)theEvent {
  [self.delegate mouseDragged:theEvent];
}

- (void) mouseUp:(NSEvent *)theEvent {
  [self.delegate mouseUp:theEvent];
}

@end
