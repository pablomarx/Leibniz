//
//  FileStream.h
//  Leibniz
//
//  Created by Steve White on 2/16/17.
//  Copyright © 2017 Steve White. All rights reserved.
//

#import <Foundation/Foundation.h>

@protocol FileStreamDelegate;

@interface FileStream : NSObject {
    FILE *_file;
    id _delegate;
}

@property (assign) id<FileStreamDelegate> delegate;
@property (assign, readonly) FILE *file;

@end

@protocol FileStreamDelegate <NSObject>

- (void) fileStream:(FileStream *)fileStream wroteData:(NSData *)data;
- (void) fileStreamClosed:(FileStream *)fileStream;

@end
