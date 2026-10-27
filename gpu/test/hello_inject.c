/*
 * libhello.dylib: proof that the kernel's exec_inject (boot-arg sweet_inject) loads an
 * injected library into the named program and that its constructor runs there.
 * It writes one line to the persistent kernel log through /dev/sweetgpu (kernel patch 0184).
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

struct sweet_gpu_log { char msg[1008]; };
#define SWEET_GPU_IOCTL_LOG _IOWR('G', 0x74, struct sweet_gpu_log)

__attribute__((constructor)) static void
sweet_hello_probe(void)
{
	struct sweet_gpu_log l;
	int fd = open("/dev/sweetgpu", O_WRONLY);

	if (fd < 0) {
		return;
	}
	memset(&l, 0, sizeof(l));
	snprintf(l.msg, sizeof(l.msg), "SWEETGLOG: HELLO injected dylib in pid %d\n", getpid());
	(void)ioctl(fd, SWEET_GPU_IOCTL_LOG, &l);
	close(fd);
}
