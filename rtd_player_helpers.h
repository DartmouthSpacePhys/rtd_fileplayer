/*
 * rtd_player_helpers.h
 *
 *  Ripped off: Jun 5, 2014
 *      Author: wibble
 *      Thief: SMH
 */    

#ifndef EPP_HELPERS_H_
#define EPP_HELPERS_H_

#include <stdbool.h>
#include <stdint.h>

#include "rtd_player_struct.h"
#include "defaults.h"

int int_cmp(const void *, const void *);
void open_cap(int);
void strfifo(char *, short *, int);
void init_opt(struct player_opt *);
int parse_opt(struct player_opt *, int, char **);
void rtd_log(char *, ...);
void printe(char *, ...);

#endif /* EPP_HELPERS_H_ */
