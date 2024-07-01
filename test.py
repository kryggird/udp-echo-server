#!/usr/bin/python
import contextlib
import shutil
import subprocess
from subprocess import DEVNULL
import socket
from time import sleep
import os
import sys

TEST_BYTESTRING = b"Hello World!"
ADDRESS_V4 = '0.0.0.0'
ADDRESS_V6 = '::1'

@contextlib.contextmanager
def kill_on_exit(process):
    try:
        yield process
    finally:
        process.kill()

if __name__ == "__main__":
    cmd = ['builddir/udp-echo-server', '--port', '9999']

    address = ADDRESS_V4
    kind = socket.AF_INET
    if (len(sys.argv) >= 2 and sys.argv[1] == '--v6'):
        kind = socket.AF_INET6
        address = ADDRESS_V6

    result = 1
    with kill_on_exit(subprocess.Popen(cmd, stdout=DEVNULL)) as p:
        sleep(1) # Hacky...

        sock = socket.socket(kind, socket.SOCK_DGRAM)
        sock.settimeout(3)
        sock.sendto(TEST_BYTESTRING, (address, 9999))

        response, addr = sock.recvfrom(1024)

        result = response != TEST_BYTESTRING

    # Return the result
    exit(int(result))
