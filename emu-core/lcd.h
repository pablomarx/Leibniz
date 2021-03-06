//
//  lcd.h
//  Leibniz
//
//  Created by Steve White on 2/10/17.
//  Copyright © 2017 Steve White. All rights reserved.
//

#ifndef lcd_h
#define lcd_h

#define BLACK_COLOR 0x00
#define SLEEP_COLOR 0xcc
#define WHITE_COLOR 0xff

extern void newton_display_open(int width, int height);
extern void newton_display_set_framebuffer(const char *display, int width, int height);

#endif /* lcd_h */
