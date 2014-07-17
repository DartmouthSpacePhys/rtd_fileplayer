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
  char fifo_srch[18];
  if (arg.o.endian) {
    strcpy(fifo_srch, "aDtromtu hoCllge");
  }
  else {
    strcpy(fifo_srch,"Dartmouth College");
  }
  long int fifo_loc;
  long int skip_loc;
  long int oldskip_loc;
  char *fifo_outbytes;

  int e = 0;
  int receiving;
  
  int rtdbytes;
  double telapsed;

  //TCP stuff
  int packet_hcount = 0;
  struct tcp_header *tcp_header;
  int pack_err;

  int count;
  long long unsigned int i = 0;
  long long unsigned int frames, wcount;
  int imod = 10;

  char *dataz;
  struct tm ct;
  struct timeval start, now, then;

  FILE *fp;

  if (arg.o.debug) { printf("File %s thread init.\n", arg.infile); fflush(stdout); }

  fp = fopen(arg.infile, "r");
  if( fp  == NULL ){
    fprintf(stderr,"Couldn't open %s!!!\n",arg.infile);
    *arg.running = false;
    arg.retval = EXIT_FAILURE; pthread_exit((void *) &arg.retval);
  } 

  dataz = malloc(arg.o.acqsize);

  rtdbytes = arg.o.rtdsize*sizeof(short int);

  fifo = malloc( sizeof(*fifo) );
  fifo_init(fifo, 4*rtdbytes);  
  fifo_outbytes = malloc(rtdbytes);
  char skip_str[8] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
  tcp_header = malloc( sizeof(struct tcp_header) );

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
    gettimeofday(&then, NULL);
    //if (arg.infile == 1) { printf("r"); fflush(stdout); }
    //        check_acq_seq(dev_handle, arg.infile, &fifo_acqseq);
      
      // Good read
	
      // copy and write
	
      // Check DSP header position within FIFO
      // "Dartmouth College "
      // "aDtromtu hoCllge e"
      //            if (arg.o.debug) {
	
      //            printf("p%i: %i\n",arg.infile,hptr[16]*65536+hptr[17]); fflush(stdout);
      //            printf("%li.",hptr-dataz);
      //			rtd_log("Bad Colonel Frame Header Shift on module %i, frame %llu: %i.\n", arg.infile, hptr-cframe->base, frames);
      //    			printe("CFHS on module %i, seq %i: %i.\n", arg.infile, frames, hptr-dataz);
      //            }
	
      // Check alternating LSB position within Colonel Frame
      /*            if (arg.o.debug) {
		    for (int i = 0; i < 175; i++) {
		    ret = cfshort[31+i]&0b1;
		    if (ret) {
		    ret = i;
		    break;
		    }
		    }
		    if (ret != 3) {
 		    rtd_log("Bad LSB Pattern Shift on module %i, frame %llu: %i.\n", arg.infile, hptr-dataz, frames);
		    printe("Bad LSBPS on module %i, frame %llu: %i.\n", arg.infile, hptr-dataz, frames);
		    }
		    } // if debug*/
	
      // Build add-on frame header
      /* memset(&sync, 0, 16); */
      /* strncpy(sync.pattern, "\xFE\x6B\x28\x40", 4); */
      /* sync.t_sec = then.tv_sec-TIME_OFFSET; */
      /* sync.t_usec = then.tv_usec; */
      /* sync.size = count; */
	
      //if (arg.infile == 1) { printf("b"); fflush(stdout); }
      //	        check_acq_seq(dev_handle, arg.infile, &fifo_acqseq);
      //if (arg.infile == 1) { printf("sc: %lu\n", (unsigned long) size_commit); fflush(stdout); }
      // Write header and frame to disk
      //      ret = fwrite(&sync, 1, sizeof(struct frame_sync), ofile);
      //      if (ret != sizeof(struct frame_sync))
      //	rtd_log("Failed to write sync, file %s: %i.", arg.infile, ret);
      //printf("foo"); fflush(stdout);
      //            fflush(ofile);
	
      //if (arg.infile == 1) { printf("w"); fflush(stdout); }
      //	        check_acq_seq(dev_handle, arg.infile, &fifo_acqseq);

    if( arg.o.tcp_data ){
      printf("num_reads = %llu\n", i);
      pack_err = parse_tcp_header(tcp_header, dataz, 40);
      if( ( i == 1 ) && ( pack_err == EXIT_SUCCESS ) ){
	
	printf("Data header start string =\t\t");
	for (int j = 0; j < 8; j++ ){
	  printf("%x",dataz[j]);
	}
	pack_err = print_tcp_header(tcp_header);
      }
    }
    if (arg.o.dt > 0) {

      // Copy into RTD memory if we're running the display
      fifo_write(fifo, dataz, count);

      if( fifo_avail(fifo) > 2*rtdbytes ) {      // Enough to make sure we find a full frame

	if ( (fifo_loc = fifo_search(fifo, fifo_srch, 2*rtdbytes) ) != EXIT_FAILURE ) {

	  if(arg.o.tcp_data){
	  //Junk all TCP packet headers
	    packet_hcount = 0;
	    oldskip_loc = fifo_loc;
	    while( ( skip_loc = fifo_skip(skip_str, 8, oldskip_loc, 36, 
					  rtdbytes - (oldskip_loc - fifo_loc), fifo) ) != EXIT_FAILURE ) {
	      packet_hcount++;
	      oldskip_loc = skip_loc;
	    }
	    //	    if( i % imod == 0 ) {
	      printf("Killed %i packet headers\n", packet_hcount);
	      //	    }
	  }
	  fifo_loc = oldskip_loc;

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
	}
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
