#include "server-helper.h"
#include "thread-helper.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int DEFAULT_PORT = 8080;
const bool DEFAULT_SEND_ZC = false;
const bool DEFAULT_SINGLE_THREADED = false;
const bool DEFAULT_IP_V4 = false;

typedef struct options_t {
    uint32_t port;
    bool send_zc;
    bool single_threaded;
    bool ip_v4;
} options_t;

static struct option long_options[] = {{"port", required_argument, 0, 'p'},
                                       {"send_zc", no_argument, 0, 's'},
                                       {"single-threaded", no_argument, 0, 't'},
                                       {"ipv4", no_argument, 0, 'i'},
                                       {0, 0, 0, 0}};

static void print_usage(const char* program_name) {
    fprintf(stderr, "Usage: %s [options]\n", program_name);
    fprintf(stderr, "  --port <uint32_t>       Set port (default: %d)\n",
            DEFAULT_PORT);
    fprintf(stderr, "  --send_zc              Enable send_zc (default: %s)\n",
            DEFAULT_SEND_ZC ? "true" : "false");
    fprintf(
        stderr,
        "  --single-threaded      Run in single-threaded mode (default: %s)\n",
        DEFAULT_SINGLE_THREADED ? "true" : "false");
    fprintf(stderr, "  --ipv4                 Use IPv4 (default: false)\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
    options_t opts = {.port = DEFAULT_PORT,
                      .send_zc = DEFAULT_SEND_ZC,
                      .single_threaded = DEFAULT_SINGLE_THREADED,
                      .ip_v4 = DEFAULT_IP_V4};

    int c;
    while ((c = getopt_long_only(argc, argv, "", long_options, NULL)) != -1) {
        switch (c) {
            case 'p':
                opts.port = strtoul(optarg, NULL, 10);
                break;
            case 's':
                opts.send_zc = true;
                break;
            case 't':
                opts.single_threaded = true;
                break;
            case '?':
                print_usage(argv[0]);
                break;
            default:
                abort();
        }
    }

    printf("Port: %u\n", opts.port);
    printf("Send ZC: %s\n", opts.send_zc ? "true" : "false");
    printf("Single-threaded: %s\n", opts.single_threaded ? "true" : "false");
    printf("IP type: %s\n", opts.ip_v4 ? "v4" : "v6");

    if (opts.single_threaded) {
        run_server(opts.ip_v4, opts.port);
    } else {
        run_many(opts.ip_v4, opts.port);
    }

    return 0;
}
