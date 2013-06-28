/*****************************************************************************
 * This file is part of gfxprim library.                                     *
 *                                                                           *
 * Gfxprim is free software; you can redistribute it and/or                  *
 * modify it under the terms of the GNU Lesser General Public                *
 * License as published by the Free Software Foundation; either              *
 * version 2.1 of the License, or (at your option) any later version.        *
 *                                                                           *
 * Gfxprim is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU         *
 * Lesser General Public License for more details.                           *
 *                                                                           *
 * You should have received a copy of the GNU Lesser General Public          *
 * License along with gfxprim; if not, write to the Free Software            *
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,                        *
 * Boston, MA  02110-1301  USA                                               *
 *                                                                           *
 * Copyright (C) 2009-2013 Cyril Hrubis <metan@ucw.cz>                       *
 *                                                                           *
 *****************************************************************************/

 /*

   SPIV -- Simple yet Powerful Image Viewer.

  */

#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>

#include <GP.h>
#include <input/GP_InputDriverLinux.h>

#include "image_cache.h"
#include "image_list.h"
#include "image_actions.h"
#include "spiv_help.h"
#include "cpu_timer.h"

static GP_Pixel black_pixel;
static GP_Pixel white_pixel;
static GP_Pixel gray_pixel;

static GP_Backend *backend = NULL;

/* image loader thread */
static int abort_flag = 0;
static int show_progress = 0;

enum zoom_type {
	/*
	 * Resize image to fit current size of the window.
	 */
	ZOOM_FIT,

	/*
	 * Use zoom set in zoom float and zoom offsets.
	 */
	ZOOM_FIXED,

	/*
	 * Fixed zoom but spiv tries to change
	 * the window size to fit the image size
	 */
	ZOOM_FIXED_WIN,

	/*
	 * Do not upscale images but downscale them
	 * if they are too big.
	 */
	ZOOM_FIT_DOWNSCALE,
};

struct loader_params {
	/* current image path */
	const char *img_path;
	/* current resize ratio */
	float rat;

	/* show loader progress */
	long show_progress:1;
	long show_progress_once:2;
	/* show image info in topleft corner */
	long show_info:3;
	/* use nearest neighbour resampling first */
	long show_nn_first:4;
	/* use dithering when blitting to display */
	long use_dithering:5;
	/* use low pass before resampling */
	long use_low_pass:6;
	/* image orientation 0, 90, 180, 270 */
	int rotate;
	/* resampling method */
	int resampling_method;

	/* offset in pixels */
	unsigned int zoom_x_offset;
	unsigned int zoom_y_offset;

	/* zoom */
	enum zoom_type zoom_type;
	float zoom;

	/* caches for loaded images */
	struct image_cache *img_resized_cache;
	struct image_cache *img_orig_cache;

	/* image list structure */
	struct image_list *img_list;
};

static int image_loader_callback(GP_ProgressCallback *self)
{
	static GP_Size size = 0;
	GP_Context *c = backend->context;

	if (abort_flag)
		return 1;

	if (!show_progress)
		return 0;

	char buf[100];

	snprintf(buf, sizeof(buf), "%s ... %-3.1f%%",
	         (const char*)self->priv, self->percentage);

	int align = GP_ALIGN_CENTER|GP_VALIGN_ABOVE;

	size = GP_TextWidth(NULL, buf);

	int start = c->w/2 - size/2 - 10;
	int end   = c->w/2 + size/2 + 10;
	int middle = start + (end - start) * self->percentage / 100;
	int top = c->h - GP_TextHeight(NULL) - 11;

	GP_FillRectXYXY(c, start, c->h - 1, middle, top, gray_pixel);
	GP_FillRectXYXY(c, middle, c->h - 1, end, top, black_pixel);

	GP_Text(c, NULL, c->w/2, c->h - 5, align,
	        white_pixel, black_pixel, buf);

	GP_BackendUpdateRect(backend, start, c->h - 1, end, top);

	return 0;
}

