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
    NSAttributedString *displayText = [self.ansiHelper attributedStringWithANSIEscapedString:output];
    [self.textView.textStorage appendAttributedString:displayText];

}

- (BOOL)textView:(NSTextView *)textView shouldChangeTextInRange:(NSRange)affectedCharRange replacementString:(NSString *)replacementString;
{
    return NO;
}

@end
