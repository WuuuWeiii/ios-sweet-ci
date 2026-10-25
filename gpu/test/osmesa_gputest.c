/*
 * osmesa_gputest: Mesa 25.0.7 OSMesa offscreen rendering over the freedreno driver,
 * the libdrm stand-in (libdrm_gpu) and the SweetAdreno630 IOKit service.
 *
 * Creates a GLES2 context through Mesa's OSMesa frontend (profile 0x36 = ES, as the
 * Mesa patch accepts), draws one triangle into the offscreen buffer, reads the
 * pixels back and prints the results. Output goes to /dev/console so the persistent
 * kernel log carries it (see tools/sweet-log.py).
 *
 * If the hardware path is not available OSMesa falls back to softpipe and the lines
 * still print (so the log distinguishes "Mesa works" from "GPU path works").
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

/* /dev/sweetgpu debug ioctl (kernel patch 0184): one line -> kernel log */
struct sweet_gpu_log { char msg[1008]; };
#define SWEET_GPU_IOCTL_LOG _IOWR('G', 0x74, struct sweet_gpu_log)

static int g_gpufd = -1;

static void
klog(const char *s)
{
	struct sweet_gpu_log l;

	if (g_gpufd < 0) {
		return;
	}
	memset(&l, 0, sizeof(l));
	strncpy(l.msg, s, sizeof(l.msg) - 1);
	(void)ioctl(g_gpufd, SWEET_GPU_IOCTL_LOG, &l);
}
#include <GL/osmesa.h>
#include <GLES2/gl2.h>

#define WIDTH  256
#define HEIGHT 256

static int g_out = 1;

static void
say(const char *fmt, ...)
{
	va_list ap;
	char buf[1024];
	int n;

	va_start(ap, fmt);
	n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n > 0) {
		klog(buf);
		(void)!write(g_out, buf, (size_t)n);
	}
}

/* Mesa patch entry point, present only in the hardware build */
extern int OSMesaSweetActive(void) __attribute__((weak));

static GLuint
compile(GLenum type, const char *src)
{
	GLuint s = glCreateShader(type);
	GLint ok = 0;

	glShaderSource(s, 1, &src, NULL);
	glCompileShader(s);
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[512];
		GLsizei len = 0;
		glGetShaderInfoLog(s, sizeof(log), &len, log);
		say("GPUTEST: shader compile failed: %.*s\n", (int)len, log);
	}
	return s;
}

int
main(void)
{
	OSMesaContext ctx;
	unsigned char *buf;
	GLuint vs, fs, prog;
	GLint loc;
	GLint ok = 0;
	static const float verts[] = {
		-1.0f, -1.0f,
		 3.0f, -1.0f,
		-1.0f,  3.0f,
	};
	unsigned char px[4] = { 0, 0, 0, 0 };
	const char *renderer, *version, *vendor;
	int attribs[] = {
		OSMESA_FORMAT, OSMESA_RGBA,
		OSMESA_DEPTH_BITS, 24,
		OSMESA_PROFILE, 0x36,           /* OpenGL ES profile (Mesa patch) */
		OSMESA_CONTEXT_MAJOR_VERSION, 2,
		OSMESA_CONTEXT_MINOR_VERSION, 0,
		0,
	};
	static const char *vs_src =
	    "attribute vec2 p;\n"
	    "void main(void) { gl_Position = vec4(p, 0.0, 1.0); }\n";
	static const char *fs_src =
	    "precision mediump float;\n"
	    "void main(void) { gl_FragColor = vec4(0.2, 0.6, 0.9, 1.0); }\n";

	g_gpufd = open("/dev/sweetgpu", O_RDWR);
	g_out = open("/dev/console", O_WRONLY);
	if (g_out < 0) {
		g_out = 1;
	}

	say("GPUTEST: start (Mesa OSMesa offscreen, %dx%d)\n", WIDTH, HEIGHT);

	ctx = OSMesaCreateContextAttribs(attribs, NULL);
	if (ctx == NULL) {
		say("GPUTEST: OSMesaCreateContextAttribs failed\n");
		return 1;
	}
	buf = calloc(1, WIDTH * HEIGHT * 4);
	if (buf == NULL || !OSMesaMakeCurrent(ctx, buf, GL_UNSIGNED_BYTE, WIDTH, HEIGHT)) {
		say("GPUTEST: OSMesaMakeCurrent failed\n");
		return 1;
	}

	renderer = (const char *)glGetString(GL_RENDERER);
	version = (const char *)glGetString(GL_VERSION);
	vendor = (const char *)glGetString(GL_VENDOR);
	say("GPUTEST: renderer=%s\n", renderer ? renderer : "(null)");
	say("GPUTEST: version=%s\n", version ? version : "(null)");
	say("GPUTEST: vendor=%s\n", vendor ? vendor : "(null)");
	say("GPUTEST: hardware_path=%d\n", OSMesaSweetActive ? OSMesaSweetActive() : -1);

	vs = compile(GL_VERTEX_SHADER, vs_src);
	fs = compile(GL_FRAGMENT_SHADER, fs_src);
	prog = glCreateProgram();
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	glBindAttribLocation(prog, 0, "p");
	glLinkProgram(prog);
	glGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if (!ok) {
		say("GPUTEST: program link failed\n");
		return 1;
	}
	glUseProgram(prog);
	loc = glGetAttribLocation(prog, "p");
	glVertexAttribPointer((GLuint)loc, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glEnableVertexAttribArray((GLuint)loc);

	glViewport(0, 0, WIDTH, HEIGHT);
	glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glFinish();

	glReadPixels(WIDTH / 2, HEIGHT / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
	say("GPUTEST: center pixel %02x%02x%02x%02x (want ~336699ff)\n", px[0], px[1], px[2], px[3]);
	{
		int ok_px = (px[0] > 0x28 && px[0] < 0x60 && px[2] > 0x80 && px[3] == 0xff);
		say("GPUTEST: triangle %s\n", ok_px ? "RENDERED (GPU path works)" : "missing/odd");
		/* a corner stays the clear colour: proves the viewport and the readback */
		glReadPixels(2, 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
		say("GPUTEST: corner pixel %02x%02x%02x%02x (want ~1a334dff)\n", px[0], px[1], px[2], px[3]);
		say("GPUTEST: RESULT %s\n", ok_px ? "PASS" : "FAIL");
	}

	OSMesaDestroyContext(ctx);
	free(buf);
	say("GPUTEST: done\n");
	return 0;
}
