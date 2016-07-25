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

#include <EGL/egl.h>
#include "weston-egl-ext.h"

#if defined(DEBUG)
#	define WLKMS_DEBUG(s, x...) { printf(s, ##x); }
#else
#	define WLKMS_DEBUG(s, x...) { }
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

static void close_drm_handle(int fd, uint32_t handle)
{
	struct drm_gem_close gem_close = { .handle = handle };
	int ret;

	ret = drmIoctl(fd, DRM_IOCTL_GEM_CLOSE, &gem_close);
	if (ret)
		WLKMS_DEBUG("%s: %s: DRM_IOCTL_GEM_CLOSE failed.(%s)\n",
			 __FILE__, __func__, strerror(errno));

}

static void destroy_buffer(struct wl_resource *resource)
{
	struct wl_kms_buffer *buffer = resource->data;
	int i;

	for (i = 0; i < buffer->num_planes; i++) {
		close(buffer->planes[i].fd);
		close_drm_handle(buffer->kms->fd, buffer->planes[i].handle);
	}

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

/* Note: This API closes unused fds passed through its call. */
static void
kms_create_mp_buffer(struct wl_client *client, struct wl_resource *resource,
		     uint32_t id, int32_t width, int32_t height, uint32_t format,
		     int32_t fd0, uint32_t stride0, int32_t fd1, uint32_t stride1,
		     int32_t fd2, uint32_t stride2)
{
	struct wl_kms *kms = resource->data;
	struct wl_kms_buffer *buffer;
	int err, nplanes;

	switch (format) {
	case WL_KMS_FORMAT_ARGB8888:
	case WL_KMS_FORMAT_XRGB8888:
	case WL_KMS_FORMAT_ABGR8888:
	case WL_KMS_FORMAT_XBGR8888:
	case WL_KMS_FORMAT_RGB888:
	case WL_KMS_FORMAT_BGR888:
	case WL_KMS_FORMAT_YUYV:
	case WL_KMS_FORMAT_YVYU:
	case WL_KMS_FORMAT_UYVY:
	case WL_KMS_FORMAT_RGB565:
	case WL_KMS_FORMAT_BGR565:
	case WL_KMS_FORMAT_RGB332:
		nplanes = 1;
		break;

	case WL_KMS_FORMAT_NV12:
	case WL_KMS_FORMAT_NV21:
	case WL_KMS_FORMAT_NV16:
	case WL_KMS_FORMAT_NV61:
		nplanes = 2;
		break;

	case WL_KMS_FORMAT_YUV420:
		nplanes = 3;
		break;

	default:
		wl_resource_post_error(resource,
				       WL_KMS_ERROR_INVALID_FORMAT,
				       "invalid format");
		return;
	}

	/* Wayland passes dup'd fds that must be closed when
	   no longer needed. Close the unused ones
	   immediately to avoid leaking them. */
	if (fd1 != WL_KMS_INVALID_FD && nplanes < 2)
		close(fd1);
	if (fd2 != WL_KMS_INVALID_FD && nplanes < 3)
		close(fd2);

	buffer = calloc(1, sizeof *buffer);
	if (buffer == NULL) {
		wl_resource_post_no_memory(resource);
		return;
	}

	buffer->kms = kms;
	buffer->width = width;
	buffer->height = height;
	buffer->format = format;
	buffer->num_planes = nplanes;
	buffer->stride = buffer->planes[0].stride = stride0;
	buffer->fd = buffer->planes[0].fd = fd0;

	if (err = drmPrimeFDToHandle(kms->fd, fd0, &buffer->planes[0].handle))
		goto invalid_fd_error;
	buffer->handle = buffer->planes[0].handle;

	if (nplanes > 1) {
		buffer->planes[1].stride = stride1;
		buffer->planes[1].fd = fd1;
		if (err = drmPrimeFDToHandle(kms->fd, fd1, &buffer->planes[1].handle)) {
			close_drm_handle(kms->fd, buffer->planes[0].handle);
			goto invalid_fd_error;
		}
	}

	if (nplanes > 2) {
		buffer->planes[2].stride = stride2;
		buffer->planes[2].fd = fd2;
		if (err = drmPrimeFDToHandle(kms->fd, fd2, &buffer->planes[2].handle)) {
			close_drm_handle(kms->fd, buffer->planes[0].handle);
			close_drm_handle(kms->fd, buffer->planes[1].handle);
			goto invalid_fd_error;
		}
	}

	WLKMS_DEBUG("%s: %s: %d planes (%d, %d, %d)\n", __FILE__, __func__, nplanes, fd0, fd1, fd2);

	// XXX: Do we need to support multiplaner KMS BO?

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
	return;

invalid_fd_error:
	WLKMS_DEBUG("%s: %s: drmPrimeFDToHandle() failed...%d (%s)\n", __FILE__, __func__, err, strerror(errno));
	wl_resource_post_error(resource, WL_KMS_ERROR_INVALID_FD, "invalid prime FD");
	free(buffer);
}


static void
kms_create_buffer(struct wl_client *client, struct wl_resource *resource,
		  uint32_t id, int32_t prime_fd, int32_t width, int32_t height,
		  uint32_t stride, uint32_t format, uint32_t handle)
{
	kms_create_mp_buffer(client, resource, id, width, height, format, prime_fd, stride,
			     WL_KMS_INVALID_FD, 0, WL_KMS_INVALID_FD, 0);
}

const static struct wl_kms_interface kms_interface = {
	.authenticate = kms_authenticate,
	.create_buffer = kms_create_buffer,
	.create_mp_buffer = kms_create_mp_buffer,
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
	wl_resource_post_event(resource, WL_KMS_FORMAT, WL_KMS_FORMAT_ABGR8888);
	wl_resource_post_event(resource, WL_KMS_FORMAT, WL_KMS_FORMAT_XBGR8888);
	wl_resource_post_event(resource, WL_KMS_FORMAT, WL_KMS_FORMAT_RGB888);
	wl_resource_post_event(resource, WL_KMS_FORMAT, WL_KMS_FORMAT_BGR888);
	wl_resource_post_event(resource, WL_KMS_FORMAT, WL_KMS_FORMAT_YUYV);
	wl_resource_post_event(resource, WL_KMS_FORMAT, WL_KMS_FORMAT_YVYU);
	wl_resource_post_event(resource, WL_KMS_FORMAT, WL_KMS_FORMAT_UYVY);
	wl_resource_post_event(resource, WL_KMS_FORMAT, WL_KMS_FORMAT_RGB565);
	wl_resource_post_event(resource, WL_KMS_FORMAT, WL_KMS_FORMAT_BGR565);
	wl_resource_post_event(resource, WL_KMS_FORMAT, WL_KMS_FORMAT_RGB332);
	wl_resource_post_event(resource, WL_KMS_FORMAT, WL_KMS_FORMAT_NV12);
	wl_resource_post_event(resource, WL_KMS_FORMAT, WL_KMS_FORMAT_NV21);
	wl_resource_post_event(resource, WL_KMS_FORMAT, WL_KMS_FORMAT_NV16);
	wl_resource_post_event(resource, WL_KMS_FORMAT, WL_KMS_FORMAT_NV61);
	wl_resource_post_event(resource, WL_KMS_FORMAT, WL_KMS_FORMAT_YUV420);
}

int wayland_kms_fd_get(struct wl_kms* kms)
{
	return kms->fd;
}

static struct wl_kms *__wl_kms = NULL;

struct wl_kms_buffer *wayland_kms_buffer_get(struct wl_resource *resource)
{
	if (!__wl_kms || resource == NULL)
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
	if (__wl_kms)
		return __wl_kms;

	if (!(__wl_kms = calloc(1, sizeof(struct wl_kms))))
		return NULL;

	__wl_kms->display = display;
	__wl_kms->device_name = strdup(device_name);
	__wl_kms->fd = fd;

	wl_global_create(display, &wl_kms_interface, 2, __wl_kms, bind_kms);

	/*
	 * we're the server in the middle. we should forward the auth
	 * request to our server.
	 */
	if (server) {
		drm_magic_t magic;

		if (!(__wl_kms->auth = kms_auth_init(server)))
			goto error;

		/* get a magic */
		if (drmGetMagic(fd, &magic) < 0)
			goto error;

		/* authenticate myself */
		if (kms_auth_request(__wl_kms->auth, magic) < 0)
			goto error;
	}

	return __wl_kms;

error:
	free(__wl_kms);
	__wl_kms = NULL;
	return NULL;
}

void wayland_kms_uninit(struct wl_kms *kms)
{
	if (kms != __wl_kms)
		return;

	free(kms->auth);
	free(kms->device_name);
	free(kms);

	/* FIXME: need wl_display_del_{object,global} */
	__wl_kms = NULL;
}

uint32_t wayland_kms_buffer_get_format(struct wl_kms_buffer *buffer)
{
	return buffer->format;
}

static
int wayland_kms_get_texture_format(struct wl_kms_buffer *buffer)
{
	int format = 0;

	switch (buffer->format) {
	case WL_KMS_FORMAT_ARGB8888:
	case WL_KMS_FORMAT_ABGR8888:
		format = EGL_TEXTURE_RGBA;
		break;

	case WL_KMS_FORMAT_XRGB8888:
	case WL_KMS_FORMAT_XBGR8888:
	case WL_KMS_FORMAT_RGB888:
	case WL_KMS_FORMAT_BGR888:
	case WL_KMS_FORMAT_RGB565:
	case WL_KMS_FORMAT_BGR565:
	case WL_KMS_FORMAT_RGB332:
		format = EGL_TEXTURE_RGB;
		break;

	case WL_KMS_FORMAT_NV12:
	case WL_KMS_FORMAT_NV21:
	case WL_KMS_FORMAT_NV16:
	case WL_KMS_FORMAT_NV61:
		format = EGL_TEXTURE_EXTERNAL_WL;
	}

	return format;
}

int wayland_kms_query_buffer(struct wl_kms *kms, struct wl_resource *resource,
				enum wl_kms_attribute attr, int *value)
{
	struct wl_kms_buffer *buffer = wayland_kms_buffer_get(resource);
	if (!buffer)
		return -1;

	switch (attr) {
	case WL_KMS_WIDTH:
		*value = buffer->width;
		return 0;

	case WL_KMS_HEIGHT:
		*value = buffer->height;
		return 0;
	
	case WL_KMS_TEXTURE_FORMAT:
		*value = wayland_kms_get_texture_format(buffer);
		return 0;
	}

	return -1;
}
