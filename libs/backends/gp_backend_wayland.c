// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * Copyright (C) 2023-2026 Cyril Hrubis <metan@ucw.cz>
 * Copyright (C) 2023 Jiri Dluhos <jiri.bluebear.dluhos@gmail.com>
 */

#include <wayland-client-protocol.h>

#define _GNU_SOURCE	/* memfd_create() */

#include "../../config.h"

#include <core/gp_debug.h>
#include <core/gp_pixmap.h>
#include <utils/gp_utf.h>
#include <backends/gp_backends.h>

#ifdef HAVE_WAYLAND

#include <wayland-util.h>
#include <wayland-version.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-unstable-v1.h"

#include <sys/mman.h>	        /* mmap(), munmap() */
#include <linux/memfd.h>        /* MFD_CLOEXEC, MFD_ALLOW_SEALING */

int memfd_create(const char *name, unsigned int flags);

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#define UN(x) x __attribute__((unused))

/* theoretically 3 should be enough but don't believe that;
   with resizing etc. we may consume significantly more. */
#define MAX_PENDING_FRAMES 8

struct client_state {
	/* display */
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_shm *shm;
	struct wl_compositor *compositor;
	struct xdg_wm_base *wm_base;

	/* input */
	struct wl_seat *seat;
	struct wl_keyboard *keyboard;
	struct wl_pointer *pointer;

	/* keymap */
	struct xkb_context *keymap_context;
	struct xkb_state *keymap_state;
	struct xkb_keymap *keymap;

	/* window */
	struct wl_surface *surface;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *xdg_toplevel;
	/* window decoration */
	struct zxdg_decoration_manager_v1 *decoration_manager;
	struct zxdg_toplevel_decoration_v1 *decoration;

	/* output size and physical size */
	struct wl_output *output;
	unsigned int dw, dw_mm, dh, dh_mm;

	/* gfxprim */

	/* window width and height */
	uint32_t w, h;

	gp_pixel pixel_type;
	gp_ev_queue ev_queue;
	gp_fd fd;

	gp_backend *backend;

	/* received the initial configure request? */
	bool configured;
	bool surface_configured;

	struct buffered_frame* frames[MAX_PENDING_FRAMES];
	struct buffered_frame* current_frame;
};

static struct client_state state = {};

/* --- Frame --- */

struct buffered_frame {
	int width;
	int height;
	size_t data_size;
	uint32_t* data;
	struct wl_buffer* buffer;
	int fd;
	bool busy;
};

/* To be called when the Wayland compositor is finished with the buffer,
   and the buffer can be reused. */
void buffer_release_callback(void *data, struct wl_buffer *wl_buffer)
{
	/* find the appropriate frame and mark it as ready for reuse */
	for (int i=0; i<MAX_PENDING_FRAMES; i++) {
		if (state.frames[i] && state.frames[i]->buffer == wl_buffer) {
			state.frames[i]->busy = false;
			return;
		}
	}
}

static struct wl_buffer_listener buffer_listener = {
	.release = buffer_release_callback,
};

/* Destroys the buffered frame and frees all its substructures.
   Wayland spec explicitly allows to destroy a frame even when it is in use by the server. */
void buffered_frame_destroy(struct buffered_frame* frame)
{
	if (frame->buffer) {
		wl_buffer_destroy(frame->buffer);
	}
	if (frame->data && frame->data != MAP_FAILED) {
		munmap(frame->data, frame->data_size);
	}
	if (frame->fd) {
		close(frame->fd);
	}
	free(frame);
}

static struct buffered_frame* buffered_frame_create(struct client_state* state, int width, int height)
{
	/* Wayland spec specifically forbids zero width/height */
	if (width <= 0 || height <= 0) {
		GP_FATAL("wayland: error allocating frame: both dimensions must be >0");
		return NULL;
	}

	/* allocate the data structure and zero it */
	struct buffered_frame* frame = calloc(1, sizeof(struct buffered_frame));
	if (!frame) {
		GP_FATAL("wayland: error allocating frame: calloc() failed");
		return NULL;
	}

	frame->width = width;
	frame->height = height;

