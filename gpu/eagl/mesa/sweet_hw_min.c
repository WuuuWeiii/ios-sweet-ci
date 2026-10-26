/*
 * Minimal hardware screen for the OSMesa target (replaces Bergamota's Darwin sweet_hw.c).
 *
 * The Bergamota version speaks the newer SweetAdreno630 kernel ABI (SURFACE_BO minor 1/2,
 * info.abi_minor, writable aliases).  Our kernel's IOKit client implements the fajita ABI
 * (24-byte INFO, no SURFACE_BO), so the version checks read garbage and the screen can end
 * up broken.  For M6 (offscreen Mesa) only the freedreno screen over the libdrm stand-in is
 * needed: create it and hand it to OSMesa; the IOSurface zero-copy helpers report "not
 * available" so the caller falls back to CPU copies.
 *
 * Works on the phone (-Dhost darwin) and on the Linux host simulation (mock libdrm).
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "pipe/p_screen.h"
#include "util/u_inlines.h"
#include "util/xmlconfig.h"
#include "util/driconf.h"
#include "freedreno/drm/freedreno_drm_public.h"

extern int drmOpenWithType(const char *name, const char *busid, int type);

struct pipe_screen *sweet_hw_create_screen(void);
int sweet_hw_active(void);
struct pipe_resource *sweet_hw_surface_resource(uint32_t surface_id, int opaque, unsigned *w, unsigned *h);
void sweet_hw_surface_sync(uint32_t surface_id);
void sweet_hw_surface_forget(uint32_t surface_id);
struct pipe_resource *sweet_hw_wrapped_resource(uint32_t surface_id, enum pipe_format fmt, unsigned w, unsigned h);
void sweet_hw_wrapped_sync(uint32_t surface_id);
int sweet_hw_surface_alias(uint32_t surface_id, void **base, unsigned *bpr, unsigned *w, unsigned *h);
void sweet_hw_surface_alias_sync(uint32_t surface_id);

static const driOptionDescription sweet_driconf[] = {
#include "pipe-loader/driinfo_gallium.h"
#include "freedreno/driinfo_freedreno.h"
};

static int g_fd = -1;
static struct pipe_screen *g_hw_screen;
static int g_tried;

struct pipe_screen *
sweet_hw_create_screen(void)
{
	static struct driOptionCache cache;
	static struct driOptionCache info;
	static struct pipe_screen_config config;
	const char *e = getenv("SWEET_GPU");

	if (g_hw_screen) {
		return g_hw_screen;
	}
	if (g_tried) {
		return NULL;
	}
	g_tried = 1;
	if (e && e[0] == '0') {
		return NULL;
	}
	g_fd = drmOpenWithType("msm", NULL, 0);
	if (g_fd < 0) {
		fprintf(stderr, "OSMesa/sweet: no SweetAdreno630 (drmOpen failed), using softpipe\n");
		return NULL;
	}
	driParseOptionInfo(&info, sweet_driconf, ARRAY_SIZE(sweet_driconf));
	driParseConfigFiles(&cache, &info, 0, "msm", NULL, NULL, NULL, 0, NULL, 0);
	config.options = &cache;
	config.options_info = &info;
	g_hw_screen = fd_drm_screen_create_renderonly(g_fd, NULL, &config);
	if (!g_hw_screen) {
		fprintf(stderr, "OSMesa/sweet: freedreno screen creation failed, using softpipe\n");
		return NULL;
	}
	if (getenv("SWEET_GPU_DEBUG")) {
		fprintf(stderr, "OSMesa/sweet: freedreno screen: %s / %s\n",
		    g_hw_screen->get_name(g_hw_screen), g_hw_screen->get_vendor(g_hw_screen));
	}
	return g_hw_screen;
}

int
sweet_hw_active(void)
{
	return g_hw_screen != NULL;
}

/* No IOSurface zero-copy with this ABI: the caller uses CPU copies. */
struct pipe_resource *
sweet_hw_surface_resource(uint32_t surface_id, int opaque, unsigned *w, unsigned *h)
{
	(void)surface_id; (void)opaque; (void)w; (void)h;
	return NULL;
}

void
sweet_hw_surface_sync(uint32_t surface_id)
{
	(void)surface_id;
}

void
sweet_hw_surface_forget(uint32_t surface_id)
{
	(void)surface_id;
}

struct pipe_resource *
sweet_hw_wrapped_resource(uint32_t surface_id, enum pipe_format fmt, unsigned w, unsigned h)
{
	(void)surface_id; (void)fmt; (void)w; (void)h;
	return NULL;
}

void
sweet_hw_wrapped_sync(uint32_t surface_id)
{
	(void)surface_id;
}

int
sweet_hw_surface_alias(uint32_t surface_id, void **base, unsigned *bpr, unsigned *w, unsigned *h)
{
	(void)surface_id; (void)base; (void)bpr; (void)w; (void)h;
	return 0;
}

void
sweet_hw_surface_alias_sync(uint32_t surface_id)
{
	(void)surface_id;
}
