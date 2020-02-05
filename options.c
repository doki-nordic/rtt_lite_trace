/*
 * Copyright (c) 2019 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <nrfjprog.h>
#include <arpa/inet.h>

#include "rtt.h"
#include "logs.h"
#include "version.h"

#include "options.h"

#define OPT_VERBOSE 'v'
#define OPT_HELP 'h'
#define OPT_SNR 's'
#define OPT_CLOCKSPEED 'c'
#define OPT_FAMILY 'f'
#define OPT_CB_ADDRESS 'a'
#define OPT_VER (0x100 + 1)
#define OPT_POLLTIME (0x100 + 2)
#define OPT_NORTTRETRY (0x100 + 3)
#define OPT_JLINKLIB (0x100 + 4)
#define OPT_NRFJPROGLIB (0x100 + 7)
#define OPT_HANGFILE (0x100 + 8)


#define DESC(text) "\0" text
#define END "\0"
#define HELP_OPT_LEN 17
#define _STR2(x) #x
#define STR(x) _STR2(x)


struct options_t options =
{
    .verbose = LOG_ERROR,
    .snr = 0,
    .family = UNKNOWN_FAMILY,
    .speed = JLINKARM_SWD_DEFAULT_SPEED_KHZ,
    .rtt_cb_address = 0,
    .jlink_lib = NULL,
    .nrfjprog_lib = "libnrfjprogdll.so",
    .poll_time_us = 5 * 1000,
    .no_rtt_retry = false,
    .hang_file = NULL,
};


static struct option long_options[] =
{
    {"help"
        DESC("Print this help message")
        END, no_argument, 0, OPT_HELP},
    {"version"
        DESC("Print version information")
        END, no_argument, 0, OPT_VER},
    {"verbose"
        DESC("Set verbosity level:")
        DESC("  0 - disable all messages")
        DESC("  1 - inform only about errors (default)")
        DESC("  2 - prints information")
        DESC("  3 - prints debugs")
        DESC("  4 - prints debugs and nrfjprogdll messages")
        END, required_argument, 0, OPT_VERBOSE},
    {"snr"
        DESC("Selects the debugger with the given serial number")
        DESC("among all those connected to the PC for the")
        DESC("operation.")
        END, required_argument, 0, OPT_SNR},
    {"family"
        DESC("Selects the device family for the operation. Valid")
        DESC("argument options are NRF51, NRF52 and UNKNOWN.")
        END, required_argument, 0, OPT_FAMILY},
    {"clockspeed"
        DESC("Sets the debugger SWD clock speed in kHz")
        DESC("resolution for the operation.")
        END, required_argument, 0, OPT_CLOCKSPEED},
    {"rttcbaddr"
        DESC("Sets address where RTT control block is localted.")
        END, required_argument, 0, OPT_CB_ADDRESS},
    {"jlinklib"
        DESC("Path to SEGGER J-Link library dll. There is no")
        DESC("need to provide it if dlopen() can find it or")
        DESC("it is on its default location:")
        DESC("/opt/SEGGER/JLink/libjlinkarm.so")
        END, required_argument, 0, OPT_JLINKLIB},
    {"nrfjproglib"
        DESC("Path to nrfjprog library (libnrfjprogdll.so).")
        DESC("There is no need to provide it if dlopen() can find")
        DESC("it.")
        END, required_argument, 0, OPT_NRFJPROGLIB},
    {"polltime"
        DESC("Interval time in milliseconds for RTT polling loop.")
        END, required_argument, 0, OPT_POLLTIME},
    {"norttretry"
        DESC("Do not try to recover from RTT errors by restarting")
        DESC("entire process")
        END, no_argument, 0, OPT_NORTTRETRY},
    {"hangfile"
        DESC("File that will be polled. If it exists link will")
        DESC("be temporary stopped until file is deleted.")
        END, required_argument, 0, OPT_HANGFILE},
    {0, 0, 0, 0}
};


void print_help()
{
    char line[HELP_OPT_LEN + 10];
    const char* str;
    struct option *opt;

    opt = long_options;
    while (opt->name)
    {
        str = opt->name;
        if (opt->val < 255)
        {
            printf("\n -%c  ", opt->val);
        }
        else
        {
            printf("\n     ");
        }

        strcpy(line, str);

        if (opt->has_arg == required_argument)
        {
            strcat(line, " <val>");
        }
        printf("--%-" STR(HELP_OPT_LEN) "s ", line);
        str += strlen(str) + 1;
        do
        {
            printf("%s\n", str);
            printf("       %" STR(HELP_OPT_LEN) "s ", "");
            str += strlen(str) + 1;
        } while (str[0]);
        opt++;
    }
    printf("\n");
}


uint32_t parse_arg_uint(const char *arg, uint32_t min, uint32_t max)
{
    char* end = NULL;
    uint32_t result = strtoull(arg, &end, 0);
    if (end == NULL || end[0] != '\0')
    {
        O_FATAL("Invalid integer argument '%s'", arg);
    }
    if (result < min || result > max)
    {
        O_FATAL("Integer argument '%s' out of range [%d-%d]", arg, min, max);
    }
    return result;
}


int parse_subnet_bits(const char *arg, uint32_t max_bits, const char **address)
{
    static char temp[64];
    char *subnet_start;
    strncpy(temp, arg, sizeof(temp));
    temp[sizeof(temp) - 0] = 0;
    subnet_start = strchr(temp, '/');
    *address = temp;
    if (subnet_start == NULL)
    {
        return -1;
    }
    subnet_start[0] = 0;
    subnet_start++;
    return parse_arg_uint(subnet_start, 0, max_bits);
}


int ci_strcmp(const char* a, const char* b)
{
    while (*a || *b)
    {
        uint8_t ua = (uint8_t)toupper(*a);
        uint8_t ub = (uint8_t)toupper(*b);
        if (ua < ub)
        {
            return -1;
        }
        else if (ua > ub)
        {
            return 1;
        }
        a++;
        b++;
    }
    return 0;
}

void parse_args(int argc, char* argv[])
{
    bool show_version = false;
    int c;
    char short_args[3 * sizeof(long_options) / sizeof(long_options[0]) + 1];
    struct option *opt;
    int index;
    const char *arg;

    opt = long_options;
    while (opt->name)
    {
        sprintf(short_args + strlen(short_args), "%c%s", opt->val, (opt->has_arg == no_argument) ? "" : (opt->has_arg == required_argument) ? ":" : "::");
        opt++; 
    }

    do
    {
        int option_index = 0;

        c = getopt_long(argc, argv, short_args, long_options, &option_index);

        arg = optarg;
        while (arg && *arg && (uint8_t)(*arg) <= ' ')
        {
            arg++;
        }

        switch (c)
        {
            case -1:
                break;

            case OPT_HELP:
                print_help();
                exit(0);
                break;

            case OPT_VER:
                show_version = true;
                break;

            case OPT_VERBOSE:
                if (arg && arg[0])
                {
                    options.verbose = parse_arg_uint(arg, LOG_NONE, LOG_NRFJPROG);
                }
                else
                {
                    options.verbose = LOG_INFO;
                }
                break;

            case OPT_SNR:
                options.snr = parse_arg_uint(arg, 1, UINT32_MAX);
                break;

            case OPT_FAMILY:
                if (ci_strcmp("NRF51", arg) == 0)
                {
                    options.family = NRF51_FAMILY;
                }
                else if (ci_strcmp("NRF52", arg) == 0)
                {
                    options.family = NRF52_FAMILY;
                }
                else if (ci_strcmp("NRF53", arg) == 0)
                {
                    //TODO: options.family = NRF53_FAMILY;
                }
                else if (ci_strcmp("NRF91", arg) == 0)
                {
                    //TODO: options.family = NRF91_FAMILY;
                }
                else if (ci_strcmp("UNKNOWN", arg) == 0)
                {
                    options.family = UNKNOWN_FAMILY;
                }
                else
                {
                    O_FATAL("Invalid family name");
                }
                break;

            case OPT_CLOCKSPEED:
                options.speed = parse_arg_uint(arg, JLINKARM_SWD_MIN_SPEED_KHZ, JLINKARM_SWD_MAX_SPEED_KHZ);
                break;

            case OPT_CB_ADDRESS:
                options.rtt_cb_address = parse_arg_uint(arg, 0, 0xFFFFFFFF);
                break;

            case OPT_JLINKLIB:
                options.jlink_lib = strdup(arg);
                break;

            case OPT_NRFJPROGLIB:
                options.nrfjprog_lib = strdup(arg);
                break;

            case OPT_POLLTIME:
                options.poll_time_us = 1000 * parse_arg_uint(arg, 1, 999);
                break;

            case OPT_NORTTRETRY:
                options.no_rtt_retry = true;
                break;

            case OPT_HANGFILE:
                options.hang_file = strdup(arg);
                break;

            case '?':
                // Error message already printed by getopt.
                break;

            default:
                O_FATAL("Unexpected getopt error %02X", c);
        }
    } while (c >= 0);

    if (optind < argc)
    {
        O_FATAL("Unexpected parameter '%s'\n", argv[optind]);
    }

    if (show_version)
    {
        uint32_t major = 0;
        uint32_t minor = 0;
        char revision[32] = "0";

        printf("Version:                   %d.%d.%d\n", APP_MAJOR_VERSION, APP_MINOR_VERSION, APP_MICRO_VERSION);
        printf("Compiled with nrfjprogdll: %d.%d.%d\n", major_version, minor_version, micro_version);

        if (!get_jlink_version(&major, &minor, revision))
        {
            O_FATAL("NRFJPROG error");
        }
        printf("Loaded SEGGER J-Link:      %d.%d%s\n", major, minor, revision);

        exit(0);
    }
}
