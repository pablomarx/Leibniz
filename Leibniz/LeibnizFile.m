//
//  LeibnizFile.m
//  Leibniz
//
//  Created by Steve White on 2/10/17.
//  Copyright © 2017 Steve White. All rights reserved.
//

#import "LeibnizFile.h"

@implementation LeibnizFile

- (instancetype) init {
    self = [super init];
    if (self != nil) {
        self.inputBuffer = [NSMutableData data];
    }
    return self;
}

@end
