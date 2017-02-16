//
//  ListenerWindowController.h
//  Leibniz
//
//  Created by Steve White on 2/10/17.
//  Copyright © 2017 Steve White. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@protocol ListenerWindowDelegate;

@interface ListenerWindowController : NSWindowController

@property (weak) id<ListenerWindowDelegate> delegate;

- (void) appendOutput:(NSString *)output;

@end

@protocol ListenerWindowDelegate <NSObject>

- (void) listenerWindow:(ListenerWindowController *)listener insertedText:(NSString *)text;
- (NSInteger) listenerWindow:(ListenerWindowController *)listener deleteForProposedLength:(NSInteger)characterCount;

@end
