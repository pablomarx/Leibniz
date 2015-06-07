//
//  monitor.h
//  Leibniz
//
//  Created by Steve White on 9/14/14.
//  Copyright (c) 2014 Steve White. All rights reserved.
//

#ifndef __Leibniz__monitor__
#define __Leibniz__monitor__

#include <stdio.h>
#include "newton.h"

typedef struct monitor_s {
  newton_t *newton;
  
  int32_t instructionsToExecute;
  char *lastInput;
} monitor_t;

void monitor_init (monitor_t *c);
monitor_t *monitor_new (void);
void monitor_free (monitor_t *c);
void monitor_del (monitor_t *c);
void monitor_set_newton (monitor_t *c, newton_t *newton);

void monitor_run(monitor_t *c);
void monitor_parse_input(monitor_t *c, const char *input);

#endif /* defined(__Leibniz__monitor__) */