	/* size of the frame; it has 4 bytes per pixel, no special padding */
	const int stride = frame->width * 4;
	frame->data_size = stride * frame->height;

	/* use memfd_create() to make a guaranteedly anonymous and unique in-memory file */
	frame->fd = memfd_create("frame", MFD_CLOEXEC|MFD_ALLOW_SEALING);
	if (!frame->fd) {
		GP_FATAL("wayland: error allocating frame: memfd_create() failed");
		buffered_frame_destroy(frame);
		return NULL;
	}

	/* resize it to the frame size; if this fails, we probably don't have enough address space */
	for(;;) {
		int ret = ftruncate(frame->fd, frame->data_size);
		if (ret == 0) { break; }
		if (errno != EINTR) {
			GP_FATAL("wayland: error allocating frame: ftruncate() failed (out of memory?)");
			buffered_frame_destroy(frame);
			return NULL;
		}
	}

	/* mmap it; if this fails, we probably don't have enough free RAM */
	frame->data = mmap(NULL, frame->data_size, PROT_READ|PROT_WRITE, MAP_SHARED, frame->fd, 0);
	if (frame->data == MAP_FAILED) {
		GP_FATAL("wayland: error allocating frame: mmap() failed (out of memory?)");
		buffered_frame_destroy(frame);
		return NULL;
	}

	/* create a temporary pool for allocating buffers from; the server needs this
	   to know where to find the shared memory; the fd indicates the anonymous file */
	struct wl_shm_pool* pool = wl_shm_create_pool(state->shm, frame->fd, frame->data_size);
	if (!pool) {
		GP_FATAL("wayland: error allocating frame: wl_shm_create_pool() failed");
		buffered_frame_destroy(frame);
		return NULL;
	}

	/* create the buffer encapsulating the shared memory */
	frame->buffer = wl_shm_pool_create_buffer(pool, 0, state->w, state->h, stride, WL_SHM_FORMAT_XRGB8888);
	if (!frame->buffer) {
		GP_FATAL("wayland: error allocating frame: wl_shm_pool_create_buffer() failed");
		buffered_frame_destroy(frame);
		wl_shm_pool_destroy(pool);
		return NULL;
	}

	/* the pool can be destroyed here, it's reference counted */
	wl_shm_pool_destroy(pool);

	/* add listener to indicate when the buffer is ready for reuse */
	wl_buffer_add_listener(frame->buffer, &buffer_listener, state);

	return frame;
}

static struct buffered_frame* get_next_free_frame(struct client_state* state)
{
	for (int i=0; i<MAX_PENDING_FRAMES; i++) {
		if (state->frames[i] && !state->frames[i]->busy) {
			return state->frames[i];
		}
	}
	for (int i=0; i<MAX_PENDING_FRAMES; i++) {
		if (state->frames[i] == NULL) {
			state->frames[i] = buffered_frame_create(state, state->w, state->h);
			if (!state->frames[i])
				return NULL;
			fprintf(stderr, "wayland: created frame for slot #%d (%dx%d)\n", i, state->w, state->h);
			return state->frames[i];
		}
	}
	GP_FATAL("wayland: all frames are busy, cannot create more (server lockup?)");
	return NULL;
}

/* --- Callbacks needed for Display --- */

/* To be called when the wayland library itself detects an error. */
static void
wl_error_callback(void* data, struct wl_display* display, void* unused, uint32_t code, const char* message)
{
	fprintf(stderr, "wayland: error (code %u): %s\n", code, message);
}

static const struct wl_display_listener display_listener = {
	.error = wl_error_callback
};

/* To be called when the wayland compositor reports the pixel format of the shared memory. */
static void shm_format_callback(void *data, struct wl_shm *shm, uint32_t format)
{
	struct client_state *state = data;

	(void) shm;

	if (state->pixel_type != GP_PIXEL_UNKNOWN)
		return;

	switch (format) {
	case WL_SHM_FORMAT_XRGB8888:
		state->pixel_type = GP_PIXEL_xRGB8888;
	break;
	case WL_SHM_FORMAT_RGB888:
		state->pixel_type = GP_PIXEL_RGB888;
	break;
	}

	if (state->pixel_type == GP_PIXEL_UNKNOWN)
		return;

	GP_DEBUG(1, "SHM pixel format %s (%"PRIx32")",
	         gp_pixel_type_name(state->pixel_type), format);
}

