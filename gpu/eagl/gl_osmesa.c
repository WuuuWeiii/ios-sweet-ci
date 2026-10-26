/*
 * The OpenGL ES library behind src/eagl.c when that library is our Mesa build
 * (gpu/eagl/mesa/build-mesa.sh: 25.0.7 + the Bergamota patch, OSMesa with an
 * OpenGL ES profile and the OSMesaSweet* direct-surface helpers).
 *
 * eagl.c wants four things besides the gl* functions: a context of some version
 * of OpenGL ES, that context made the calling thread's, a context taken away
 * again, a name for the log, and (optionally) "make this texture BE the
 * IOSurface" - done here through OSMesaSweetAttachDirect, which is the
 * Bergamota patch's equivalent of OES_EGL_image.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hakios.gl.h"

/* Mesa's GL/osmesa.h, as the Bergamota patch interprets it */
#define OSMESA_RGBA                     0x1908
#define OSMESA_FORMAT                   0x22
#define OSMESA_DEPTH_BITS               0x30
#define OSMESA_STENCIL_BITS             0x31
#define OSMESA_PROFILE                  0x33
#define OSMESA_CONTEXT_MAJOR_VERSION    0x36
#define OSMESA_CONTEXT_MINOR_VERSION    0x37
/* value of OSMESA_PROFILE that the Bergamota patch accepts for a GLES context */
#define SWEET_ES_PROFILE                0x36

#define GL_UNSIGNED_BYTE                0x1401
#define GL_TEXTURE_2D                   0x0de1
#define GL_TEXTURE_BINDING_2D           0x8069
#define GL_RENDERER                     0x1f01

typedef struct osmesa_context *OSMesaContext;
extern OSMesaContext OSMesaCreateContextAttribs(const int *attributes, OSMesaContext share);
extern unsigned char OSMesaMakeCurrent(OSMesaContext context, void *buffer, unsigned type, int width, int height);
extern void OSMesaDestroyContext(OSMesaContext context);
extern void (*OSMesaGetProcAddress(const char *name))(void);
extern const unsigned char *glGetString(unsigned name);
extern unsigned glGetError(void);
extern void glGetIntegerv(unsigned name, int *value);

/* OSMesa draws its "window" into memory handed to it. Nothing here draws into a window
 * (the window server draws into textures of its own, see eagl.c): a few pixels will do. */
#define WINDOW  16

struct hakios_gl {
	OSMesaContext context;
	unsigned char window[WINDOW * WINDOW * 4];
};

struct hakios_gl *
hakios_gl_create(int version)
{
	const int attributes[] = {
		OSMESA_FORMAT, OSMESA_RGBA, OSMESA_DEPTH_BITS, 24, OSMESA_STENCIL_BITS, 8,
		OSMESA_PROFILE, SWEET_ES_PROFILE,
		OSMESA_CONTEXT_MAJOR_VERSION, version, OSMESA_CONTEXT_MINOR_VERSION, 0,
		0
	};
	struct hakios_gl *gl = calloc(1, sizeof(*gl));

	if (gl != NULL && (gl->context = OSMesaCreateContextAttribs(attributes, NULL)) == NULL) {
		free(gl);
		gl = NULL;
	}
	return gl;
}

int
hakios_gl_make_current(struct hakios_gl *gl)
{
	if (gl == NULL) {
		return OSMesaMakeCurrent(NULL, NULL, 0, 0, 0);
	}
	return OSMesaMakeCurrent(gl->context, gl->window, GL_UNSIGNED_BYTE, WINDOW, WINDOW);
}

void
hakios_gl_destroy(struct hakios_gl *gl)
{
	OSMesaDestroyContext(gl->context);
	free(gl);
}

const char *
hakios_gl_library(void)
{
	return "Mesa, off-screen (OSMesa, Bergamota patch)";
}

/*
 * The texture bound to GL_TEXTURE_2D is to become the IOSurface's own memory: what is drawn
 * into it is in the surface, what is in the surface is what the texture shows, no copy.
 * OSMesaSweetAttachDirect(name, surface) is the Bergamota patch's version of that (the
 * kernel's GPU driver makes a BO of the surface memory; on the CPU drivers it fails).
 * 0: not done, the caller fills/reads the texture with OpenGL ES calls instead.
 */
int
hakios_gl_surface_texture(struct hakios_gl *gl, uint32_t surface)
{
	static int (*attach_direct)(unsigned name, uint32_t surface);
	static int asked;

	(void)gl;
	if (!asked) {
		asked = 1;
		attach_direct = (int (*)(unsigned, uint32_t))(void *)OSMesaGetProcAddress("OSMesaSweetAttachDirect");
	}
	if (attach_direct == NULL) {
		return 0;
	}
	int name = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &name);
	if (name <= 0) {
		return 0;
	}
	while (glGetError() != 0) {
	}
	if (!attach_direct((unsigned)name, surface)) {
		return 0;
	}
	return glGetError() == 0;
}
