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
#include <wayland-client.h>
#include "wayland-kms-auth.h"
#include "wayland-kms-client-protocol.h"

#if defined(DEBUG)
#	define WLKMS_DEBUG(s, x...) { printf(s, ##x); }
#else
#	define WLKMS_DEBUG(s, x...) { }
#endif

struct kms_auth {
	struct wl_display *wl_display;	/* wl_display facing my server */
	struct wl_event_queue *wl_queue;
	struct wl_registry *wl_registry;
	struct wl_kms *wl_kms;
	int authenticated;
};


/*
 * Sync with the server
 */

static void wayland_sync_callback(void *data, struct wl_callback *callback, uint32_t serial)
{
	int *done = data;
	*done = 1;
	WLKMS_DEBUG("%s: %s: %d\n", __FILE__, __func__, __LINE__);
	wl_callback_destroy(callback);
}

static const struct wl_callback_listener wayland_sync_listener = {
	.done = wayland_sync_callback
};

/*
 * sync with the server
 */
static int wayland_sync(struct kms_auth *auth)
{
	struct wl_callback *callback;
	int ret = 0, done = 0;

	WLKMS_DEBUG("%s: %s: %d\n", __FILE__, __func__, __LINE__);
	callback = wl_display_sync(auth->wl_display);
	wl_callback_add_listener(callback, &wayland_sync_listener, &done);
	wl_proxy_set_queue((struct wl_proxy*)callback, auth->wl_queue);
	while (ret >= 0 && !done) {
		ret = wl_display_dispatch_queue(auth->wl_display, auth->wl_queue);
	}

	if (!done) {
		wl_callback_destroy(callback);
	}

	return ret;
}

/*
 * For the nested authentication
 */

static void wayland_kms_handle_authenticated(void *data, struct wl_kms *kms)
{
	struct kms_auth *auth = data;
	WLKMS_DEBUG("%s: %s: %d: authenticated.\n", __FILE__, __func__, __LINE__);
	auth->authenticated = 1;
}

static void wayland_kms_handle_format(void *data, struct wl_kms *kms, uint32_t format)
{
}

static void wayland_kms_handle_device(void *data, struct wl_kms *kms, const char *device)
{
}

static const struct wl_kms_listener wayland_kms_listener = {
	.authenticated = wayland_kms_handle_authenticated,
	.format = wayland_kms_handle_format,
	.device = wayland_kms_handle_device
};

/*
 * registry routines to the server global objects
 */

static void wayland_registry_handle_global(void *data, struct wl_registry *registry,
		                                           uint32_t name, const char *interface, uint32_t version)
{
	struct kms_auth *auth = data;

	WLKMS_DEBUG("%s: %s: %d\n", __FILE__, __func__, __LINE__);

	/*
	 * we need to connect to the wl_kms objects
	 */
	if (!strcmp(interface, "wl_kms")) {
		auth->wl_kms = wl_registry_bind(registry, name, &wl_kms_interface, version);
		wl_kms_add_listener(auth->wl_kms, &wayland_kms_listener, auth);
	}
}

static void wayland_registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
}

static const struct wl_registry_listener wayland_registry_listener = {
	.global = wayland_registry_handle_global,
	.global_remove = wayland_registry_handle_global_remove,
};

int
kms_auth_request(struct kms_auth *auth, uint32_t magic)
{
	auth->authenticated = 0;
	wl_kms_authenticate(auth->wl_kms, magic);

	if (wayland_sync(auth) < 0 || !auth->authenticated)
		return -1;

	return 0;
}

struct kms_auth*
kms_auth_init(struct wl_display *display)
{
	struct kms_auth *auth;

	if (!(auth = calloc(1, sizeof(struct kms_auth))))
		return NULL;

	auth->wl_display = display;

	auth->wl_queue = wl_display_create_queue(auth->wl_display);
	auth->wl_registry = wl_display_get_registry(auth->wl_display);
	wl_proxy_set_queue((struct wl_proxy*)auth->wl_registry, auth->wl_queue);
	wl_registry_add_listener(auth->wl_registry, &wayland_registry_listener, auth);

	if (wayland_sync(auth) < 0) {
		kms_auth_uninit(auth);
		return NULL;
	}

	return auth;
}

void
kms_auth_uninit(struct kms_auth *auth)
{
	if (!auth)
		return;

	if (auth->wl_kms)
		wl_kms_destroy(auth->wl_kms);

	if (auth->wl_registry)
		wl_registry_destroy(auth->wl_registry);

	free(auth);
}
