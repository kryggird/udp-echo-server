project('udp-echo-server', 'c', default_options : ['c_std=c17'])

liburing_dep = dependency('liburing')

executable('udp-echo-server',
           'main.c',
           c_args: ['-march=skylake', '-g'],
           dependencies: [liburing_dep],
           )
