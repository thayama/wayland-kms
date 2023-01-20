deps_libwayland_kms = [
  dep_wayland_server,
  dep_wayland_client,
  dep_libdrm,
  dep_libdrm_headers,
]

srcs_libwayland_kms = [
  'wayland-kms-auth.c',
  'wayland-kms-auth.h',
  'wayland-kms.c',
  'wayland-kms.h',
  'weston-egl-ext.h',
  wayland_kms_server_protocol_h,
  wayland_kms_client_protocol_h,
  wayland_kms_protocol_c,
]

lib_wayland_kms = shared_library(
  'wayland-kms',
  srcs_libwayland_kms,
  install: true,
  version: meson.project_version(),
  dependencies: deps_libwayland_kms,
)

pkgconfig.generate(
  lib_wayland_kms,
  name: 'wayland-kms',
  version: meson.project_version(),
  description: 'wayland-kms library',
  requires_private: [ dep_wayland_server, dep_libdrm ],
)
