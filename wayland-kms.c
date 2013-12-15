/*
 * Copyright © 2013 Renesas Solutions Corp.
 * Copyright © 2011 Kristian Høgsberg
 * Copyright © 2011 Benjamin Franzke
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
 *
 * Based on wayland-drm.c by the following authors:
 *    Kristian Høgsberg <krh@bitplanet.net>
 *    Benjamin Franzke <benjaminfranzke@googlemail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <errno.h>

#include <xf86drm.h>
#include <wayland-server.h>
#include "wayland-kms.h"
#include "wayland-kms-auth.h"
#include "wayland-kms-server-protocol.h"

#if defined(DEBUG)
#	define WLKMS_DEBUG(s, x...) { printf(s, ##x); }
#else
#	define WLKMS_DEBUG(s, x...) { }
#endif

/*
 * Taken from EGL/egl.h. Better to refer the egl.h
 * in the future.
 */
#ifndef EGL_TEXTURE_RGBA
#	define EGL_TEXTURE_RGBA		0x305E
#endif

struct wl_kms {
	struct wl_display *display;
	int fd;				/* FD for DRM */
	char *device_name;

	struct kms_auth *auth;		/* for nested authentication */
};

/*
 * wl_kms server
 */

static void destroy_buffer(struct wl_resource *resource)
{
	struct wl_kms_buffer *buffer = resource->data;
	free(buffer);
}

static void
buffer_destroy(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

const static struct wl_buffer_interface kms_buffer_interface = {
	.destroy = buffer_destroy
};

static void
kms_authenticate(struct wl_client *client, struct wl_resource *resource,
		 uint32_t magic)
{
	struct wl_kms *kms = resource->data;
	int err;

	WLKMS_DEBUG("%s: %s: magic=%lu\n", __FILE__, __func__, magic);

	if (kms->auth) {
		err = kms_auth_request(kms->auth, magic);
	} else {
		err = drmAuthMagic(kms->fd, magic);
	}

	if (err < 0) {
		wl_resource_post_error(resource, WL_KMS_ERROR_AUTHENTICATION_FAILED,
				       "authentication failed");
		WLKMS_DEBUG("%s: %s: authentication failed.\n", __FILE__, __func__);
	} else {
		wl_resource_post_event(resource, WL_KMS_AUTHENTICATED);
		WLKMS_DEBUG("%s: %s: authentication succeeded.\n", __FILE__, __func__);
	}
}

static void
kms_create_buffer(struct wl_client *client, struct wl_resource *resource,
		  uint32_t id, int32_t prime_fd, int32_t width, int32_t height,
		  uint32_t stride, uint32_t format, uint32_t handle)
{
	struct wl_kms *kms = resource->data;
	struct wl_kms_buffer *buffer;
	int err;

	switch (format) {
	case WL_KMS_FORMAT_ARGB8888:
	case WL_KMS_FORMAT_XRGB8888:
		break;
	default:
		wl_resource_post_error(resource,
				       WL_KMS_ERROR_INVALID_FORMAT,
				       "invalid format");
		return;
	}

	buffer = calloc(1, sizeof *buffer);
	if (buffer == NULL) {
		wl_resource_post_no_memory(resource);
		return;
	}

	buffer->kms = kms;
	buffer->width = width;
	buffer->height = height;
	buffer->format = format;
	buffer->stride = stride;
	buffer->fd = prime_fd;

	WLKMS_DEBUG("%s: %s: prime_fd=%d\n", __FILE__, __func__, prime_fd);
#if 0
	if ((err = drmPrimeFDToHandle(kms->fd, prime_fd, &buffer->handle))) {
		WLKMS_DEBUG("%s: %s: drmPrimeFDToHandle() failed...%d (%s)\n", __FILE__, __func__, err, strerror(errno));
		wl_resource_post_error(resource,
				       WL_KMS_ERROR_INVALID_FD,
				       "invalid prime FD");
		return;
	}
#else
	{
		struct drm_gem_open op;
		int ret;

		op.name   = prime_fd;
		op.handle = 0;

		ret = drmIoctl(kms->fd, DRM_IOCTL_GEM_OPEN, &op);
		if (ret) {
			WLKMS_DEBUG("%s: %s: DRM_IOCTL_GEM_OPEN failed...(%s)\n", __FILE__, __func__, strerror(errno));
			wl_resource_post_error(resource, WL_KMS_ERROR_INVALID_FD, "invalid prime FD");
			return;
		}
		buffer->handle = op.handle;
	}
#endif

	// We create a wl_buffer
	buffer->resource = wl_resource_create(client, &wl_buffer_interface, 1, id);
	if (!buffer->resource) {
		wl_resource_post_no_memory(resource);
		free(buffer);
		return;
	}

	wl_resource_set_implementation(buffer->resource,
				       (void (**)(void))&kms_buffer_interface,
				       buffer, destroy_buffer);
}

const static struct wl_kms_interface kms_interface = {
	.authenticate = kms_authenticate,
	.create_buffer = kms_create_buffer,
};

static void
bind_kms(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct wl_kms *kms = data;
	struct wl_resource *resource;
	uint32_t capabilities;

	resource = wl_resource_create(client, &wl_kms_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &kms_interface, data, NULL);

	wl_resource_post_event(resource, WL_KMS_DEVICE, kms->device_name);
	wl_resource_post_event(resource, WL_KMS_FORMAT, WL_KMS_FORMAT_ARGB8888);
	wl_resource_post_event(resource, WL_KMS_FORMAT, WL_KMS_FORMAT_XRGB8888);
}

struct wl_kms_buffer *wayland_kms_buffer_get(struct wl_resource *resource)
{
	if (resource == NULL)
		return NULL;

	if (wl_resource_instance_of(resource, &wl_buffer_interface,
				    &kms_buffer_interface))
		return wl_resource_get_user_data(resource);
	else
		return NULL;
}

struct wl_kms *wayland_kms_init(struct wl_display *display,
				struct wl_display *server, char *device_name, int fd)
{
	struct wl_kms *kms;

	if (!(kms = calloc(1, sizeof(struct wl_kms))))
		return NULL;

	kms->display = display;
	kms->device_name = strdup(device_name);
	kms->fd = fd;

	wl_global_create(display, &wl_kms_interface, 1, kms, bind_kms);

	/*
	 * we're the server in the middle. we should forward the auth
	 * request to our server.
	 */
	if (server) {
		if (!(kms->auth = kms_auth_init(server))) {
			free(kms);
			kms = NULL;
		}
	}

	return kms;
}

void wayland_kms_uninit(struct wl_kms *kms)
{
	free(kms->device_name);

	/* FIXME: need wl_display_del_{object,global} */

	free(kms);
}

uint32_t wayland_kms_buffer_get_format(struct wl_kms_buffer *buffer)
{
	return buffer->format;
}

int wayland_kms_query_buffer(struct wl_kms *kms, struct wl_resource *resource,
				enum wl_kms_attribute attr, int *value)
{
	struct wl_kms_buffer *buffer = wayland_kms_buffer_get(resource);
	if (!buffer)
		return -1;

	switch(attr) {
	case WL_KMS_WIDTH:
		*value = buffer->width;
		return 0;

	case WL_KMS_HEIGHT:
		*value = buffer->height;
		return 0;
	
	case WL_KMS_TEXTURE_FORMAT:
		*value = EGL_TEXTURE_RGBA;
		return 0;
	}

	return -1;
}
