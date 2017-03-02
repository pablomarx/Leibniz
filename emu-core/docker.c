//
//  docker.c
//  Leibniz
//
//  Created by Steve White on 3/1/17.
//  Copyright © 2017 Steve White. All rights reserved.
//

#include "docker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crc16.h"

#define SYN 0x16
#define DLE 0x10
#define STX 0x02
#define ETX 0x03

#define LR  1
#define LD  2
#define LT  4
#define LA  5
#define LN  6
#define LNA 7

#define countof(__a__) (sizeof(__a__) / sizeof(__a__[0]))

void docker_init(docker_t *c) {
  c->bufferLen = 256;
  c->buffer = calloc(c->bufferLen, sizeof(uint8_t));
}

docker_t *docker_new(void) {
  docker_t *c = calloc(1, sizeof(docker_t));
  docker_init(c);
  return c;
}

void docker_free(docker_t *c) {
  if (c->buffer != NULL) {
    free(c->buffer);
  }
  if (c->response != NULL) {
    free(c->response);
  }
}

void docker_del(docker_t *c) {
  docker_free(c);
  free(c);
}

void docker_reset(docker_t *c) {
  memset(c->buffer, 0x00, c->bufferLen);
  c->bufferIdx = 0;
  
  if (c->response != NULL) {
    free(c->response);
    c->response = NULL;
  }
  c->responseLen = 0;
}

uint8_t *docker_get_response(docker_t *c, uint32_t *outLength) {
  if (outLength != NULL) {
    *outLength = c->responseLen;
  }
  return c->response;
}

void docker_make_framed_response(docker_t *c, uint8_t *data, uint32_t length) {
  uint32_t idx = 0;
  uint16_t xsum = 0;
  uint32_t rsplen = 0;
  
  rsplen = length + 7;
  for (uint32_t i=0; i<length; i++) {
    if (data[i] == DLE) {
      rsplen++; // Need to escape the DLE
    }
  }
  
  if (c->response == NULL) {
    c->responseLen = rsplen;
    c->response = calloc(c->responseLen, sizeof(uint8_t));
  }
  else {
    uint32_t newlength = c->responseLen + length;
    c->response = realloc(c->response, newlength);
    idx = c->responseLen;
    c->responseLen = newlength;
  }
  
  c->response[idx++] = SYN;
  c->response[idx++] = DLE;
  c->response[idx++] = STX;
  for (uint32_t i=0; i<length; i++) {
    c->response[idx++] = data[i];
    xsum = crc16_update(xsum, data[i]);

    if (data[i] == DLE) {
      c->response[idx++] = DLE;
      xsum = crc16_update(xsum, DLE);
    }
  }

  xsum = crc16_update(xsum, ETX);
  
  c->response[idx++] = DLE;
  c->response[idx++] = ETX;
  c->response[idx++] = (xsum & 0xff);
  c->response[idx++] = (xsum >> 8);
}

void docker_newt_dock_response(docker_t *c, const char *command, uint8_t seqNo, void *data, uint32_t length) {
  uint32_t frameSize = 3 /*lt header*/ + 12 /*newtdock<cmd>*/ + 4/*length*/;
  if (data != NULL) {
    frameSize += length;
  }
  uint8_t *frame = calloc(frameSize, sizeof(uint8_t));
  uint8_t idx = 0;
  
  frame[idx++] = 2; // length
  frame[idx++] = LT;
  frame[idx++] = seqNo;
  
  memcpy(frame + idx, "newt", 4);
  idx += 4;
  memcpy(frame + idx, "dock", 4);
  idx += 4;
  memcpy(frame + idx, command, 4);
  idx += 4;
  
  frame[idx++] = ((length >> 24) & 0xff);
  frame[idx++] = ((length >> 16) & 0xff);
  frame[idx++] = ((length >>  8) & 0xff);
  frame[idx++] = ((length      ) & 0xff);
  
  if (data != NULL) {
    memcpy(frame + idx, data, length);
  }
  
  docker_make_framed_response(c, frame, frameSize);

  free(frame);
}

