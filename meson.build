project('udp-echo-server', 'c', default_options : ['c_std=c17'])

liburing_dep = dependency('liburing')
thread_dep = dependency('threads')

executable('udp-echo-server',
           'main.c',
           c_args: ['-g'],
           dependencies: [liburing_dep, thread_dep],
           )
