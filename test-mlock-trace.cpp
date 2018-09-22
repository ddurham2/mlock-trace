#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>

// with some intentional leaks

void* __attribute__ ((noinline)) bar(int a)
{
	void* p = malloc(65536);
	mlock(p, 4096);
	//free(p);
	return p;
}

void* __attribute__ ((noinline)) foo(int a)
{
	return bar(a);
}


int main()
{
	foo(10);

	void* p = foo(11);

	fprintf(stderr, "==== mid-process locks (1) ====\n");
	kill(getpid(), SIGUSR1);
	sleep(1); // time for signal to propagate


	munlock(p, 4096);

	fprintf(stderr, "==== mid-process locks (2) ====\n");
	kill(getpid(), SIGUSR1);
	sleep(1); // time for signal to propagate

	foo(12);

	fprintf(stderr, "==== locks at exit ====\n");
	return 0;
}
