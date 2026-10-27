/*
 * libgputest.dylib: runs the OSMesa/freedreno offscreen test from a constructor, so the
 * kernel exec_inject (boot-arg sweet_inject=<lib>,<program>) can load it into backboardd
 * where dyld/amfid are up and the GPU user client is idle. gputest_main is the regular test
 * with its main() renamed (-Dmain=gputest_main).
 */
#include <stdio.h>
#include <unistd.h>

int gputest_main(void);

__attribute__((constructor)) static void
sweet_gputest_constructor(void)
{
	fprintf(stderr, "GPUTEST: constructor entered (pid %d)\n", (int)getpid());
	gputest_main();
	fprintf(stderr, "GPUTEST: constructor done (pid %d)\n", (int)getpid());
}