static const struct wl_shm_listener shm_listener = {
	.format = shm_format_callback
};

/* To be called when a wayland compositor sends us a ping (keepalive/heartbeat) message. */
void xdg_wm_ping_callback(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {

	/* reply with pong to confirm we are still alive */
	xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
	xdg_wm_ping_callback
};

static void
keyboard_handle_keymap(void *data, struct wl_keyboard UN(*keyboard),
                       uint32_t format, int fd, uint32_t size)
{
	struct client_state *state = data;

	GP_DEBUG(0, "keymap change");

	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
		GP_WARN("Unknown keymap format!");
		goto err0;

	}

	char *keymap_str = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (keymap_str == MAP_FAILED) {
		GP_WARN("Failed to map keymap");
		goto err0;
	}

	xkb_keymap_unref(state->keymap);
	xkb_context_unref(state->keymap_context);
	xkb_state_unref(state->keymap_state);

	state->keymap_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	state->keymap = xkb_keymap_new_from_string(state->keymap_context,
	                                           keymap_str,
	                                           XKB_KEYMAP_FORMAT_TEXT_V1,
	                                           XKB_KEYMAP_COMPILE_NO_FLAGS);
	state->keymap_state = xkb_state_new(state->keymap);

	munmap(keymap_str, size);

	GP_DEBUG(1, "Keymap created");
err0:
	close(fd);
}

static void
keyboard_handle_enter(void UN(*data), struct wl_keyboard UN(*keyboard),
                      uint32_t UN(serial), struct wl_surface UN(*surface),
                      struct wl_array UN(*keys))
{
}

static void
keyboard_handle_leave(void UN(*data), struct wl_keyboard UN(*keyboard),
                      uint32_t UN(serial), struct wl_surface UN(*surface))
{
}

static void
keyboard_handle_key(void *data, struct wl_keyboard UN(*keyboard),
                    uint32_t UN(serial), uint32_t UN(time), uint32_t key,
                    uint32_t key_state)
{
	struct client_state *state = data;
	gp_backend *backend = state->backend;
	int ret;
	char buf[128];

	ret = xkb_state_key_get_utf8(state->keymap_state, key+8, buf, sizeof(buf));
	if (ret) {
		uint32_t utf;
		const char *str = buf;
		size_t len = gp_utf8_strlen(str);

		/*
		 * Send all characters without a key scancode, the key scancode
		 * is send to the last one.
		 */
		while ((utf = gp_utf8_next(&str))) {
			gp_ev_queue_push_key(backend->event_queue,
			                     --len ? 0 : key, key_state, utf, 0);
		}

		return;
	}

	gp_ev_queue_push_key(backend->event_queue, key, key_state, 0, 0);
}

static void
keyboard_handle_modifiers(void *data, struct wl_keyboard UN(*keyboard),
                          uint32_t UN(serial), uint32_t mods_depressed,
                          uint32_t mods_latched, uint32_t mods_locked,
                          uint32_t group)
{
	struct client_state *state = data;

	xkb_state_update_mask(state->keymap_state,
	                      mods_depressed, mods_latched, mods_locked,
	                      0, 0, group);
}

static const struct wl_keyboard_listener keyboard_listener = {
	.keymap = keyboard_handle_keymap,
	.enter = keyboard_handle_enter,
	.leave = keyboard_handle_leave,
	.key = keyboard_handle_key,
	.modifiers = keyboard_handle_modifiers,
};

static void
pointer_handle_enter(void UN(*data), struct wl_pointer UN(*pointer),
                     uint32_t UN(serial), struct wl_surface UN(*surface),
		     wl_fixed_t UN(sx), wl_fixed_t UN(sy))
{
}

static void
pointer_handle_leave(void UN(*data), struct wl_pointer UN(*pointer),
                     uint32_t UN(serial), struct wl_surface UN(*surface))
{
}

