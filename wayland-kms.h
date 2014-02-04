#ifndef WAYLAND_KMS_H
#define WAYLAND_KMS_H

#include <wayland-server.h>

struct wl_kms;

struct wl_kms_buffer {
	struct wl_resource *resource;
	struct wl_kms *kms;
	int32_t width, height;
	uint32_t stride, format;
	uint32_t handle;
	int fd;
};

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
