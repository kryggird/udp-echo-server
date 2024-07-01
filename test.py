#!/usr/bin/python
import contextlib
import shutil
import subprocess
from subprocess import DEVNULL
import socket
from time import sleep
import os

TEST_BYTESTRING = b"Hello World!"

@contextlib.contextmanager
def kill_on_exit(process):
    try:
        yield process
    finally:
        process.kill()

if __name__ == "__main__":
    if shutil.which('valgrind'):
        cmd = ['valgrind', 'builddir/udp-echo-server', '--port', '8080']
    else:
        print("valgrind not available")
        cmd = ['builddir/udp-echo-server', '--port', '8080']

    result = 1
    with kill_on_exit(subprocess.Popen(cmd, stdout=DEVNULL)) as p:
        sleep(1) # Hacky...

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(3)
        sock.sendto(TEST_BYTESTRING, ('0.0.0.0', 8080))

        response, addr = sock.recvfrom(1024)

        result = response != TEST_BYTESTRING

    # Return the result
    exit(int(result))