static void
pointer_handle_motion(void UN(*data), struct wl_pointer UN(*pointer),
		      uint32_t UN(time), wl_fixed_t sx, wl_fixed_t sy)
{
	struct client_state *state = data;
	assert(state);

	gp_backend *backend = state->backend;

	gp_ev_queue_push_rel_to(backend->event_queue, sx/256, sy/256, 0);
}

static void
pointer_handle_button(void *data, struct wl_pointer UN(*pointer),
                      uint32_t UN(serial), uint32_t UN(time), uint32_t button,
                      uint32_t button_state)
{
	struct client_state *state = data;
	assert(state);

	gp_backend *backend = state->backend;

	gp_ev_queue_push_key(backend->event_queue, button, button_state, 0, 0);
}

static void
pointer_handle_axis(void UN(*data), struct wl_pointer UN(*pointer),
		    uint32_t UN(time), uint32_t axis,  wl_fixed_t value)
{
	struct client_state *state = data;
	assert(state);

	gp_backend *backend = state->backend;

	switch (axis) {
	case WL_POINTER_AXIS_SOURCE_WHEEL:
		gp_ev_queue_push(backend->event_queue, GP_EV_REL, GP_EV_REL_WHEEL, value/(15 * 256), 0);
	break;
	}
}

static const struct wl_pointer_listener pointer_listener = {
	.enter = pointer_handle_enter,
	.leave = pointer_handle_leave,
	.motion = pointer_handle_motion,
	.button = pointer_handle_button,
	.axis = pointer_handle_axis,
};

static void seat_handle_capabilities(void *data, struct wl_seat *seat,
                                     enum wl_seat_capability caps)
{
	struct client_state *state = data;

        if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !state->keyboard) {
                state->keyboard = wl_seat_get_keyboard(seat);
                wl_keyboard_add_listener(state->keyboard, &keyboard_listener, state);
        } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && state->keyboard) {
                wl_keyboard_destroy(state->keyboard);
                state->keyboard = NULL;
        }

	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !state->pointer) {
		state->pointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(state->pointer, &pointer_listener, state);
	} else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && state->pointer) {
		wl_pointer_destroy(state->pointer);
		state->pointer = NULL;
	}
}

static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_handle_capabilities,
};

static void display_handle_geometry(void *data, struct wl_output UN(*wl_output),
                                    int32_t UN(x), int32_t UN(y),
                                    int32_t physical_width, int32_t physical_height,
                                    int32_t UN(subpixel), const char UN(*make),
                                    const char UN(*model), int32_t UN(transform))
{
	struct client_state *state = data;

	state->dw_mm = physical_width;
	state->dh_mm = physical_height;
}

static void display_handle_mode(void *data, struct wl_output UN(*wl_output),
                                uint32_t UN(flags), int32_t width, int32_t height,
                                int32_t UN(refresh))
{
	struct client_state *state = data;

	state->dw = width;
	state->dh = height;
}

static void display_handle_done(void UN(*data), struct wl_output UN(*wl_output))
{
}

static void display_handle_scale(void UN(*data), struct wl_output UN(*wl_output),
                                 int32_t UN(factor))
{
}

static void display_handle_name(void UN(*data), struct wl_output UN(*wl_output),
                                const char UN(*name))
{
}

static void display_handle_desc(void UN(*data), struct wl_output UN(*wl_output),
                                const char UN(*desc))
{
}

static const struct wl_output_listener output_listener = {
	display_handle_geometry,
	display_handle_mode,
	display_handle_done,
	display_handle_scale,
	display_handle_name,
	display_handle_desc,
};

/* To be called upon query whether we want to display our own window decorations. */
static void configure_decorations(void UN(*data),
                                  struct zxdg_toplevel_decoration_v1 UN(*zxdg_toplevel_decoration_v1),
                                  uint32_t mode)
{
	if (mode != ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE)
		GP_WARN("Failed to turn on server side decorations!");
}

static struct zxdg_toplevel_decoration_v1_listener decoration_listener = {
	configure_decorations,
};

/* --- Registry --- */