static GP_Context *load_image(struct loader_params *params, int elevate);

/*
 * Ask backend to resize window may not be implemented or authorized. If
 * backend (window) is resized we will get SYS_RESIZE event, see the main event
 * loop.
 */
static void resize_backend(struct loader_params *params,
                           float ratio, int shift_flag)
{
	GP_Context *img = load_image(params, 1);

	if (!shift_flag)
		ratio = 1.00 / ratio;

	unsigned int w = img->w * ratio + 0.5;
	unsigned int h = img->h * ratio + 0.5;

	GP_BackendResize(backend, w, h);
}


static float calc_img_size(struct loader_params *params,
                           uint32_t img_w, uint32_t img_h,
                           uint32_t src_w, uint32_t src_h)
{
	float w_rat;
	float h_rat;

	switch (params->zoom_type) {
	case ZOOM_FIT_DOWNSCALE:
		if (img_w <= src_w && img_h <= src_h)
			return 1.00;
	case ZOOM_FIT:
		w_rat = 1.00 * src_w / img_w;
		h_rat = 1.00 * src_h / img_h;
		return GP_MIN(w_rat, h_rat);
	case ZOOM_FIXED:
		return params->zoom;
	case ZOOM_FIXED_WIN:
		resize_backend(params, params->zoom, 0);
		return params->zoom;
	}

	return 1.00;
}

static const char *img_name(const char *img_path)
{
	int i, len = strlen(img_path);

	for (i = len - 1; i > 0; i--) {
		if (img_path[i] == '/')
			return &img_path[i+1];
	}

	return img_path;
}

static void set_caption(const char *path, float rat)
{
	char buf[256];

	snprintf(buf, sizeof(buf), "Spiv ~ %s 1:%3.3f", img_name(path), rat);

	GP_BackendSetCaption(backend, buf);
}

/*
 * Loads image
 */
static GP_Context *load_image(struct loader_params *params, int elevate)
{
	struct cpu_timer timer;
	GP_Context *img, *context = backend->context;

	GP_ProgressCallback callback = {.callback = image_loader_callback,
	                                .priv = "Loading image"};

	img = image_cache_get(params->img_orig_cache, params->img_path, 0, 0, elevate);

	/* Image not cached, load it */
	if (img == NULL) {
		fprintf(stderr, "Loading '%s'\n", params->img_path);

		cpu_timer_start(&timer, "Loading");
		if ((img = GP_LoadImage(params->img_path, &callback)) == NULL) {

			if (errno == ECANCELED)
				return NULL;

			GP_Fill(context, black_pixel);
			GP_Print(context, NULL, context->w/2, context->h/2 - 10,
			         GP_ALIGN_CENTER|GP_VALIGN_CENTER, white_pixel, black_pixel,
				 "'%s'", params->img_path);
			GP_Print(context, NULL, context->w/2, context->h/2 + 10,
				 GP_ALIGN_CENTER|GP_VALIGN_CENTER, white_pixel, black_pixel,
				 "Failed to load image :( (%s)", strerror(errno));
			GP_BackendFlip(backend);
			return NULL;
		}

		/* Workaround */
//		if (img->pixel_type != GP_PIXEL_RGB888) {
//			GP_Context *tmp;
//			tmp = GP_ContextConvertAlloc(img, GP_PIXEL_RGB888);
//			GP_ContextFree(img);
//			img = tmp;
//		}

		image_cache_put(params->img_orig_cache, img, params->img_path, 0, 0);

		cpu_timer_stop(&timer);
	}

	return img;
}

/*
 * Fill context with chessboard-like pattern.
 */
