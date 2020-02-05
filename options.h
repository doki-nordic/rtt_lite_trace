/*
 * Copyright (c) 2019 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef _options_h_
#define _options_h_

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>
#include "dyn_nrfjprogdll.h"


struct options_t
{
    uint32_t verbose;

    const char *iface;

    uint32_t snr;
    device_family_t family;
    uint32_t speed;
    uint32_t rtt_cb_address;
    const char* jlink_lib;
    const char* nrfjprog_lib;

    uint32_t poll_time_us;
    bool no_rtt_retry;

    const char* hang_file;
};

extern struct options_t options;

void parse_args(int argc, char* argv[]);


#endif
