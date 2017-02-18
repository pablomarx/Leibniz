//
//  EmulatorView.m
//  Leibniz
//
//  Created by Steve White on 9/10/14.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#import "EmulatorView.h"

#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>

@implementation EmulatorView

- (id) initWithFrame:(NSRect)frameRect {
    NSOpenGLPixelFormatAttribute att[] =
    {
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFAAccelerated,
        0
    };
    
    NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:att];

    self = [super initWithFrame:frameRect pixelFormat:pixelFormat];
    [pixelFormat release];
    return self;
}

- (void) updateWithFramebuffer:(const uint8_t *)framebuffer width:(uint16_t)width height:(uint16_t)height {
    [self.openGLContext makeCurrentContext];

    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, framebuffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    
    glTexCoord2f(0.0, 1.0);
    glVertex2f(-1.0, 1.0);
    
    glTexCoord2f(1.0, 1.0);
    glVertex2f(-1.0, -1.0);
    
    glTexCoord2f(1.0, 0.0);
    glVertex2f(1.0, -1.0);
    
    glTexCoord2f(0.0, 0.0);
    glVertex2f(1.0, 1.0);

    glEnd();

    [self.openGLContext flushBuffer];
}

@end