static void pattern_fill(GP_Context *ctx, unsigned int x0, unsigned int y0, unsigned int w, unsigned int h)
{
	unsigned int x, y;

	GP_Pixel g1 = GP_RGBToContextPixel(0x64, 0x64, 0x64, ctx);
	GP_Pixel g2 = GP_RGBToContextPixel(0x80, 0x80, 0x80, ctx);

	unsigned int wm = w/10 < 10 ? 10 : w/10;
	unsigned int hm = h/10 < 10 ? 10 : h/10;
	unsigned int wt = w/20 < 5  ?  5 : w/20;
	unsigned int ht = h/20 < 5  ?  5 : h/20;

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			GP_Pixel pix;

			if ((x % wm < wt) ^ (y % hm < ht))
				pix = g1;
			else
				pix = g2;

			GP_PutPixel(ctx, x0 + x, y0 + y, pix);
		}
	}
}

/*
 * Updates display.
 */
static void update_display(struct loader_params *params, GP_Context *img)
{
	GP_Context *context = backend->context;
	struct cpu_timer timer;
	GP_ProgressCallback callback = {.callback = image_loader_callback};

	switch (params->rotate) {
	case 0:
	break;
	case 90:
		callback.priv = "Rotating image (90)";
		img = GP_FilterRotate90Alloc(img, &callback);
	break;
	case 180:
		callback.priv = "Rotating image (180)";
		img = GP_FilterRotate180Alloc(img, &callback);
	break;
	case 270:
		callback.priv = "Rotating image (270)";
		img = GP_FilterRotate270Alloc(img, &callback);
	break;
	}

	if (img == NULL)
		return;

	int cx = 0;
	int cy = 0;

	switch (params->zoom_type) {
	case ZOOM_FIT_DOWNSCALE:
	case ZOOM_FIT:
		cx = (context->w - img->w)/2;
		cy = (context->h - img->h)/2;
	break;
	case ZOOM_FIXED:
	case ZOOM_FIXED_WIN:
		cx = params->zoom_x_offset;
		cy = params->zoom_y_offset;
	break;
	}

	GP_Context sub_display;

	cpu_timer_start(&timer, "Blitting");

	if (params->use_dithering) {
		callback.priv = "Dithering";
		GP_SubContext(context, &sub_display, cx, cy, img->w, img->h);
		GP_FilterFloydSteinberg_RGB888(img, &sub_display, NULL);
	//	GP_FilterHilbertPeano_RGB888(img, &sub_display, NULL);
	} else {
		if (GP_PixelHasFlags(img->pixel_type, GP_PIXEL_HAS_ALPHA))
			pattern_fill(context, cx, cy, img->w, img->h);

		GP_Blit_Clipped(img, 0, 0, img->w, img->h, context, cx, cy);
	}

	cpu_timer_stop(&timer);

	if (params->rotate)
		GP_ContextFree(img);

	/* clean up the rest of the display */
	GP_FillRectXYWH(context, 0, 0, cx, context->h, black_pixel);
	GP_FillRectXYWH(context, 0, 0, context->w, cy, black_pixel);

	int w = context->w - img->w - cx;

	if (w > 0)
		GP_FillRectXYWH(context, img->w + cx, 0, w, context->h, black_pixel);

	int h = context->h - img->h - cy;

	if (h > 0)
		GP_FillRectXYWH(context, 0, img->h + cy, context->w, h, black_pixel);

	set_caption(params->img_path, params->rat);

	if (!params->show_info) {
		GP_BackendFlip(backend);
		return;
	}

	GP_Size th = GP_TextHeight(NULL);

	GP_Print(context, NULL, 11, 11, GP_ALIGN_RIGHT|GP_VALIGN_BOTTOM,
	         black_pixel, white_pixel, "%ux%u", img->w, img->h);

	GP_Print(context, NULL, 10, 10, GP_ALIGN_RIGHT|GP_VALIGN_BOTTOM,
	         white_pixel, black_pixel, "%ux%u", img->w, img->h);

	GP_Print(context, NULL, 11, 13 + th, GP_ALIGN_RIGHT|GP_VALIGN_BOTTOM,
	         black_pixel, white_pixel, "1:%3.3f", params->rat);

	GP_Print(context, NULL, 10, 12 + th, GP_ALIGN_RIGHT|GP_VALIGN_BOTTOM,
	         white_pixel, black_pixel, "1:%3.3f", params->rat);

	GP_Print(context, NULL, 11, 15 + 2 * th, GP_ALIGN_RIGHT|GP_VALIGN_BOTTOM,
	         black_pixel, white_pixel, "%s", img_name(params->img_path));

	GP_Print(context, NULL, 10, 14 + 2 * th, GP_ALIGN_RIGHT|GP_VALIGN_BOTTOM,
	         white_pixel, black_pixel, "%s", img_name(params->img_path));

	GP_Print(context, NULL, 11, 17 + 3 * th, GP_ALIGN_RIGHT|GP_VALIGN_BOTTOM,
	         black_pixel, white_pixel, "%s%s",
		 params->use_low_pass && params->rat < 1 ? "Gaussian LP + " : "",
		 GP_InterpolationTypeName(params->resampling_method));

	GP_Print(context, NULL, 10, 16 + 3 * th, GP_ALIGN_RIGHT|GP_VALIGN_BOTTOM,
	         white_pixel, black_pixel, "%s%s",
		 params->use_low_pass && params->rat < 1 ? "Gaussian LP + " : "",
		 GP_InterpolationTypeName(params->resampling_method));

	unsigned int count = image_list_count(params->img_list);
	unsigned int pos = image_list_pos(params->img_list) + 1;

	GP_Print(context, NULL, 11, 19 + 4 * th, GP_ALIGN_RIGHT|GP_VALIGN_BOTTOM,
	         black_pixel, white_pixel, "%u of %u", pos, count);

	GP_Print(context, NULL, 10, 18 + 4 * th, GP_ALIGN_RIGHT|GP_VALIGN_BOTTOM,
	         white_pixel, black_pixel, "%u of %u", pos, count);

	unsigned int dir_count = image_list_dir_count(params->img_list);
	unsigned int dir_pos = image_list_dir_pos(params->img_list) + 1;

	if (dir_count > 1 && dir_count != count) {
		GP_Print(context, NULL, 11, 21 + 5 * th, GP_ALIGN_RIGHT|GP_VALIGN_BOTTOM,
			 black_pixel, white_pixel, "%u of %u in directory", dir_pos, dir_count);

		GP_Print(context, NULL, 10, 20 + 5 * th, GP_ALIGN_RIGHT|GP_VALIGN_BOTTOM,
			 white_pixel, black_pixel, "%u of %u in directory", dir_pos, dir_count);
	}

	GP_BackendFlip(backend);
}

