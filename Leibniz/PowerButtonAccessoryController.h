//
//  PowerButtonAccessoryController.h
//  Leibniz
//
//  Created by Steve White on 2/16/17.
//  Copyright © 2017 Steve White. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "PowerButton.h"

@interface PowerButtonAccessoryController : NSTitlebarAccessoryViewController

@property (readonly) PowerButton *powerButton;

@end
