/*
 * simple_fifo.h
 *
 *  Created on: Mar 1, 2011
 *      Author: wibble
 */

#ifndef SIMPLE_FIFO_H_
#define SIMPLE_FIFO_H_

#define EFIFO_MEM 512

struct simple_fifo {
	char *base;
	long int size;

	char *head;
	char *tail;
};

int fifo_init(struct simple_fifo *, long int);
int fifo_write(struct simple_fifo *, char *, long int);
long int fifo_read(char *, struct simple_fifo *, long int);
int fifo_kill(struct simple_fifo *, long int);
void fifo_destroy(struct simple_fifo *);
long int fifo_avail(struct simple_fifo *);

long int fifo_search(struct simple_fifo *, char *, size_t);


#endif /* SIMPLE_FIFO_H_ */
