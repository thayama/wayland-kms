#ifndef WAYLAND_KMS_AUTH_H
#define WAYLAND_KMS_AUTH_H

struct kms_auth;

extern struct kms_auth *kms_auth_init(struct wl_display *display);
extern void kms_auth_uninit(struct kms_auth *auth);
extern int kms_auth_request(struct kms_auth *auth, uint32_t magic);

#endif
