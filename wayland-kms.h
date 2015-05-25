/*
 * Copyright 息 2013 Renesas Solutions Corp.
 *
 * Based on src/egl/wayland/wayland-drm/wayland-drm.h in Mesa.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Takanari Hayama <taki@igel.co.jp>
 */

#ifndef WAYLAND_KMS_H
#define WAYLAND_KMS_H

#include <wayland-server.h>

struct wl_kms;

#define MAX_PLANES 3

struct wl_kms_planes {
	int fd;
	uint32_t stride;
	uint32_t handle;
};

struct wl_kms_buffer {
	struct wl_resource *resource;
	struct wl_kms *kms;
	int32_t width, height;
	uint32_t stride, format;
	uint32_t handle;
	int fd;
	void *private;

	// for multi-planer formats
	int num_planes;
	struct wl_kms_planes planes[MAX_PLANES];
};

int wayland_kms_fd_get(struct wl_kms *kms);

struct wl_kms_buffer *wayland_kms_buffer_get(struct wl_resource *resource);

struct wl_kms *wayland_kms_init(struct wl_display *display,
			        struct wl_display *server, char *device_name, int fd);

void wayland_kms_uninit(struct wl_kms *kms);

uint32_t wayland_kms_buffer_get_format(struct wl_kms_buffer *buffer);

void *wayland_kms_buffer_get_buffer(struct wl_kms_buffer *buffer);

enum wl_kms_attribute {
	WL_KMS_WIDTH,
	WL_KMS_HEIGHT,
	WL_KMS_TEXTURE_FORMAT
};

int wayland_kms_query_buffer(struct wl_kms *kms, struct wl_resource *resource,
				enum wl_kms_attribute attr, int *value);

#endif
