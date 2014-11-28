/*
 * rtd_player_helpers.c
 *
 *  Ripped off: Early June (?), 2013
 *      Author: wibble
 *       Thief: SMH
 *
 */

#define _GNU_SOURCE

#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
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

  o->digitizer_data = DEF_DIGITDATA;
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
  
  while (-1 != (c = getopt(argc, argv, "A:x:f:S:C:gtER:m:rd:a:XvVh"))) {
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
	//	printf("And now we have %i: %s\n",j,infiles[j]);
      }
      break;
    case 'g':
      options->digitizer_data = true;
      //      options->rtdfile = "/tmp/rtd/rtd.data";
      break;
    case 't':
      options->tcp_data = true;
      break;
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
      printf("\t-g Digitizer data (Real data, excludes search for Dartmouth headers) [Default: %i]\n", DEF_DIGITDATA);
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

/* This one is for pulling in PCM data, the structure of which 
 * (at least for a single synchronous PCM channel coming from the DEWESoft 
 * NET interface at Wallops) is as follows:
 * 
 * Bytes 0-7:  Start packet string: { 0x00, 0x01, 0x02, 0x03, /
 *                                   0x04, 0x05, 0x06, 0x07 }
 * Bytes 8-11: (32-bit int) Packet size in bytes without stop and start string
 * Bytes 12-15: (32-bit int) Packet type (always zero for data packets)
 * Bytes 16-19: (32-bit int) Number of synchronous samples per channel
 * Bytes 20-27: (64-bit int) Number of samples acquired so far
 * Bytes 28-35: (Double floating point) Absolute/relative time
 *              (# days since 12/30/1899 | number of days since start of acq)
 * 
 * *Total header offset is 36 bytes*
 *
 * Bytes 36-39: Number of samples
 * Bytes 40-(# of samples * sample data type, which should be two-byte unsigned words): Data
 */
/* struct tcp_header { */
/*   char start_str[8]; */
/*   uint32_t pack_sz; //in bytes */
/*   uint32_t pack_numsamps; //number of synchronous samples per channel  */
/*                           //(there should only be one channel) */
/*   uint64_t pack_totalsamps; //number of samples acquired so far */
/*   double pack_time; // as given above */
/* }; */


void *parse_tcp_header(struct tcp_header *header, char *search_bytes, size_t search_length) {

  //  printf("tcp_header is %li bytes large\n", sizeof(struct tcp_header) );

  int header_length = 40;

  char skip_str[16] = { 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 
  			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };  

  //  char start_str[8] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };

  void *check_addr = memmem(search_bytes, search_length, skip_str, 16);

  if( check_addr != NULL ){
    memcpy(header, check_addr+8, header_length);
    return check_addr + 8;
  }
  /* else { */
  /*   printf("check_addr is NULL\n"); */
  /* } */

  //Return location of header, not footer that comes 8 bytes before it--that is, add 8.
  return check_addr;
}

int print_tcp_header(struct tcp_header *header){

  printf("TCP header start string =\t\t");
  for (int i = 0; i < 8; i ++){
    printf("%x",header->start_str[i]);
  }
  printf("\n");
  printf("Packet size:\t\t%"PRIu32"\n", header->pack_sz);
  printf("Packet type:\t\t%"PRIu32"\n", header->pack_type);
  printf("Packet number of samples:\t%"PRIu32"\n", header->pack_numsamps);
  printf("Total samples sent so far:\t%"PRIu64"\n", header->pack_totalsamps);
  printf("Packet time:\t\t%f\n", header->pack_time);
  printf("Sync channel num samples:\t%"PRIu32"\n", header->sync_numsamps);
  return EXIT_SUCCESS;
}
