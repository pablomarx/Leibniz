//
//  LeibnizFile.h
//  Leibniz
//
//  Created by Steve White on 2/10/17.
//  Copyright © 2017 Steve White. All rights reserved.
//

#import <Foundation/Foundation.h>

@class ListenerWindowController;

@interface LeibnizFile : NSObject {
    ListenerWindowController *_listener;
    NSString *_name;
    NSArray *_openDescriptors;
    uint32_t _notifyAddress;
    NSMutableData *_inputBuffer;
}

@property (strong) ListenerWindowController *listener;
@property (strong) NSString *name;
@property (strong) NSArray *openDescriptors;
@property (assign) uint32_t notifyAddress;
@property (strong) NSMutableData *inputBuffer;

@end

