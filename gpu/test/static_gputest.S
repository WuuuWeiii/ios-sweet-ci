/*
 * static_gputest.S -- the smallest possible userspace GPU client: no dyld, no
 * libSystem, raw XNU syscalls only. It opens /dev/sweetgpu and sends one line
 * through the kernel-log ioctl (0x74), which proves that an executable we
 * placed on the root volume runs at all (and tells dyld problems apart).
 */
	.section __TEXT,__text,regular,pure_instructions
	.globl _start
	.align 4
_start:
	/* fd = open("/dev/sweetgpu", O_RDWR)  (xnu syscall 5) */
	mov	x16, #5
	adr	x0, devpath
	mov	x1, #2
	mov	x2, #0
	svc	#0x80
	mov	x19, x0
	tbz	x19, #63, have_fd
	b	exit1

have_fd:
	/* clear the 1008-byte message buffer */
	adr	x20, logbuf
	mov	x21, #0
	mov	x22, #1008
clear_loop:
	strb	w21, [x20, x22]
	subs	x22, x22, #1
	b.ne	clear_loop

	/* copy "STATIC OK: no dyld needed\n" */
	adr	x23, okmsg
	mov	x24, #0
copy_loop:
	ldrb	w25, [x23, x24]
	cbz	w25, copy_done
	strb	w25, [x20, x24]
	add	x24, x24, #1
	b	copy_loop

copy_done:
	/* ioctl(fd, SWEET_GPU_IOCTL_LOG, logbuf)  (xnu syscall 54) */
	mov	x16, #54
	mov	x0, x19
	adr	x1, logcmd_ptr
	ldr	x1, [x1]
	mov	x2, x20
	svc	#0x80

	/* close(fd) (xnu syscall 6) */
	mov	x16, #6
	mov	x0, x19
	svc	#0x80

	/* exit(0) */
	mov	x16, #1
	mov	x0, #0
	svc	#0x80

exit1:
	mov	x16, #1
	mov	x0, #1
	svc	#0x80

	.section __TEXT,__cstring,cstring_literals
devpath:
	.asciz "/dev/sweetgpu"
okmsg:
	.asciz "STATIC OK: no dyld needed\n"

	.align 3
logcmd_ptr:
	.quad 0xC3F04774

	.section __DATA,__data
	.align 4
logbuf:
	.space 1008
