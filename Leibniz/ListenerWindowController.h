//
//  ListenerWindowController.h
//  Leibniz
//
//  Created by Steve White on 2/10/17.
//  Copyright © 2017 Steve White. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@protocol ListenerWindowDelegate;

@interface ListenerWindowController : NSWindowController<NSTextViewDelegate> {
    NSTextView *_textView;
    NSFont *_font;
    NSMutableDictionary *_attributes;
    id _delegate;
}

@property (strong) IBOutlet NSTextView *textView;
@property (strong) NSFont *font;
@property (strong) NSMutableDictionary *attributes;
@property (assign) id<ListenerWindowDelegate> delegate;

- (void) appendOutput:(NSString *)output;

@end

@protocol ListenerWindowDelegate <NSObject>

- (void) listenerWindow:(ListenerWindowController *)listener insertedText:(NSString *)text;
- (NSInteger) listenerWindow:(ListenerWindowController *)listener deleteForProposedLength:(NSInteger)characterCount;

@end
