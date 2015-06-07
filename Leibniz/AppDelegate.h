//
//  AppDelegate.h
//  Leibniz
//
//  Created by Steve White on 9/10/14.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface AppDelegate : NSObject <NSApplicationDelegate>


@end


@interface EmulatorView : NSView

- (void) setEmulatorImage:(CGImageRef)image;

@end

@interface BlahView : NSView

@end