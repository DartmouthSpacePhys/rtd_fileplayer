/*
 * defaults.h
 *
 *  Created on: Mar 9, 2011
 *      Author: wibble
 *
 *  Jun 2, 2014, SMH: I've completely ripped of MPD's qusb_acq to do this
 */

#ifndef DEFAULTS_H_
#define DEFAULTS_H_

/* Size of data in bytes between frame sync pulses */
#define DEF_ACQSIZE 131200  // Data acquisition request size for TRANSPARENT mode

#define DEF_PREFIX "rtd_player"
#define DEF_OUTDIR "/home/spencerh/data"

#define DEF_CFSIZE 65536 // Colonel Frame size, words
// #define DEF_CFHEAD "Dartmouth College "
/* Accommodate the weirdness right now */
#define DEF_CFHEAD "Dartmouth College"

#define DEF_RTDSIZE 65536 // RTD Output size, words
#define DEF_RTDFILE "/tmp/rtd/rtd.data"
#define DEF_RTD_DT 0 // No RTD by default
#define DEF_RTDAVG 12

#define DEF_TIMEOUT 1000 //timeout of 1 second MAX!

#define MAXPORTS 2

#endif /* DEFAULTS_H_ */