GP_Context *load_resized_image(struct loader_params *params, GP_Size w, GP_Size h)
{
	long cookie = (w & 0xffff) | (h & 0xffff)<<16;
	GP_Context *img, *res = NULL;
	struct cpu_timer timer;
	GP_ProgressCallback callback = {.callback = image_loader_callback};

	int key = (params->resampling_method<<1) | !!(params->use_low_pass);

	/* Try to get resized cached image */
	img = image_cache_get(params->img_resized_cache, params->img_path, cookie, key, 1);

	if (img != NULL)
		return img;

	/* Otherwise load image and resize it */
	if ((img = load_image(params, 1)) == NULL)
		return NULL;

	if (params->show_nn_first) {
		/* Do simple interpolation and blit the result */
		GP_Context *nn = GP_FilterResizeNNAlloc(img, w, h, NULL);
		if (nn != NULL) {
			update_display(params, nn);
			GP_ContextFree(nn);
		}
	}

	/* Do low pass filter */
	if (params->use_low_pass && params->rat < 1) {
		cpu_timer_start(&timer, "Blur");
		callback.priv = "Blurring Image";

		res = GP_FilterGaussianBlurAlloc(img, 0.3/params->rat,
		                                 0.3/params->rat, &callback);

		if (res == NULL)
			return NULL;

		img = res;

		cpu_timer_stop(&timer);
	}

//	img->gamma = GP_GammaAcquire(img->pixel_type, 0.45);

	cpu_timer_start(&timer, "Resampling");
	callback.priv = "Resampling Image";
	GP_Context *i1 = GP_FilterResize(img, NULL, params->resampling_method, w, h, &callback);
//	img->gamma = NULL;
//	GP_Context *i2 = GP_FilterResize(img, NULL, params->resampling_method, w, h, &callback);
//	img = GP_FilterDifferenceAlloc(i2, i1, NULL);
//	img = GP_FilterInvert(img, NULL, NULL);
	img = i1;
//	if (params->resampling_method == GP_INTERP_CUBIC_INT)
//		GP_FilterEdgeSharpening(img, img, 0.2, NULL);
	cpu_timer_stop(&timer);

/*
	if (params->rat > 1.5) {
		cpu_timer_start(&timer, "Sharpening");
		callback.priv = "Sharpening";
		GP_FilterEdgeSharpening(i1, i1, 0.1, &callback);
		cpu_timer_stop(&timer);
	}
*/

	/* Free low passed context if needed */
	GP_ContextFree(res);

	if (img == NULL)
		return NULL;

	image_cache_put(params->img_resized_cache, img, params->img_path, cookie, key);

	return img;
}

