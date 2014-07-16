/*
 * rtd_player_helpers.c
 *
 *  Ripped off: Early June (?), 2013
 *      Author: wibble
 *       Thief: SMH
 *
 */

#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>
#include <time.h>

#include "rtd_player_helpers.h"
#include "defaults.h"

void printe(char *format, ...) {
	va_list args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	fflush(stderr);
	va_end(args);
}

void init_opt(struct player_opt *o) {
  memset(o, 0, sizeof(struct player_opt));
  o->acqsize = DEF_ACQSIZE;
  //  memset(o->infiles, 0, sizeof(char) * MAXINFILES*50); o->infiles[0] = "";
  o->infiles[0] = "";
  o->num_files = 1;
  o->oldsport = false;
  
  o->tcp_data = DEF_TCPDATA;
  o->endian = DEF_ENDIANNESS;
  o->rtdsize = DEF_RTDSIZE;
  o->rtdfile = DEF_RTDFILE;
  o->dt = DEF_RTD_DT;
  o->rtdavg = DEF_RTDAVG;

  o->maxacq = 0;
  
  o->debug = false;
  o->verbose = false;
}

int parse_opt(struct player_opt *options, int argc, char **argv) {
  char *infiles[MAXINFILES];
  char *pn;
  int c, i = 0;
  
  while (-1 != (c = getopt(argc, argv, "A:x:f:S:C:tER:m:rd:a:XvVh"))) {
    switch (c) {
    case 'A':
      options->acqsize = strtoul(optarg, NULL, 0);
      break;
    case 'x':
      options->maxacq = strtoul(optarg, NULL, 0);
      break;
    case 'f':
      pn = strtok(optarg, ",");
      infiles[i] = pn;
      while ((pn = strtok(NULL , ",")) != NULL) {
	i++;
	infiles[i] = pn;
      }
      int j;
      for (j = 0; j <= i; j++) {
	options->infiles[j] = infiles[j];
	options->num_files = i+1;
	printf("Now we gots %i: %s\n",j,infiles[j]);
      }
      break;
    case 't':
      options->tcp_data = true;
    case 'E':
      options->endian = true;
      break;
    case 'R':
      options->rtdsize = strtoul(optarg, NULL, 0);
      break;
    case 'm':
      options->rtdfile = optarg;
      break;
    case 'd':
      options->dt = strtod(optarg, NULL);
      break;
    case 'a':
      options->rtdavg = strtoul(optarg, NULL, 0);
      break;
    case 'v':
      options->verbose = true;
      break;
    case 'V':
      options->debug = true;
      break;
    case 'h':
    default:
      printf("rtd_player: \"Play back\" acquired data files for prtd or cprtd.\n\n Options:\n");
      printf("\t-A <#>\tAcquisition request size [Default: %i].\n", DEF_ACQSIZE);
      printf("\t-x <#>\tMax <acqsize>-byte acquisitions [Inf].\n");
      printf("\t-f <#>\tFile(s) to 'acquire' from (see below) [1].\n");
      printf("\t\tCan either give a single file, or a comma-separated list.\n");
      printf("\t\ti.e., \"this.data,that.data\", \"/path/to/my.data\"\n");
      printf("\n");
      printf("\t-t Wallops TCP/IP data (removes TCP packet headers from RTD data) [Default: %i]\n", DEF_TCPDATA);
      printf("\t-E Switch endianness of \"Dartmouth\" search for RTD output [Default: %i]\n",DEF_ENDIANNESS);
      printf("\t\t(FSCC-LVDS data needs this disabled, but TCP data needs it enabled)\n");
      printf("\t-R <#>\tReal-time display output size (in words) [%i].\n", DEF_RTDSIZE);
      printf("\t-m <s>\tReal-time display file [%s].\n", DEF_RTDFILE);
      printf("\t-d <#>\tReal-time display output period [%i].\n", DEF_RTD_DT);
      printf("\t-a <#>\tNumber of RTD blocks to average [%i].\n", DEF_RTDAVG);
      printf("\n");
      printf("\t-v Be verbose.\n");
      printf("\t-V Print debug-level messages.\n");
      printf("\t-h Display this message.\n");
      exit(1);
    }
    
  }
  
  return argc;
}

/* qsort int comparison function */
int int_cmp(const void *a, const void *b)
{
  const int *ia = (const int *)a; // casting pointer types
  const int *ib = (const int *)b;
  return *ia  - *ib;
  /* integer comparison: returns negative if b > a
     and positive if a > b */
}


