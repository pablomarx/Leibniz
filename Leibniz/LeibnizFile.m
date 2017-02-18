//
//  LeibnizFile.m
//  Leibniz
//
//  Created by Steve White on 2/10/17.
//  Copyright © 2017 Steve White. All rights reserved.
//

#import "LeibnizFile.h"

@implementation LeibnizFile

@synthesize listener = _listener;
@synthesize name = _name;
@synthesize openDescriptors = _openDescriptors;
@synthesize notifyAddress = _notifyAddress;
@synthesize inputBuffer = _inputBuffer;

- (instancetype) init {
    self = [super init];
    if (self != nil) {
        self.inputBuffer = [[NSMutableData alloc] init];
    }
    return self;
}

- (void) dealloc {
    [_listener release], _listener = nil;
    [_name release], _name = nil;
    [_openDescriptors release], _openDescriptors = nil;
    [_inputBuffer release], _inputBuffer = nil;
    [super dealloc];
}

@end