static void *image_loader(void *ptr)
{
	struct loader_params *params = ptr;
	struct cpu_timer sum_timer;
	GP_Context *img, *context = backend->context;

	cpu_timer_start(&sum_timer, "sum");

	show_progress = params->show_progress || params->show_progress_once;
	params->show_progress_once = 0;

	/* Figure out rotation */
	GP_Size w, h;

	switch (params->rotate) {
	case 0:
	case 180:
	default:
		w = context->w;
		h = context->h;
	break;
	case 90:
	case 270:
		w = context->h;
		h = context->w;
	break;
	}

	if ((img = load_image(params, 0)) == NULL)
		return NULL;

	params->rat = calc_img_size(params, img->w, img->h, w, h);

	/* Special case => no need to resize */
	if (params->rat == 1.0)
		goto update;

	w = img->w;
	h = img->h;

	img = load_resized_image(params, w * params->rat + 0.5, h * params->rat + 0.5);

	if (img == NULL)
		return NULL;

	image_cache_print(params->img_resized_cache);
	image_cache_print(params->img_orig_cache);

update:
	update_display(params, img);
	cpu_timer_stop(&sum_timer);

	return NULL;
}

static pthread_t loader_thread = (pthread_t)0;

static void stop_loader(void)
{
	if (loader_thread) {
		abort_flag = 1;
		pthread_join(loader_thread, NULL);
		loader_thread = (pthread_t)0;
		abort_flag = 0;
	}
}

static void show_image(struct loader_params *params, const char *path)
{
	int ret;

	if (path != NULL)
		params->img_path = path;

	/* stop previous loader thread */
	stop_loader();

	ret = pthread_create(&loader_thread, NULL, image_loader, (void*)params);

	if (ret) {
		fprintf(stderr, "Failed to start thread: %s\n", strerror(ret));
		GP_BackendExit(backend);
		exit(1);
	}
}

static void set_zoom_offset(struct loader_params *params, int dx, int dy)
{
	params->zoom_x_offset += dx;
	params->zoom_y_offset += dy;
	show_image(params, NULL);
}

static void zoom_mul(struct loader_params *params, float mul)
{
	params->zoom *= mul;
	show_image(params, NULL);
}

static void sighandler(int signo)
{
	if (backend != NULL)
		GP_BackendExit(backend);

	fprintf(stderr, "Got signal %i\n", signo);

	_exit(1);
}

