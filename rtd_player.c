/* rtd_player
 *  ->A shameless and complete bastardization of fscc_acq, 
 *      which was already an insult to its parent, qusb_acq
 *    
 *    
 *
 * se creó y encargó : Jul 10 (Sarah's birthday), 2014
 *
 *
 */

#define _GNU_SOURCE

#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <termios.h>

#include "simple_fifo.h"
#include "rtd_player_helpers.h"
#include "rtd_player.h"

#define EEPP_FILE 8
#define EEPP_THREAD 9
#define MIN_BYTES_READ 100

static bool running = true;

int main(int argc, char **argv)
{
  struct player_opt o;

  init_opt(&o);
  parse_opt(&o, argc, argv);

  if ( o.infiles[0] && o.infiles[0][0] == '\0' ) {
      fprintf(stderr,"No input file provided. Use option -f on command line, or -h to see options.\n");
      exit(EXIT_FAILURE);
    }


  signal(SIGINT, do_depart);

  rtd_play(o);

}

void rtd_play(struct player_opt o) {
  
  time_t pg_time;
  int tret, ret, rtdsize = 0;
  struct rtd_player_ptargs *thread_args;
  pthread_t *data_threads;
  //	pthread_t rtd_thread;
    
  short int **rtdframe, *rtdout = NULL;
  struct header_info header;
  int rfd, active_threads = 0;
  char *rmap = NULL;
  struct stat sb;
  pthread_mutex_t *rtdlocks;
  double telapsed;
  struct timeval now, then;

  pg_time = time(NULL);


  data_threads = malloc(o.num_files * sizeof(pthread_t));
  printf("o.num_files is currently %i\n",o.num_files);
  rtdlocks = malloc(o.num_files * sizeof(pthread_mutex_t));
  thread_args = malloc(o.num_files * sizeof(struct rtd_player_ptargs));
  rtdframe = malloc(o.num_files * sizeof(short int *));
  
  if (o.dt > 0) {
    printf("RTD");
    
    rtdsize = o.rtdsize * sizeof(short int);
    if (rtdsize > 2*o.acqsize) printf("RTD Total Size too big!\n");
    else printf(" (%i", o.rtdsize);
    if (1024*o.rtdavg > rtdsize) printf("Too many averages for given RTD size.\n");
    else printf("/%iavg)", o.rtdavg);
    printf("...");
    
    rtdout = malloc(o.num_files * rtdsize);
    
    if ((rtdframe == NULL) || (rtdout == NULL)) {
      printe("RTD mallocs failed.\n");
    }
    
    for (int i = 0; i < o.num_files; i++) {
      rtdframe[i] = malloc(rtdsize);
    }
    
    /*
     * Create/truncate the real-time display file, fill it
     * with zeros to the desired size, then mmap it.
     */
    rfd = open(o.rtdfile,
	       O_RDWR|O_CREAT|O_TRUNC,
	       S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if (rfd == -1) {
      printe("Failed to open rtd file.\n"); return;
    }
    if ((fstat(rfd, &sb) == -1) || (!S_ISREG(sb.st_mode))) {
      printe("Improper rtd file.\n"); return;
    }
    int mapsize = o.num_files*rtdsize + 100;
    char *zeroes = malloc(mapsize);
    memset(zeroes, 0, mapsize);
    ret = write(rfd, zeroes, mapsize);
    free(zeroes);
    //		printf("mmap, %i",rfd);fflush(stdout);
    rmap = mmap(0, mapsize, PROT_WRITE|PROT_READ, MAP_SHARED, rfd, 0);
    if (rmap == MAP_FAILED) {
      printe("mmap() of rtd file failed.\n"); return;
    }
    ret = close(rfd);
    madvise(rmap, mapsize, MADV_WILLNEED);
    
    /*
     * Set up basic RTD header
     */
    header.num_read = o.rtdsize*o.num_files;
    sprintf(header.site_id,"%s","RxDSP Woot?");
    header.hkey = 0xF00FABBA;
    header.num_channels=o.num_files;
    header.channel_flags=0x0F;
    header.num_samples=o.rtdsize;
//!!! DOES THIS NEED TO BE CHANGED TO 960000?
    header.sample_frequency=960000;
    header.time_between_acquisitions=o.dt;
    header.byte_packing=0;
    header.code_version=0.1;
  }
    
  /*
   * Set up and create the write thread for each file.
   */
  for (int i = 0; i < o.num_files; i++) {
    thread_args[i].infile = o.infiles[i];
    ret = pthread_mutex_init(&rtdlocks[i], NULL);
    if (ret) {
      printe("RTD mutex init failed: %i.\n", ret); exit(EEPP_THREAD);
    }
    

    printf("file %s...", o.infiles[i]); fflush(stdout);
    thread_args[i].o = o;
    thread_args[i].retval = 0;
    thread_args[i].running = &running;
    thread_args[i].rtdframe = rtdframe[i];
    thread_args[i].rlock = &rtdlocks[i];
    thread_args[i].time = pg_time;
    
    ret = pthread_create(&data_threads[i], NULL, rtd_player_data_pt, (void *) &thread_args[i]);
    
    if (ret) {
      printe("Thread %i failed!: %i.\n", i, ret); exit(EEPP_THREAD);
    } else active_threads++;
  }
  
  if (o.debug) printf("Size of header: %li, rtdsize: %i, o.num_files: %i.\n", sizeof(header), rtdsize, o.num_files);


  /*
   * Now we sit back and update RTD data until all files quit reading.
   */
  gettimeofday(&then, NULL);
  while ((active_threads > 0) || running) {
    if (o.dt > 0) {
      gettimeofday(&now, NULL); // Check time
      telapsed = now.tv_sec-then.tv_sec + 1E-6*(now.tv_usec-then.tv_usec);

      if (telapsed > o.dt) {
	/*
	 * Lock every rtd mutex, then just copy in the lock, for speed.
	 */
	for (int i = 0; i < o.num_files; i++) {
	  pthread_mutex_lock(&rtdlocks[i]);
	  memmove(&rtdout[i*o.rtdsize], rtdframe[i], rtdsize);
	  pthread_mutex_unlock(&rtdlocks[i]);
	}

	header.start_time = time(NULL);
	header.start_timeval = now;
	header.averages = o.rtdavg;

	memmove(rmap, &header, sizeof(struct header_info));
	memmove(rmap+102, rtdout, rtdsize*o.num_files);

	then = now;
      }

    }

    /*
     * Check for any threads that are joinable (i.e. that have quit).
     */
    for (int i = 0; i < o.num_files; i++) {
      ret = pthread_tryjoin_np(data_threads[i], (void *) &tret);

      tret = thread_args[i].retval;
      if (ret == 0) {
	active_threads--;
	if (tret) printf("file %s error: %i...", o.infiles[i], tret);
	if(active_threads == 0) {
	  running = false;
	}
      } // if (ret == 0) (thread died)
    } // for (; i < o.num_files ;)
    usleep(5000); // Zzzz...
  }

  /*
   * Free.  FREE!!!
   */
  if (o.dt > 0) {
    for (int i = 0; i < o.num_files; i++) {
      if (rtdframe[i] != NULL) free(rtdframe[i]);
    }
    free(rtdframe); free(rtdlocks);
    free(thread_args); free(data_threads);
    if (rtdout != NULL) free(rtdout);
  }

  printf("All done!\n");

  pthread_exit(NULL);
  
  
}

void *rtd_player_data_pt(void *threadarg) {

  struct rtd_player_ptargs arg;
  arg = *(struct rtd_player_ptargs *) threadarg;

  struct simple_fifo *fifo;
  long int fifo_loc;
  char *fifo_outbytes;
  char fifo_srch[18];
  if (arg.o.endian) {
    strcpy(fifo_srch, "aDtromtu hoCllge");
  }
  else {
    strcpy(fifo_srch,"Dartmouth College");
  }

  int e = 0;
  int receiving;
  
  //TCP stuff
  int tcp_hc = 0; //tcp header count
  int tcp_tc = 0; //tcp tail count
  struct tcp_header *tcp_hdr;
  int tcp_hdrsz = 40;
  int tcp_tailsz = 8;
  void *oldheader_loc;
  void *header_loc;
  void *tail_loc;
  //  bool junk_tcpheader = true;

  char *dataz;
  int count;
  long long unsigned int i = 0;
  long long unsigned int frames, wcount;
  int imod = 10;

  //RTD stuff
  int rtdbytes;
  double telapsed;
  struct tm ct;
  struct timeval start, now, then;

  FILE *fp;
  struct stat fstat;

  if (arg.o.debug) { printf("File %s thread init.\n", arg.infile); fflush(stdout); }

  fp = fopen(arg.infile, "r");
  if( fp  == NULL ){
    fprintf(stderr,"Couldn't open %s!!!\n",arg.infile);
    *arg.running = false;
    arg.retval = EXIT_FAILURE; pthread_exit((void *) &arg.retval);
  } 
  stat(arg.infile,&fstat);

  dataz = malloc(arg.o.acqsize);

  rtdbytes = arg.o.rtdsize*sizeof(short int);

  fifo = malloc( sizeof(*fifo) );
  fifo_init(fifo, 4*rtdbytes);  
  fifo_outbytes = malloc(rtdbytes);
  long int fifo_count;

  tcp_hdr = malloc( sizeof(struct tcp_header) );
  char tcp_str[16] = { 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00,
		       0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };

  frames = count = wcount = 0;
  
  gmtime_r(&arg.time, &ct);
  gettimeofday(&start, NULL);
  then = start;
   
  //!!!Make sleeptime some sort of command-line arg
  printf("Sleeping %i us.\n", 10000);
  
  /*
   * Main data loop
   */
  receiving = 1;

  while ( *arg.running ) {
    if (arg.o.debug) { printf("Serial file %s debug.\n", arg.infile); fflush(stdout); }

    //    usleep(sleeptime);
      
    if (arg.o.debug) { printf("Serial file %s read data.\n", arg.infile); fflush(stdout); }
    
    memset(dataz, 0, arg.o.acqsize);
    if (receiving) {
      count = fread(dataz, 1, arg.o.acqsize, fp);
      usleep(100000);
    }
    if( count == -1) {
      fprintf(stderr,"Couldn't read file %s!!\n",arg.infile);
      *arg.running = false;
      arg.retval = EXIT_FAILURE; pthread_exit((void *) &arg.retval);
    } else if( count == 0){
      printf("End of file %s reached. Done.\n", arg.infile );
      receiving = 0;
      *arg.running = false;
      arg.retval = EXIT_SUCCESS; pthread_exit((void *) &arg.retval);
    }
    if (count > MIN_BYTES_READ) {
      wcount += count;
      if ((i++ % imod) == 0)  {
	  printf("Read %i bytes of data\n", count);
	}
      }
    
    if(arg.o.debug){ printf("num_reads = %llu\n", i); }
    
    gettimeofday(&then, NULL);

    if( arg.o.tcp_data ){

      tcp_hc = 0;
      long int tail_diff = 0;
      long int header_diff = 0;
      long int keep = count;

      tail_loc = memmem(dataz, count, tcp_str, 8);
      oldheader_loc = memmem(dataz, keep, &tcp_str[8], 8);

      /*In general, reads will finish before we find the tail, so we want to kill them right here*/
      if( (tail_loc != NULL ) && ( oldheader_loc != NULL ) && (tail_loc < oldheader_loc) ){

	tcp_tc++;

	if(arg.o.debug){ printf("**\ntail %i loc:%p\n**\n",tcp_hc, tail_loc); }

	//	if(junk_tcpheader){
	//get diff between start of dataz and where tail was found so we don't move more than warranted
	tail_diff = (long int)tail_loc - (long int)dataz;
	if(arg.o.debug){ printf("tail diff:\t%li\n",tail_diff); }

	keep -= ( tcp_tailsz + tail_diff );

	if(arg.o.debug) {printf("Copying %li bytes from %p to %p\n",keep,
				tail_loc+tcp_tailsz, tail_loc); }
	memmove(tail_loc, tail_loc+tcp_tailsz, keep); 

	oldheader_loc -= tcp_tailsz; 	  //We killed the tail string, so we need to update the header location
	  //	}	

      }

      /*First go, in case dataz starts with a header*/
      //&tcp_str[8] is address for start string
      if( oldheader_loc != NULL ){
	tcp_hc++;
	if(arg.o.debug){printf("**\nheader %i loc:%p\n**\n",tcp_hc, oldheader_loc); }
	

	//Get tcp packet header data
	memcpy(tcp_hdr, oldheader_loc, tcp_hdrsz);
	
	if((i-1) % imod == 0) print_tcp_header(tcp_hdr);
	
	//JUNK HEADER RIGHT HERE
	//	if(junk_tcpheader){

	//get diff between start of dataz and where header was found so we don't move more than is warranted
	header_diff = (long int)oldheader_loc - (long int)dataz;
	if(arg.o.debug){ printf("header diff:\t%li\n",header_diff); }

	keep =  count - tcp_hdrsz - header_diff; //Only deal with bytes that haven't been searched or discarded

	if(arg.o.debug) {printf("Copying %li bytes from %p to %p\n",keep,
				oldheader_loc+tcp_hdrsz, oldheader_loc); }
	memmove(oldheader_loc, oldheader_loc+tcp_hdrsz, keep); 
	  //	}	

	//Now data are moved, and oldheader_loc has no header there!
	//loop while we can still find a footer immediately followed by a header
	while( ( keep  > 0 )  &&
	       ( ( header_loc = parse_tcp_header(tcp_hdr, oldheader_loc, keep ) )  != NULL ) ){
	  tcp_hc++;
	  tcp_tc++;

	  //	  printf("**\nheader %i loc:%p\n**\n",tcp_hc,header_loc);
	  //	  if((i-1) % imod == 0) pack_err = print_tcp_header(tcp_hdr);
	  //	  pack_err = print_tcp_header(tcp_hdr);
	
	  //JUNK HEADER RIGHT HERE
	  //	  if(junk_tcpheader){

	  header_diff = (long int)header_loc - (long int)oldheader_loc;
	    
	  if(arg.o.debug) {printf("header diff:\t%li\n",header_diff); }

	  keep = keep -  header_diff - tcp_hdrsz - tcp_tailsz; //only keep bytes not searched or discarded

	  if(arg.o.debug){ printf("Keep+totaldiff=\t%li\n",keep+(long int)header_loc-(long int)dataz); }

	  //Notice that the following memmove looks at header_loc MINUS tcp_tailsz, which is because we want 
	  //to kill the footer of the TCPIP packet (i.e., {0x07,0x06,0x05,0x04,0x03,0x02,0x01,0x00})
	  memmove(header_loc-tcp_tailsz,header_loc+tcp_hdrsz, keep ); 
	  if(arg.o.debug){ printf("Kept %li bytes\n",keep); }
	    //	  }

	  oldheader_loc = header_loc;	

	}//while(we can find headers to kill)
      } //if(oldheader_loc != NULL)
    } //if(arg.o.tcpdata)

    if (arg.o.dt > 0) {
  
      if(arg.o.tcp_data) { fifo_count = count - tcp_tc * tcp_tailsz - tcp_hc * tcp_hdrsz;}
      else { fifo_count = count; }
      // Copy into RTD memory if we're running the display

      fifo_write( fifo, dataz, fifo_count );

      //If we missed either a header or a footer, it will get FFTed and make display a little messy
      if(arg.o.tcp_data){
	if ( (  parse_tcp_header(tcp_hdr, fifo->head, fifo_avail(fifo) ) ) != NULL ) {
	  printf("Missed a tcp header!!!\n"); }
	if( ( memmem(fifo->head, fifo_avail(fifo), tcp_str, tcp_tailsz) ) != NULL ){
	  printf("Missed a footer!!!\n"); }
      }

      if( !arg.o.digitizer_data ) {// Enough to make sure we find a full frame

	if( fifo_avail(fifo) > 2*rtdbytes ){ 
	  if ( (fifo_loc = fifo_search(fifo, fifo_srch, 2*rtdbytes) ) != EXIT_FAILURE ){

	    /* if(arg.o.tcp_data){ */
	    /* //Junk all TCP packet headers */
	    /*   tcp_hc = 0; */
	    /*   oldskip_loc = fifo_loc; */
	    /*   while( ( skip_loc = fifo_skip(skip_str, 8, oldskip_loc, 36,  */
	    /* 				  rtdbytes - (oldskip_loc - fifo_loc), fifo) ) != EXIT_FAILURE ) { */
	    /*     tcp_hc++; */
	    /*     oldskip_loc = skip_loc; */
	    /*   } */
	    /*   //	    if( i % imod == 0 ) { */
	    /*     printf("Killed %i packet headers\n", tcp_hc); */
	    /*     //	    } */
	    /* } */

	    //	  fifo_loc = oldskip_loc;

	    //Junk everything in FIFO before new Dartmouth header
	    fifo_kill(fifo, fifo_loc);
	
	    fifo_read(fifo_outbytes, fifo, rtdbytes);
	    pthread_mutex_lock(arg.rlock);
	    if (arg.o.debug) {
	      printf("file %s rtd moving rtdbytes %i from cfb %p to rtdb %p with %u avail.\n",
		     arg.infile, rtdbytes, dataz, arg.rtdframe, count);
	    }
	    memmove(arg.rtdframe, fifo_outbytes, rtdbytes);
	    pthread_mutex_unlock(arg.rlock);
	  }
	  else {
	    fprintf(stderr, "Couldn't find %s in fifo search!!\n", fifo_srch);
	    fprintf(stderr,"Total bytes read so far:\t%lli\n",wcount);
	    fprintf(stderr,"Percent of input file read:\t%Lf%%\n",100*(long double)wcount/(long double)fstat.st_size);
	  }
	} 
      } else { //it IS digitizer data!
	fifo_read(fifo_outbytes, fifo, rtdbytes);
	pthread_mutex_lock(arg.rlock);
	if (arg.o.debug) {
	  printf("file %s rtd moving rtdbytes %i from cfb %p to rtdb %p with %u avail.\n",
		 arg.infile, rtdbytes, dataz, arg.rtdframe, count);
	}
	memmove(arg.rtdframe, fifo_outbytes, rtdbytes);
	pthread_mutex_unlock(arg.rlock);
      }	  
    }
	
    frames++;
    
    if ((arg.o.maxacq > 0) && (frames > arg.o.maxacq)) {
      *arg.running = false;
    }
    
  }
    
  gettimeofday(&now, NULL);
    
  /* Close the file */
  
  if ( (e = fclose(fp) ) != 0) {
    printe("Couldn't close file %i: %li.\n", arg.infile, e);
    arg.retval = e;
  }
    
  /* Free 'em all */
  free(dataz);
  free(fifo); free(fifo_outbytes);

  telapsed = now.tv_sec-start.tv_sec + 1E-6*(now.tv_usec-start.tv_usec);
    
  printf("Read %lli bytes from %s in %.4f s: %.4f KBps.\n", wcount, arg.infile, telapsed, (wcount/1024.0)/telapsed);
    
  arg.retval = EXIT_SUCCESS; pthread_exit((void *) &arg.retval);
}

static void do_depart(int signum) {
  running = false;
  fprintf(stderr,"\nStopping...");
  
  return;
}