/* To be called when a global name is removed (invalidated) from the registry. */
static void registry_global_remove_callback(void UN(*data), struct wl_registry UN(*registry), uint32_t UN(name))
{
}

/* To be called when a global name from the registry is reported to the client. */
static void registry_global_callback(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
	struct client_state *state = data;

	(void) version;

	GP_DEBUG(5, "Wayland interface '%s'", interface);

	if (!strcmp(interface, "wl_shm")) {
		fprintf(stderr, "wayland: registry: received name: wl_shm\n");

		state->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);

		wl_shm_add_listener(state->shm, &shm_listener, state);
		return;
	}

	if (!strcmp(interface, "wl_compositor")) {
		fprintf(stderr, "wayland: registry: received name: wl_compositor\n");
		state->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
		return;
	}

	if (!strcmp(interface, "xdg_wm_base")) {
		fprintf(stderr, "wayland: registry: received name: xdg_wm_base\n");
		state->wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(state->wm_base, &xdg_wm_base_listener, state);
		return;
	}

	if (!strcmp(interface, "wl_seat")) {
		fprintf(stderr, "wayland: registry: received name: wl_seat\n");
		state->seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
		wl_seat_add_listener(state->seat, &seat_listener, state);
		return;
	}

	if (!strcmp(interface, "zxdg_decoration_manager_v1")) {
		if (version != 1) {
			GP_WARN("Unexpected decoration manager version %"PRIu32, version);
			return;
		}
		fprintf(stderr, "wayland: registry: received name: zxdg_decoration_manager_v1\n");
		state->decoration_manager = wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1);
		return;
	}

	if (!strcmp(interface, "wl_output")) {
		if (!state->output) {
			fprintf(stderr, "wayland: registry: received name: wl_output\n");
			state->output = wl_registry_bind(registry, name, &wl_output_interface, 1);
			wl_output_add_listener(state->output, &output_listener, state);
		}
		else {
			fprintf(stderr, "wayland: registry: received name again: wl_output (rebinding)\n");
			state->output = wl_registry_bind(registry, name, &wl_output_interface, 1);
			wl_output_add_listener(state->output, &output_listener, state);
		}
		return;
	}
}

static struct wl_registry_listener registry_listener = {
	.global = registry_global_callback,
	.global_remove = registry_global_remove_callback,
};

/* --- Display connection --- */

/* Disconnects from the display. */
static void display_disconnect(struct client_state *state)
{
	if (state->shm)
		wl_shm_destroy(state->shm);

	if (state->wm_base)
		xdg_wm_base_destroy(state->wm_base);

	if (state->compositor)
		wl_compositor_destroy(state->compositor);

	wl_registry_destroy(state->registry);
	wl_display_flush(state->display);
	wl_display_disconnect(state->display);
}

/* Connect to the display, initializing the client state structure. */
static int
display_connect(const char *disp_name, struct client_state *state)
{
	memset(state, 0, sizeof(struct client_state));

	state->pixel_type = GP_PIXEL_UNKNOWN;

	state->display = wl_display_connect(disp_name);
	if (!state->display) {
		GP_FATAL("wl_display_connect() failed, is a Wayland compositor running?");
		return 1;
	}

	static struct wl_display_listener display_listener;
	memset(&display_listener, 0, sizeof(display_listener));
	display_listener.error = wl_error_callback;

	wl_display_add_listener(state->display, &display_listener, state);

	state->registry = wl_display_get_registry(state->display);

	// static struct wl_registry_listener registry_listener;
	// memset(&registry_listener, 0, sizeof(registry_listener));
	// registry_listener.global = registry_global_callback;
	// registry_listener.global_remove = registry_global_remove_callback;

	wl_registry_add_listener(state->registry, &registry_listener, state);

	/* first roundtrip; here, the server should send us various names from the registry */
	fprintf(stderr, "wayland: triggering first roundtrip...\n");
	wl_display_roundtrip(state->display);

	if (!state->shm) {
		GP_FATAL("wayland: wl_shm interface not supported");
		display_disconnect(state);
		return 1;
	}

	/* we added shm listener in the first roundtrip trigger it with second call */
	fprintf(stderr, "wayland: triggering second roundtrip...\n");
	wl_display_roundtrip(state->display);

	if (state->pixel_type == GP_PIXEL_UNKNOWN) {
		GP_FATAL("Failed to match a pixel type");
		display_disconnect(state);
		return 1;
	}

	fprintf(stderr, "wayland: display connection ready\n");
	return 0;
}

