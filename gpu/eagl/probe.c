/*
 * Load probe for libsweetgl: one line into the persistent kernel log as soon as the
 * library is loaded. If an injected backboardd hangs without this line, the hang is in
 * dyld/loader; with the line, it is in the EAGL hooking.
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
sweetgl_load_probe(void)
{
	struct sweet_gpu_log l;
	int fd = open("/dev/sweetgpu", O_WRONLY);

	if (fd < 0) {
		return;
	}
	memset(&l, 0, sizeof(l));
	snprintf(l.msg, sizeof(l.msg), "SWEETGLOG: libsweetgl loaded in pid %d\n", getpid());
	(void)ioctl(fd, SWEET_GPU_IOCTL_LOG, &l);
	close(fd);
}
