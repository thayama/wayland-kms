project(
  'wayland-kms', 'c',
  version: '1.6.1',
  license: 'MIT',
  meson_version: '>= 0.54.0',
)

config_h = configuration_data()
config_h.set_quoted('PACKAGE', meson.project_name())
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())

cc = meson.get_compiler('c')
add_project_arguments(
  language: 'c'
)

pkgconfig = import('pkgconfig')

dep_wayland_server = dependency('wayland-server')
dep_wayland_client = dependency('wayland-client')
dep_libdrm = dependency('libdrm')
dep_libdrm_headers = dep_libdrm.partial_dependency(compile_args: true)

subdir('protocol')
subdir('src')
