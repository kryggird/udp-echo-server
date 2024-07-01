# Simple UDP server

A simple UDP echo server for Linux built on top of `io_uring`.
It uses a "thread-per-core" model where one thread (and one uring) is spawned 
for each core in the machine.

To minimize the number of syscalls needed to operate, operations are batched
and the multishot recv functionaly of `io_uring` is used.
The server can optionally send responses using the new zero-copy functionality
of the Linux kernel with the flag `--send_zc`.

Note that, due to the use of certain `io_uring` features, the server needs
to run on Linux 6.0 or later.

## Build instructions

This server depends on `liburing` and `pthreads`. Meson's wraptool can be used
to install the needed dependencies. A script called `first_setup.sh` will run
the required commands.

Alternatively, you can run the following:

```sh
mkdir -p builddir
mkdir -p subprojects

# Download liburing dependency
meson wrap install liburing

# Setup and compile
meson setup --reconfigure builddir
meson compile -C builddir
```

## Running

The server can be run with the following command

```
builddir/udp-echo-server --port <port> --print-stats
```

The server listen to all addresses and defaults to IPv6. If one wants to limit
the server to IPv4, one can use the `--ipv4` flag.

Also, by default, the server uses all available cores. A `--single-threaded` flag
is provided for single core operation. For more complex topologies, the
`taskset` utility from coreutils can be used. 
The server will decide how many threads to spin based on the CPU mask, not on
the number of cores.
