//
//  ListenerWindowController.m
//  Leibniz
//
//  Created by Steve White on 2/10/17.
//  Copyright © 2017 Steve White. All rights reserved.
//

#import "ListenerWindowController.h"

@implementation ListenerWindowController

@synthesize textView = _textView;
@synthesize font = _font;
@synthesize attributes = _attributes;
@synthesize delegate = _delegate;

- (instancetype) init {
    return [self initWithWindowNibName:NSStringFromClass([self class])];
}

- (void) dealloc {
    [_textView release], _textView = nil;
    [_font release], _font = nil;
    [_attributes release], _attributes = nil;
    _delegate = nil;
    [super dealloc];
}

- (void)windowDidLoad {
    [super windowDidLoad];
    self.textView.automaticQuoteSubstitutionEnabled = NO;
    self.font = [NSFont userFixedPitchFontOfSize:11];
    self.attributes = [NSMutableDictionary dictionaryWithObject:self.font
                                                         forKey:NSFontAttributeName];
}

- (NSRange) rangeOfFirstANSISequenceInString:(NSString *)text {
    NSRange ansiStartRange = [text rangeOfString:@"\e["];
    if (ansiStartRange.location == NSNotFound) {
        return NSMakeRange(NSNotFound, 0);
    }
    
    NSRange remainder;
    remainder.location = NSMaxRange(ansiStartRange);
    remainder.length = [text length] - remainder.location;
    
    NSRange ansiEndRange = [text rangeOfString:@"m"
                                       options:0
                                         range:remainder];
    
    if (ansiEndRange.location == NSNotFound) {
        return NSMakeRange(NSNotFound, 0);
    }
    
    NSRange ansiRange;
    ansiRange.location = ansiStartRange.location;
    ansiRange.length = NSMaxRange(ansiEndRange) - ansiRange.location;
    return ansiRange;
}

- (void) parseAndAppendText:(NSString *)text {
    if ([text length] == 0) {
        return;
    }
    
    NSRange ansiRange = [self rangeOfFirstANSISequenceInString:text];
    NSInteger chunkLength;
    if (ansiRange.location == NSNotFound) {
        chunkLength = [text length];
    }
    else {
        chunkLength = ansiRange.location;
    }
    
    // Insert a chunk of text (stopping at the ANSI esc seq, if exists)
    if (chunkLength > 0) {
        NSString *chunk = [text substringToIndex:chunkLength];
        NSAttributedString *displayText = [[NSAttributedString alloc] initWithString:chunk attributes:self.attributes];
        [self.textView.textStorage appendAttributedString:displayText];
        [displayText release];
    }
    
    if (ansiRange.location == NSNotFound) {
        return;
    }
    
    // Parse the ansi code... Newton only uses bold/plain.
    // Update our attributes to match
    NSString *ansiCode = [text substringWithRange:ansiRange];
    NSString *remainder = [text substringFromIndex:NSMaxRange(ansiRange)];
    if ([ansiCode isEqualToString:@"\e[0m"] == YES) {
        [self.attributes setObject:self.font forKey:NSFontAttributeName];
    }
    else if ([ansiCode isEqualToString:@"\e[1m"] == YES) {
        NSFont *boldFont = [[NSFontManager sharedFontManager] convertFont:self.font toHaveTrait:NSFontBoldTrait];
        [self.attributes setObject:boldFont forKey:NSFontAttributeName];
    }
    
    // Parse the remainder of the incoming text
    [self parseAndAppendText:remainder];
}

- (void) appendOutput:(NSString *)output {
    NSTextView *textView = self.textView;
    NSClipView *clipView = [textView.enclosingScrollView contentView];
    NSRect documentRect = clipView.documentRect;
    NSRect documentVisibleRect = clipView.documentVisibleRect;
    BOOL atBottom = NSMaxY(documentRect) == NSMaxY(documentVisibleRect);
    
    [self parseAndAppendText:output];
    
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
    if (self.delegate == nil) {
        return NO;
    }
    
    NSRange eofRange = NSMakeRange([textView.textStorage length], 0);
    if (NSEqualRanges(affectedCharRange, eofRange) == YES) {
        [self.delegate listenerWindow:self insertedText:replacementString];
        return YES;
    }
    
    if (NSMaxRange(affectedCharRange) == NSMaxRange(eofRange) && [replacementString length] == 0) {
        NSInteger toDelete = [self.delegate listenerWindow:self deleteForProposedLength:affectedCharRange.length];
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
