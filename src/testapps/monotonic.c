/*
 * ----------------------------------------------------------------------------------------
 * This application tests if your clock on the target system is running monotonic.
 * It sometimes happens in brocken embedded Linux Kernel implementations that time
 * does not increase monotonic because of nonatomic timer reads.
 * ----------------------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
int 
main() {
	struct timeval old,new;
 	gettimeofday(&old,NULL);
	uint8_t c;
	fcntl(1,F_SETFL,O_NONBLOCK);
	while(1) {
		if(read(1,&c,1) == 1) {
			if(c=='q') {
				fcntl(1,F_SETFL,0);
				exit(0);
			}
		}
 		gettimeofday(&new,NULL);
		//fprintf(stderr," new %u.%u, old %u.%u\n",new.tv_sec,new.tv_usec,old.tv_sec,old.tv_usec);
		if(new.tv_sec < old.tv_sec) {
			fprintf(stderr,"Your clock is not monotonic\n");
		}
		if((new.tv_sec == old.tv_sec) && (new.tv_usec < old.tv_usec)) {
			fprintf(stderr,"Your clock is not monotonic old %u, new %u\n",new.tv_usec,old.tv_usec);
		}
		old = new;
	}
	exit(0);
}
