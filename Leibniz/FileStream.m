//
//  FileStream.m
//  Leibniz
//
//  Created by Steve White on 2/16/17.
//  Copyright © 2017 Steve White. All rights reserved.
//

#import "FileStream.h"

@implementation FileStream

int fileStreamRead(void *handler, char *buf, int size) {
    return 0;
}

int fileStreamWrite(void *handler, const char *buf, int size) {
    FileStream *self = (__bridge FileStream *)handler;

    NSData *data = [[NSData alloc] initWithBytes:buf
                                          length:size];
    [self fileWroteData:data];
    return size;
}

fpos_t fileStreamSeek(void *handler, fpos_t offset, int whence) {
    return (fpos_t)0;
}

int fileStreamClose(void *handler) {
    FileStream *self = (__bridge FileStream *)handler;
    [self fileClosed];
    return 0;
}

- (FILE *) file {
    if (_file == NULL) {
        _file = funopen((__bridge void *)self,
                        fileStreamRead,
                        fileStreamWrite,
                        fileStreamSeek,
                        fileStreamClose);
        setvbuf(_file, (char *)NULL, _IOLBF, 0);
    }
    return _file;
}

- (void) fileWroteData:(NSData *)data {
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.delegate fileStream:self wroteData:data];
    });
}

- (void) fileClosed {
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.delegate fileStreamClosed:self];
    });
}

@end
