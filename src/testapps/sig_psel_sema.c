/*
 * -----------------------------------------------------------------------------------
 * This test checks if signal delivery, pselect and semaphores are working reliable.
 * It is know to fail on Fedora core 4 with kernel 2.6.17 SMP, on SUSE 10.0 and
 * on PowerPC debian with kernel 2.6.11  
 *
 * Signal/Semaphore/pselect is required by softgun's io-thread.
 *
 * This application uses two threads. main thread waits for a semaphore and sends a
 * signal to second thread.
 * Second thread up's the semaphore and waits for signal from main thread.
 *
 * If semaphores or pselect or signal delivery is not working reliable the
 * main thread will hang. After a timeout a error message is printed
 * -----------------------------------------------------------------------------------
 */
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <semaphore.h>
#include <unistd.h>

sem_t sem;

static pthread_t testthread;
uint32_t count = 0;

static void
handle_sigusr1()
{
	// nothing
}

/*
 * ----------------------------------------------------------------
 * This Thread posts the semaphore and then waits for signal 
 * from main thread.
 * ----------------------------------------------------------------
 */
void *
psel_test(void *cd)
{
	int result;
	struct sigaction sa;
	sigset_t emptyset, blockset;
	sa.sa_handler = handle_sigusr1;	/* Establish signal handler */
	sa.sa_flags = 0;
	sigemptyset(&blockset);	/* Block SIGUSR1 */
	sigemptyset(&emptyset);
	sigaddset(&blockset, SIGUSR1);
	sigprocmask(SIG_BLOCK, &blockset, NULL);
	sigaction(SIGUSR1, &sa, NULL);	// fd change notification
	for (count = 0; count < 1000000; count++) {
		/* Now IO is handled, allow the IO-thread to run again */
		//fprintf(stdout,"\r%d  ",count);
		sem_post(&sem);
		result = pselect(0, NULL, NULL, NULL, NULL, &emptyset);
		//result = select(0,NULL,NULL,NULL,NULL);
	}
	fprintf(stderr, "Signal/pselect/semaphore test successful\n");
	exit(0);
}

/*
 * -----------------------------------------------------------------------------
 * Main Thread creates a second thread, then it waits 
 * for semaphore post from second thread and then sends a signal
 * to second thread
 * -----------------------------------------------------------------------------
 */
int
main(int argc, char *argv[])
{
	int result;
	struct timeval now;
	struct timespec timeout;
	sem_init(&sem, 1, 0);
	result = pthread_create(&testthread, NULL, psel_test, NULL);
	sleep(1);
	while (1) {
		gettimeofday(&now, NULL);
		timeout.tv_nsec = now.tv_usec * 1000;
		timeout.tv_sec = now.tv_sec + 1;
		result = sem_timedwait(&sem, &timeout);
		pthread_kill(testthread, SIGUSR1);
		gettimeofday(&now, NULL);
		if (result == 0) {
			continue;
		}
		fprintf(stderr,
			"Timeout after %d pselects! your Signal/pselect/semaphore implementation is broken\n",
			count);
	}
	exit(0);
}
