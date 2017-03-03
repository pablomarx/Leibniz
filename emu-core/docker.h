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

typedef void (*docker_connected_f) (void *ext);
typedef void (*docker_disconnected_f) (void *ext);
typedef void (*docker_install_progress_f) (void *ext, double progress);

typedef struct docker_s {
  uint8_t *buffer;
  uint32_t bufferLen;
  uint32_t bufferIdx;

  uint8_t *response;
  uint32_t responseLen;

  docker_connected_f connected;
  docker_disconnected_f disconnected;
  docker_install_progress_f installProgress;
  void *ext;
  
  FILE *packageFile;
  uint32_t packageSize;
  int16_t packageSeqNo;
} docker_t;

docker_t *docker_new(void);
void docker_del(docker_t *c);

void docker_set_callbacks (docker_t *c, void *ext, docker_connected_f connected, docker_disconnected_f disconnected, docker_install_progress_f progress);

int8_t docker_install_package_at_path(docker_t *c, const char *path);

void docker_reset(docker_t *c);

uint8_t *docker_get_response(docker_t *c, uint32_t *outLength);

void docker_parse_payload(docker_t *c);
bool docker_can_parse_payload(docker_t *c);
void docker_receive_byte(docker_t *c, uint8_t byte);

#endif /* docker_h */
