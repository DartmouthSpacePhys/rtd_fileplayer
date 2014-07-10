/*
 * rtd_player.h
 *
 *  Ripped off: Jun 3, 2014
 *      Author: wibble
 *      Thief: SMH
 */

#ifndef EPP_ACQ_H_
#define EPP_ACQ_H_

#define CF_HEADER_SIZE 200 // Colonel Frame header size
#define TIME_OFFSET 946684800 // Time to 2000.1.1 from Epoch

#include "rtd_player_struct.h"

void rtd_play(struct player_opt);
void check_acq_seq(int, bool *);
static void do_depart(int);
void *rtd_player_data_pt(void *);


#endif /* EPP_ACQ_H_ */
