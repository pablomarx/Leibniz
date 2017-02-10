#include "newton.h"
#include "monitor.h"
#include "runt.h"
#include "HammerConfigBits.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX(a, b)    (((a) > (b)) ? (a) : (b))

typedef struct newton_file_s newton_file_t;
struct newton_file_s {
  char *name;
  int mode;
  int fp;
  int istty;
  uint32_t input_notify;
  newton_file_t *next;
};

newton_file_t *open_files = NULL;

newton_file_t *newton_tap_file_for_fp(uint32_t fp)
{
  newton_file_t *file = open_files;
  while (file != NULL) {
    if (file->fp == fp) {
      return file;
    }
    file = file->next;
  }
  return NULL;
}

void newton_tap_file_delete(newton_file_t *file)
{
  newton_file_t *aFile = open_files;
  newton_file_t *last = NULL;
  while (aFile != NULL) {
    if (aFile == file) {
      if (last == NULL) {
        open_files = file->next;
      }
      else {
        last->next = file->next;
      }
      break;
    }
    last = aFile;
    aFile = aFile->next;
  }
  
  free(file->name);
  free(file);
}

int32_t leibniz_sys_open(void *ext, const char *name, int mode) {
  uint32_t fp = 0;
  newton_file_t *file = open_files;
  while(file != NULL) {
    fp = MAX(fp, file->fp);
    file = file->next;
  }
  fp = fp + 1;
  
  file = calloc(1, sizeof(newton_file_t));
  file->fp = fp;
  file->istty = (name[0] == '%');
  file->mode = mode;
  
  file->name = calloc(strlen(name)+1, sizeof(char));
  strcpy(file->name, name);
  
  file->next = open_files;
  open_files = file;
  return fp;
}

int32_t leibniz_sys_close(void *ext, uint32_t fildes) {
  newton_file_t *file = newton_tap_file_for_fp(fildes);
  if (file != NULL) {
    newton_tap_file_delete(file);
  }
  return 1;
}

int32_t leibniz_sys_istty(void *ext, uint32_t fildes) {
  int32_t result = 0;
  newton_file_t *file = newton_tap_file_for_fp(fildes);
  if (file != NULL) {
    result = file->istty;
  }
  return result;
}

int32_t leibniz_sys_read(void *ext, uint32_t fildes, void *buf, uint32_t nbyte) {
  return 0;
}

int32_t leibniz_sys_write(void *ext, uint32_t fildes, const void *buf, uint32_t nbyte) {
  printf("msg: %s\n", buf);
  return 0;
}

int32_t leibniz_sys_set_input_notify(void *ext, uint32_t fildes, uint32_t addr) {
  return 1;
}

#pragma mark -
void FlushDisplay(const char *display, int width, int height) {
}

#pragma mark -

void print_usage(const char *name) {
  fprintf(stderr, "usage: %s [-b bootmode] [-d debugmode] [-m mapfile] romfile\n", name);
  exit(1);
}

int main(int argc, char **argv) {
  extern char *optarg;
  int c, err = 0;
  
  char *bootmode = NULL;
  char *mapname = NULL;
  int debugmode = 0;
  
  while ((c = getopt(argc, argv, "b:m:d:")) != -1) {
    switch (c) {
      case 'd':
        debugmode = atoi(optarg);
        break;
      case 'b':
        bootmode = optarg;
        break;
      case 'm':
        mapname = optarg;
        break;
      case '?':
        err = 1;
        break;
    }
  }
  
  if (err || optind == argc) {
    print_usage(argv[0]);
  }
  
  char *romFile = argv[optind];
  
  newton_t *newton = newton_new();
  if (newton_load_rom(newton, romFile) == -1) {
    return -1;
  }
  
  if (bootmode != NULL) {
    newton_set_bootmode(newton, atoi(bootmode));
  }
  
  if (mapname != NULL) {
    newton_load_mapfile(newton, mapname);
  }
  
  newton_set_log_flags(newton, NewtonLogAll, 1);
  
  runt_t *runt = newton_get_runt(newton);
  runt_set_log_flags(runt, RuntLogAll, 1);
  
  newton_set_break_on_unknown_memory(newton, true);
  if (debugmode) {
    newton_set_debugger_bits(newton, 1);
    newton_set_newt_config(newton, kConfigBit3 | kDontPauseCPU | kStopOnThrows | kEnableStdout | kDefaultStdioOn | kEnableListener);
  }
  
  newton_set_tapfilecntl_funtcions(newton,
                   NULL,
                   leibniz_sys_open,
                   leibniz_sys_close,
                   leibniz_sys_istty,
                   leibniz_sys_read,
                   leibniz_sys_write,
                   leibniz_sys_set_input_notify);
  
  monitor_t *monitor = monitor_new();
  monitor_set_newton(monitor, newton);
  monitor_run(monitor);
  
  monitor_del(monitor);
  newton_del(newton);
  
  return 0;
}
