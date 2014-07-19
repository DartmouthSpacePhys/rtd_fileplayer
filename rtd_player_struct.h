 /*
 * rtd_player_struct.h
 *
 *  Ripped off: Jun 3, 2014
 *      Author: wibble
 *      Thief: SMH
 *
 */

#ifndef EPP_STRUCT_H_
#define EPP_STRUCT_H_

#include "defaults.h"

/* Program options */
struct player_opt {
  int acqsize;
  int maxacq;
  char *infiles[MAXINFILES];
  int num_files;
  bool oldsport;
  
  bool digitizer_data;
  bool tcp_data;
  bool endian;
  int rtdsize;
  char *rtdfile;
  double dt;
  int rtdavg;
  
  bool debug;
  bool verbose;
};

/* colonel frame structure */
struct colonel_frame {
  long int size;
  char *base;
  char *tail;
  int infile;
};

struct rtd_player_ptargs {
  struct player_opt o;

  char *infile;
  time_t time;
  bool *running;
  int retval;
  char *port;

  short int *rtdframe;
  pthread_mutex_t *rlock;
};

/* define the header structure for monitor file */
struct header_info {
  int hkey;
  char site_id[12];
  int num_channels;
  char channel_flags;
  unsigned int num_samples;
  unsigned int num_read;
  unsigned int averages;
  float sample_frequency;
  float time_between_acquisitions;
  int byte_packing;
  time_t start_time;
  struct timeval start_timeval;
  float code_version;
};

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
 * Bytes 36-39: Number of samples (for synchronous channels only)
 * Bytes 40-(# of samples * sample data type, which should be two-byte unsigned words): Data
 */
struct tcp_header {
  char start_str[8];
  uint32_t pack_sz; //in bytes
  uint32_t pack_type;
  uint32_t pack_numsamps; //number of synchronous samples per channel 
                          //(there should only be one channel)
  uint64_t pack_totalsamps; //number of samples acquired so far
  double pack_time; // as given above

  uint32_t sync_numsamps;

};

/*
 * Define frame sync structure.  Pragma compiler directives
 * ensure this structure is properly-sized.
 */

#pragma pack(push,2)

struct frame_sync {
  char pattern[4];
  uint32_t t_sec;
  uint32_t t_usec;
  uint32_t size;
};

#pragma pack(pop)

#endif /* EPP_STRUCT_H_ */