void docker_parse_newt_dock_payload(docker_t *c) {
  const char *command = (const char *)c->buffer + 14;
  uint8_t seqNo = c->buffer[5];
  
  if (strncmp(command, "rtdk", 4) == 0) {
    // Session type should be 4 to load a package
    uint8_t sessionType[] = { 0, 0, 0, 4 };
    docker_newt_dock_response(c, "dock", seqNo, sessionType, sizeof(sessionType));
  }
  else if (strncmp(command, "name", 4) == 0) {
    uint8_t seconds[] = { 0, 0, 0, 30 };
    docker_newt_dock_response(c, "stim", seqNo, seconds, sizeof(seconds));
  }
  else if (strncmp(command, "dres", 4) == 0) {
    uint32_t errCode = *(uint32_t *)&c->buffer[22];
    if (errCode != 0) {
      // Newton formats suggests the Newton will disconnect
      // after sending an error code
      printf("errCode=0x%08x\n", errCode);
      return;
    }

    // Ready to install packages.. but we'll disconncet for now
    docker_newt_dock_response(c, "disc", seqNo, NULL, 0);
  }
}

void docker_make_la_response(docker_t *c, uint8_t seqNo) {
  uint8_t frame[] = { 0x03, LA, seqNo, 0x01 };
  docker_make_framed_response(c, frame, countof(frame));
}

void docker_parse_payload(docker_t *c) {
  switch(c->buffer[4]) {
    case LR: {
      uint8_t lr[]= {23,1,2,1,6,1,0,0,0,0,255,2,1,2,3,1,1,4,2,64,0,8,1,3};
      docker_make_framed_response(c, lr, countof(lr));
      break;
    }
    case LT: {
      if (strncmp((const char *)c->buffer + 6, "newt", 4) == 0) {
        if (strncmp((const char *)c->buffer + 10, "dock", 4) == 0) {
          docker_parse_newt_dock_payload(c);
        }
      }
      break;
    }
    case LA:
      docker_make_la_response(c, c->buffer[5]);
      break;
    case LD:
      // Disconnect?
      break;
    default:
      printf("Unhandled frame type: 0x%02x\n", c->buffer[4]);
      break;
  }
  
  c->bufferIdx = 0;
}

bool docker_can_parse_payload(docker_t *c) {
  if (c->bufferIdx < 4) {
    return false;
  }
  
  uint32_t payloadLength = 3; // SYN, DLE, STX
  payloadLength += 2; // DLE, ETX
  payloadLength += 2; // crc16
  payloadLength += c->buffer[3]; // header length
  payloadLength += 1; // header length bit
  
  if (c->bufferIdx < payloadLength) {
    return false;
  }
  
  // Check for DLE, ETX, with enough room for CRC16
  for (uint32_t i=5; i<c->bufferIdx-3; i++) {
    if (c->buffer[i-1] != DLE && c->buffer[i] == DLE && c->buffer[i + 1] == ETX) {
      // XXX: Check the checksum!?
      return true;
    }
  }
  
  return false;
}

void docker_receive_byte(docker_t *c, uint8_t byte) {
  // Ensure we're dealing with a bisync frame
  if (c->bufferIdx == 0 && byte != SYN) {
    return;
  }
  if (c->bufferIdx == 1 && byte != DLE) {
    c->bufferIdx = 0;
    return;
  }
  if (c->bufferIdx == 2 && byte != STX) {
    c->bufferIdx = 0;
    return;
  }

  if (c->bufferIdx >= c->bufferLen) {
    c->bufferLen += 256;
    c->buffer = realloc(c->buffer, c->bufferLen);
  }
  
  c->buffer[c->bufferIdx] = byte;
  c->bufferIdx++;
}