static void init_backend(const char *backend_opts)
{
	backend = GP_BackendInit(backend_opts, "Spiv", stderr);

	if (backend == NULL) {
		fprintf(stderr, "Failed to initalize backend '%s'\n", backend_opts);
		exit(1);
	}
}


static void init_caches(struct loader_params *params)
{
	size_t size = image_cache_get_ram_size();
	unsigned int resized_size = (1024 * size)/10;
	unsigned int orig_size = (1024 * size)/50;

	if (resized_size > 100 * 1024 * 1024)
		resized_size = 100 * 1024 * 1024;

	if (orig_size > 40 * 1024 * 1024)
		orig_size = 40 * 1024 * 1024;

	GP_DEBUG(1, "Cache sizes original = %u, resized = %u",
	         orig_size, resized_size);

	params->img_resized_cache = image_cache_create(resized_size);
	params->img_orig_cache = image_cache_create(orig_size);

//	params->img_resized_cache = NULL;
//	params->img_orig_cache = NULL;
}

static uint32_t timer_callback(GP_Timer *self)
{
	struct loader_params *params = self->priv;

	show_image(params, image_list_move(params->img_list, 1));

	return 0;
}

int main(int argc, char *argv[])
{
	GP_Context *context = NULL;
	const char *backend_opts = "X11";
	int sleep_ms = 0;
	int opt;
	int shift_flag;
	GP_PixelType emul_type = GP_PIXEL_UNKNOWN;

	struct loader_params params = {
		.img_path = NULL,

		.show_progress = 0,
		.show_progress_once = 0,
		.show_info = 0,
		.show_nn_first = 0,
		.use_dithering = 0,
		.rotate = 0,
		.resampling_method = GP_INTERP_LINEAR_LF_INT,

                .zoom_type = ZOOM_FIT_DOWNSCALE,
                .zoom = 1,

		.img_resized_cache = NULL,
		.img_orig_cache = NULL,
	};

	GP_TIMER_DECLARE(timer, 0, 0, "Slideshow", timer_callback, &params);

	while ((opt = getopt(argc, argv, "b:ce:fhIPs:r:z:0:1:2:3:4:5:6:7:8:9:")) != -1) {
		switch (opt) {
		case 'I':
			params.show_info = 1;
		break;
		case 'P':
			params.show_progress = 1;
		break;
		case 'f':
			params.use_dithering = 1;
		break;
		case 's':
			sleep_ms = atoi(optarg);
		break;
		case 'c':
			params.resampling_method = GP_INTERP_CUBIC_INT;
			/* Cubic resampling needs low pass */
			params.use_low_pass = 1;
			/* Cubic resampling is slow, show nn first */
			params.show_nn_first = 1;
		break;
		case 'e':
			emul_type = GP_PixelTypeByName(optarg);

			if (emul_type == GP_PIXEL_UNKNOWN) {
				fprintf(stderr, "Invalid pixel type '%s'\n", optarg);
				return 1;
			}
		break;
		case 'r':
			if (!strcmp(optarg, "90"))
				params.rotate = 90;
			else if (!strcmp(optarg, "180"))
				params.rotate = 180;
			else if (!strcmp(optarg, "270"))
				params.rotate = 270;
		case 'b':
			backend_opts = optarg;
		break;
		case 'h':
			print_help();
			exit(0);
		break;
		case 'z':
			switch (optarg[0]) {
			case 'f':
				params.zoom_type = ZOOM_FIXED;
			break;
			case 'w':
				params.zoom_type = ZOOM_FIXED_WIN;
			break;
			}
		break;
		case '0' ... '9':
			image_action_set(opt - '0', optarg);
		break;
		default:
			fprintf(stderr, "Invalid paramter '%c'\n", opt);
			print_help();
			return 1;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "Requires path to at least one image\n\n");
		print_help();
		return 1;
	}

	signal(SIGINT, sighandler);
	signal(SIGSEGV, sighandler);
	signal(SIGBUS, sighandler);
	signal(SIGABRT, sighandler);

	init_caches(&params);

	init_backend(backend_opts);

	if (emul_type != GP_PIXEL_UNKNOWN)
		backend = GP_BackendVirtualInit(backend, emul_type, GP_BACKEND_CALL_EXIT);

	context = backend->context;

	black_pixel = GP_ColorToContextPixel(GP_COL_BLACK, context);
	white_pixel = GP_ColorToContextPixel(GP_COL_WHITE, context);
	gray_pixel = GP_RGBToContextPixel(0x33, 0x33, 0x33, context);

	GP_Fill(context, black_pixel);
	GP_BackendFlip(backend);

	struct image_list *list = image_list_create((const char**)argv + optind);
	params.img_list = list;

	params.show_progress_once = 1;
	show_image(&params, image_list_img_path(list));

	if (sleep_ms) {
		timer.period = sleep_ms;
		GP_BackendAddTimer(backend, &timer);
	}

	for (;;) {
		GP_BackendWait(backend);

		GP_Event ev;

		while (GP_BackendGetEvent(backend, &ev)) {

			shift_flag = GP_EventGetKey(&ev, GP_KEY_LEFT_SHIFT) ||
			             GP_EventGetKey(&ev, GP_KEY_RIGHT_SHIFT);

			switch (ev.type) {
			case GP_EV_REL:
				switch (ev.code) {
				case GP_EV_REL_WHEEL:
					if (ev.val.val > 0)
						goto next;

					if (ev.val.val < 0)
						goto prev;
				break;
				case GP_EV_REL_POS:
					if (GP_EventGetKey(&ev, GP_BTN_LEFT))
						set_zoom_offset(&params,
						                ev.val.rel.rx,
								ev.val.rel.ry);
				break;
				}
			break;
			case GP_EV_KEY:
				if (ev.code != GP_EV_KEY_DOWN)
					continue;

				switch (ev.val.key.key) {
				case GP_KEY_H:
					draw_help(backend);
					show_image(&params, NULL);
				break;
				case GP_KEY_F:
					if (GP_BackendIsX11(backend))
						GP_BackendX11RequestFullscreen(backend, 2);
				break;
				case GP_KEY_I:
					params.show_info = !params.show_info;

					params.show_progress_once = 1;
					show_image(&params, NULL);
				break;
				case GP_KEY_P:
					params.show_progress = !params.show_progress;
				break;
				case GP_KEY_R:
					params.rotate += 90;
					if (params.rotate > 270)
						params.rotate = 0;

					params.show_progress_once = 1;
					show_image(&params, NULL);
				break;
				case GP_KEY_RIGHT_BRACE:
					params.resampling_method++;

					if (params.resampling_method > GP_INTERP_MAX)
						params.resampling_method = 0;

					if (params.resampling_method == GP_INTERP_LINEAR_LF_INT) {
						params.use_low_pass = 0;
						params.show_nn_first = 0;
					} else {
						params.use_low_pass = 1;
						params.show_nn_first = 1;
					}

					params.show_progress_once = 1;
					show_image(&params, image_list_img_path(list));
				break;
				case GP_KEY_LEFT_BRACE:
					if (params.resampling_method == 0)
						params.resampling_method = GP_INTERP_MAX;
					else
						params.resampling_method--;

					if (params.resampling_method == GP_INTERP_LINEAR_LF_INT) {
						params.use_low_pass = 0;
						params.show_nn_first = 0;
					} else {
						params.use_low_pass = 1;
						params.show_nn_first = 1;
					}

					params.show_progress_once = 1;
					show_image(&params, image_list_img_path(list));
				break;
				case GP_KEY_L:
					params.use_low_pass = !params.use_low_pass;

					params.show_progress_once = 1;
					show_image(&params, image_list_img_path(list));
				break;
				case GP_KEY_D:
					image_cache_drop(params.img_resized_cache);
					image_cache_drop(params.img_orig_cache);
				break;
				case GP_KEY_ESC:
				case GP_KEY_ENTER:
				case GP_KEY_Q:
					image_cache_drop(params.img_resized_cache);
					image_cache_drop(params.img_orig_cache);
					GP_BackendExit(backend);
					return 0;
				break;
				case GP_KEY_PAGE_UP:
					params.show_progress_once = 1;
					show_image(&params, image_list_dir_move(list, -1));
				break;
				case GP_KEY_PAGE_DOWN:
					params.show_progress_once = 1;
					show_image(&params, image_list_dir_move(list, 1));
				break;
				case GP_KEY_HOME:
					params.show_progress_once = 1;
					show_image(&params, image_list_first(list));
				break;
				case GP_KEY_END:
					params.show_progress_once = 1;
					show_image(&params, image_list_last(list));
				break;
				case GP_KEY_RIGHT:
					if (shift_flag)
						set_zoom_offset(&params, 1, 0);
					else
						set_zoom_offset(&params, 10, 0);

				break;
				case GP_KEY_LEFT:
					if (shift_flag)
						set_zoom_offset(&params, -1, 0);
					else
						set_zoom_offset(&params, -10, 0);
				break;
				case GP_KEY_UP:
					if (shift_flag)
						set_zoom_offset(&params, 0, -1);
					else
						set_zoom_offset(&params, 0, -10);
				break;
				case GP_KEY_DOWN:
					if (shift_flag)
						set_zoom_offset(&params, 0, 1);
					else
						set_zoom_offset(&params, 0, 10);
				break;
				next:
				case GP_KEY_SPACE:
					params.show_progress_once = 1;
					if (shift_flag)
						show_image(&params, image_list_move(list, 10));
					else
						show_image(&params, image_list_move(list, 1));
				break;
				prev:
				case GP_KEY_BACKSPACE:
					params.show_progress_once = 1;
					if (shift_flag)
						show_image(&params, image_list_move(list, -10));
					else
						show_image(&params, image_list_move(list, -1));
				break;
				case GP_KEY_1:
					resize_backend(&params, 1, shift_flag);
				break;
				case GP_KEY_2:
					resize_backend(&params, 2, shift_flag);
				break;
				case GP_KEY_3:
					resize_backend(&params, 3, shift_flag);
				break;
				case GP_KEY_4:
					resize_backend(&params, 4, shift_flag);
				break;
				case GP_KEY_5:
					resize_backend(&params, 5, shift_flag);
				break;
				case GP_KEY_6:
					resize_backend(&params, 6, shift_flag);
				break;
				case GP_KEY_7:
					resize_backend(&params, 7, shift_flag);
				break;
				case GP_KEY_8:
					resize_backend(&params, 8, shift_flag);
				break;
				case GP_KEY_9:
					resize_backend(&params, 9, shift_flag);
				break;
				case GP_KEY_KP_PLUS:
				case GP_KEY_DOT:
					params.show_progress_once = 1;
					zoom_mul(&params, 1.5);
				break;
				case GP_KEY_KP_MINUS:
				case GP_KEY_COMMA:
					params.show_progress_once = 1;
					zoom_mul(&params, 1/1.5);
				break;
				case GP_KEY_F1 ... GP_KEY_F10:
					image_action_run(ev.val.key.key - GP_KEY_F1,
					                 image_list_img_path(list));
				break;
				}
			break;
			case GP_EV_SYS:
				switch (ev.code) {
				case GP_EV_SYS_RESIZE:
					/* stop loader thread before resizing backend buffer */
					stop_loader();
					GP_BackendResizeAck(backend);
					GP_Fill(backend->context, 0);
					params.show_progress_once = 1;
					show_image(&params, NULL);
				break;
				case GP_EV_SYS_QUIT:
					GP_BackendExit(backend);
					return 0;
				break;
				}
			break;
			}
		}
	}

	GP_BackendExit(backend);

	return 0;
}