/* --- Surface --- */

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base,
                             uint32_t serial)
{
	(void) data;

	xdg_wm_base_pong(xdg_wm_base, serial);
}

static void window_destroy(struct client_state *state)
{
	if (state->xdg_toplevel)
		xdg_toplevel_destroy(state->xdg_toplevel);

	if (state->xdg_surface)
		xdg_surface_destroy(state->xdg_surface);

	wl_surface_destroy(state->surface);
}

static void toplevel_configure(void *data, struct xdg_toplevel UN(*toplevel),
                               int32_t w, int32_t h, struct wl_array UN(*states))
{
	struct client_state *state = data;

	state->configured = true;

	if (w == 0 || h == 0) {
		/* Compositor is deferring to us */
		return;
	}
	state->w = w;
	state->h = h;

	gp_ev_queue_push_render_stop(state->backend->event_queue, 0);
}

static void toplevel_close(void *data, struct xdg_toplevel UN(*toplevel))
{
	struct client_state *st = data;

	gp_ev_queue_push(st->backend->event_queue, GP_EV_SYS, GP_EV_SYS_QUIT, 0, 0);
}

static const struct xdg_toplevel_listener toplevel_listener = {
	.configure = toplevel_configure,
	.close = toplevel_close,
};

static void
xdg_surface_configure(void* data, struct xdg_surface *xdg_surface, uint32_t serial)
{
	struct client_state *state = data;
	assert(state);

	state->surface_configured = true;
	xdg_surface_ack_configure(xdg_surface, serial);
}

struct xdg_surface_listener surface_listener = {
	.configure = xdg_surface_configure,
};

