//
//  TouchView.h
//  Leibniz
//
//  Created by Steve White on 9/10/14.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface NSObject(TouchViewDelegate)
- (void) tabletTouchedDownAtPoint:(NSPoint)point;
- (void) tabletDraggedToPoint:(NSPoint)point;
- (void) tabletTouchedUpAtPoint:(NSPoint)point;
@end

@interface TouchView : NSView {
    id _delegate;
}

@property (assign) IBOutlet id delegate;

@end
