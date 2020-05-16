//SPDX-License-Identifier: LGPL-2.0-or-later
/*

   Copyright (c) 2019-2020 Cyril Hrubis <metan@ucw.cz>

 */

#ifndef GP_PROXY_GP_PROXY_H__
#define GP_PROXY_GP_PROXY_H__

#include <core/gp_pixmap.h>
#include <core/gp_pixel.h>
#include <input/gp_event.h>
#include <stdint.h>

struct gp_proxy_event {
	uint32_t type;
	uint32_t size;
	gp_event ev;
};

struct gp_proxy_path {
	size_t size;
	char path[64];
};

struct gp_proxy_rect_ {
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
};

struct gp_proxy_map {
	uint32_t type;
	uint32_t size;
	struct gp_proxy_path map;
};

struct gp_proxy_pixmap {
	uint32_t type;
	uint32_t size;
	gp_pixmap pix;
};

struct gp_proxy_rect {
	uint32_t type;
	uint32_t size;
	struct gp_proxy_rect_ rect;
};

struct gp_proxy_ptype {
	uint32_t type;
	uint32_t size;
	gp_pixel_type ptype;
};

union gp_proxy_msg {
	struct {
		uint32_t type;
		uint32_t size;
		char payload[];
	};
	struct gp_proxy_event ev;
	struct gp_proxy_map map;
	struct gp_proxy_pixmap pix;
	struct gp_proxy_rect rect;
	struct gp_proxy_ptype ptype;
};

enum gp_proxy_msg_types {
	GP_PROXY_NAME,
	GP_PROXY_EXIT,
	/* Payload type is pixel_type */
	GP_PROXY_PIXEL_TYPE,
	/* Payload type is gp_event */
	GP_PROXY_EVENT,
	/* Asks client to map a shared buffer */
	GP_PROXY_MAP,
	/* Asks client to unmap a shared buffer */
	GP_PROXY_UNMAP,
	/* Asks client to create a pixmap from the shared buffer */
	GP_PROXY_PIXMAP,
	/* Application is on screen */
	GP_PROXY_SHOW,
	/* Application is hidden */
	GP_PROXY_HIDE,
	/* Update rect on screen */
	GP_PROXY_UPDATE,
	GP_PROXY_MAX,
};

/* Must be bigger than maximal message size! */
#define GP_PROXY_BUF_SIZE 128

struct gp_proxy_buf {
	size_t pos;
	size_t size;
	char buf[GP_PROXY_BUF_SIZE];
};

static inline void gp_proxy_buf_init(struct gp_proxy_buf *buf)
{
	buf->pos = 0;
	buf->size = 0;
}

/*
 * Parses next message in the proxy buffer, the pointer to a start of the
 * message is stored into the msg pointer.
 *
 * @buf Proxy bufer filled by the gp_proxy_buf_recv() function.
 * @msg Pointer to the start of the next message.
 * @return Non-zero if full message was found in the buffer, zero otherwise.
 */
int gp_proxy_next(struct gp_proxy_buf *buf, union gp_proxy_msg **msg);

/*
 * Receives data from from fd and stores them to the proxy buffer.
 *
 * @fd File descriptor connected to the server/client.
 * @buf Proxy buffer to store data to.
 * @return Number of bytes stored into the buffer, -1 on failure.
 */
int gp_proxy_buf_recv(int fd, struct gp_proxy_buf *buf);

/*
 * Sends data to the server/client.
 *
 * @fd File descriptor connected to the server/client.
 * @type Type of the message.
 * @payload Pointer to message payload accordingly to the message type.
 * @return Zero on success, non-zero on failure.
 */
int gp_proxy_send(int fd, enum gp_proxy_msg_types type, void *payload);

#endif /* GP_PROXY_GP_PROXY_H__ */