static int window_create(struct client_state *state, unsigned int w, unsigned int h, const char *caption)
{
	state->w = w;
	state->h = h;

	state->surface = wl_compositor_create_surface(state->compositor);
	state->xdg_surface = xdg_wm_base_get_xdg_surface(state->wm_base, state->surface);

	state->configured = false;

	if (!state->xdg_surface) {
		GP_FATAL("Failed to get xdg_surface");
		window_destroy(state);
		return 1;
	}

	xdg_surface_add_listener(state->xdg_surface, &surface_listener, state);

	state->xdg_toplevel = xdg_surface_get_toplevel(state->xdg_surface);
	if (!state->xdg_toplevel) {
		GP_FATAL("Failed to get xdg_toplevel");
		window_destroy(state);
		return 1;
	}

	xdg_toplevel_add_listener(state->xdg_toplevel, &toplevel_listener, state);
	xdg_toplevel_set_app_id(state->xdg_toplevel, caption);
	xdg_toplevel_set_title(state->xdg_toplevel, caption);

	if (state->decoration_manager) {
		state->decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(state->decoration_manager, state->xdg_toplevel);
		zxdg_toplevel_decoration_v1_add_listener(state->decoration, &decoration_listener, state);
		zxdg_toplevel_decoration_v1_set_mode(state->decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}

	wl_surface_commit(state->surface);

	/* the Wayland compositor should send us a confirmation; wait for it (calling attach() earlier than that is illegal) */
	while (!state->surface_configured) {
		wl_display_roundtrip(state->display);
	}

	struct buffered_frame* frame = get_next_free_frame(state);
	assert(frame);

	wl_surface_attach(state->surface, frame->buffer, frame->width, frame->height);
	wl_surface_commit(state->surface);

	return 0;
}

static void wayland_update(gp_backend *self)
{
	gp_backend* backend = self;

	//TODO: we need two buffers and swap the backend->pixmap on attach
	//      so that we avoid drawing into a memory that is currently
	//      attached to surface

	state.current_frame->busy = true;
	wl_surface_attach(state.surface, state.current_frame->buffer, state.w, state.h);
	wl_surface_damage(state.surface, 0, 0, state.w, state.h);
	wl_surface_commit(state.surface);
	wl_display_flush(state.display);

	state.current_frame = get_next_free_frame(&state);
	assert(state.current_frame);
	assert(state.current_frame->data);

	backend->pixmap->pixels = (uint8_t*)(state.current_frame->data);
}

static void wayland_update_rect(gp_backend* self, gp_coord x, gp_coord y, gp_coord w, gp_coord h)
{
	gp_backend* backend = self;

	state.current_frame->busy = true;
	wl_surface_attach(state.surface, state.current_frame->buffer, state.w, state.h);
	wl_surface_damage(state.surface, x, y, w, h);
	wl_surface_commit(state.surface);
	wl_display_flush(state.display);

	state.current_frame = get_next_free_frame(&state);
	assert(state.current_frame);
	assert(state.current_frame->data);

	backend->pixmap->pixels = (uint8_t*)(state.current_frame->data);
}

static enum gp_backend_ret wayland_set_attr(gp_backend* self, enum gp_backend_attr attrs, const void* values)
{
	(void) self;
	(void) attrs;
	(void) values;

	return GP_BACKEND_NOTSUPP;
}

static enum gp_poll_event_ret wayland_process_fd(gp_fd *self)
{
	(void) self;

	wl_display_dispatch(state.display);

	return 0;
}

static int wayland_render_stopped(gp_backend* self)
{
	(void) self;

	//TODO: Resize buffer

//	gp_ev_queue_push_resize(st->backend->event_queue, w, h, 0);

	return 0;
}

static void wayland_exit(gp_backend* self)
{
	(void) self;

	/* Free keymap */
	xkb_keymap_unref(state.keymap);
	xkb_context_unref(state.keymap_context);
	xkb_state_unref(state.keymap_state);

	window_destroy(&state);
	display_disconnect(&state);
}

static struct gp_backend backend = {
	.name = "Wayland",
	.update = wayland_update,
	.update_rect = wayland_update_rect,
	.set_attr = wayland_set_attr,
	.render_stopped = wayland_render_stopped,
	.exit = wayland_exit,
};

gp_backend *gp_wayland_init(const char *display,
                            gp_size w, gp_size h, const char *caption)
{
	/* ensure client_state is initialized */
	memset(&state, 0, sizeof(struct client_state));

	/* first step: connect to the display, give up if this fails */
	if (display_connect(display, &state))
		return NULL;

	int fd = wl_display_get_fd(state.display);

	state.fd = (gp_fd) {
		.fd = fd,
		.event = wayland_process_fd,
		.events = GP_POLLIN,
		.priv = &backend,
	};

	if (gp_poll_add(&backend.fds, &state.fd)) {
		display_disconnect(&state);
		return NULL;
	}

	if (window_create(&state, w, h, caption)) {
		gp_poll_rem(&backend.fds, &state.fd);
		display_disconnect(&state);
		return NULL;
	}

	state.current_frame = get_next_free_frame(&state);
	assert(state.current_frame);

	state.backend = &backend;

	backend.pixmap = gp_pixmap_alloc(w, h, state.pixel_type);

	backend.pixmap->pixels = (void*) state.current_frame->data;
	assert(backend.pixmap->pixels);

	backend.event_queue = &state.ev_queue;

	if (state.dw_mm && state.dh_mm) {
		backend.dpi = gp_dpi_from_size(state.dw, state.dw_mm,
		                               state.dh, state.dh_mm);
	} else {
		backend.dpi = 0;

		GP_DEBUG(1, "Output size and DPI is not known");
	}

	gp_ev_queue_init(backend.event_queue, w, h, 0, NULL, NULL, 0);

	gp_ev_queue_push_pixel_type(backend.event_queue, state.pixel_type, 0);

	return &backend;
}

#else

gp_backend *gp_wayland_init(const char *display,
                            gp_size w, gp_size h, const char *caption)
{
	(void) display;
	(void) w;
	(void) h;
	(void) caption;

	GP_FATAL("Wayland support not compiled in!");

	return NULL;
}

#endif /* HAVE_WAYLAND */
