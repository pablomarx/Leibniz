//
//  EmulatorView.h
//  Leibniz
//
//  Created by Steve White on 9/10/14.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface EmulatorView : NSOpenGLView

- (void) updateWithFramebuffer:(const uint8_t *)framebuffer width:(uint16_t)width height:(uint16_t)height;

@end
