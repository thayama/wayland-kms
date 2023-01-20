dep_scanner = dependency('wayland-scanner', native: true)
prog_scanner = find_program(dep_scanner.get_pkgconfig_variable('wayland_scanner'))

base_file = 'wayland-kms'
xml_path = 'wayland-kms.xml'

foreach output_type: [ 'client-header', 'server-header', 'private-code' ]
  if output_type == 'client-header'
    output_file = '@0@-client-protocol.h'.format(base_file)
  elif output_type == 'server-header'
    output_file = '@0@-server-protocol.h'.format(base_file)
  else
    output_file = '@0@-protocol.c'.format(base_file)
    if dep_scanner.version().version_compare('< 1.14.91')
      output_type = 'code'
    endif
  endif

  var_name = output_file.underscorify()
  target = custom_target(
    '@0@ @1@'.format(base_file, output_type),
    command: [ prog_scanner, output_type, '@INPUT@', '@OUTPUT@' ],
    input: xml_path,
    output: output_file,
  )

  set_variable(var_name, target)
endforeach
