//
//  docker.h
//  Leibniz
//
//  Created by Steve White on 3/1/17.
//  Copyright © 2017 Steve White. All rights reserved.
//

#ifndef docker_h
#define docker_h

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct docker_s {
  uint8_t *buffer;
  uint32_t bufferLen;
  uint32_t bufferIdx;
  
  FILE *packageFile;
  int16_t packageSeqNo;
  
  uint8_t *response;
  uint32_t responseLen;
} docker_t;

docker_t *docker_new(void);
void docker_del(docker_t *c);

void docker_reset(docker_t *c);

uint8_t *docker_get_response(docker_t *c, uint32_t *outLength);

void docker_parse_payload(docker_t *c);
bool docker_can_parse_payload(docker_t *c);
void docker_receive_byte(docker_t *c, uint8_t byte);

#endif /* docker_h */
