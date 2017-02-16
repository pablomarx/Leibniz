//
//  ListenerWindowController.m
//  Leibniz
//
//  Created by Steve White on 2/10/17.
//  Copyright © 2017 Steve White. All rights reserved.
//

#import "ListenerWindowController.h"
#import "AMR_ANSIEscapeHelper.h"

@interface ListenerWindowController ()<NSTextViewDelegate>
@property (strong) IBOutlet NSTextView *textView;
@property (strong) AMR_ANSIEscapeHelper *ansiHelper;
@end

@implementation ListenerWindowController

- (instancetype) init {
    return [self initWithWindowNibName:NSStringFromClass([self class])];
}

- (void)windowDidLoad {
    [super windowDidLoad];
    self.ansiHelper = [[AMR_ANSIEscapeHelper alloc] init];
    self.ansiHelper.font = [NSFont userFixedPitchFontOfSize:11];
}

- (void) appendOutput:(NSString *)output {
    NSTextView *textView = self.textView;
    NSClipView *clipView = [textView.enclosingScrollView contentView];
    NSRect documentRect = clipView.documentRect;
    NSRect documentVisibleRect = clipView.documentVisibleRect;
    BOOL atBottom = NSMaxY(documentRect) == NSMaxY(documentVisibleRect);
    
    NSAttributedString *displayText = [self.ansiHelper attributedStringWithANSIEscapedString:output];
    [textView.textStorage appendAttributedString:displayText];
    
    if (atBottom == YES) {
        documentRect = clipView.documentRect;
        NSPoint scrollPoint;
        scrollPoint.x = 0;
        scrollPoint.y = NSHeight(documentRect) - NSHeight(documentVisibleRect);
        [clipView scrollToPoint: scrollPoint];
    }
}

- (BOOL)textView:(NSTextView *)textView shouldChangeTextInRange:(NSRange)affectedCharRange replacementString:(NSString *)replacementString;
{
    NSRange eofRange = NSMakeRange([textView.textStorage length], 0);
    if (NSEqualRanges(affectedCharRange, eofRange) == YES) {
        if (self.delegate != nil) {
            [self.delegate listenerWindow:self insertedText:replacementString];
        }
        return YES;
    }
    
    if (NSMaxRange(affectedCharRange) == NSMaxRange(eofRange) && [replacementString length] == 0) {
        NSInteger toDelete = 0;
        if (self.delegate != nil) {
            [self.delegate listenerWindow:self deleteForProposedLength:affectedCharRange.length];
        }
        else {
            toDelete = affectedCharRange.length;
        }
        
        if (toDelete == 0) {
            NSBeep();
            return NO;
        }
        
        if (toDelete == affectedCharRange.length) {
            return YES;
        }
        
        NSRange newRange;
        newRange.length = toDelete;
        newRange.location = [textView.textStorage length] - newRange.length;
        [textView replaceCharactersInRange:newRange withString:@""];
        return NO;
    }
    
    [textView setSelectedRange:eofRange];
    [textView insertText:replacementString replacementRange:eofRange];
    return NO;
}

@end
