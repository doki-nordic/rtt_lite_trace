/*
 * Copyright (c) 2019 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "options.h"
#include "dyn_nrfjprogdll.h"
#include "logs.h"

#include "rtt.h"

#define MAX_WRITE_QUEUE_SIZE (2 * 1024 * 1024)


static const char* jlink_lib_def[] = {
    "libjlinkarm.so",
    "/opt/SEGGER/JLink/libjlinkarm.so"
};


static void nrfjprog_callback(const char *msg)
{
    PRINT_NRFJPROG("%s", msg);
}


static int channel_up = -1;

static nrfjprogdll_err_t open_jlink(device_family_t family)
{
    int i;
    nrfjprogdll_err_t error;
    const char *error_symbol;

    error_symbol = load_nrfjprogdll(options.nrfjprog_lib);

    if (error_symbol && error_symbol[0])
    {
        U_FATAL("NRFJPROG error: undefined reference to '%s'.\nInvalid nrfjprog library provided.\nSee --nrfjproglib option for help.", error_symbol);
    }
    else if (error_symbol)
    {
        U_FATAL("NRFJPROG error: nrfjprog library cannot be open.\nSee --nrfjproglib option for help.");
    }

    if (options.jlink_lib == NULL)
    {
        for (i = 0; i < sizeof(jlink_lib_def)/sizeof(jlink_lib_def[0]); i++)
        {
            error = NRFJPROG_open_dll(jlink_lib_def[i], nrfjprog_callback, family);
            if (error == SUCCESS)
            {
                options.jlink_lib = jlink_lib_def[i];
                return error;
            }
        }
    }
    else
    {
        error = NRFJPROG_open_dll(options.jlink_lib, nrfjprog_callback, family);
    }

    if (error == JLINKARM_DLL_COULD_NOT_BE_OPENED)
    {
        U_FATAL("NRFJPROG error: J-Link library cannot be open. See --jlinklib option in help.");
    }

    return error;
}

bool get_jlink_version(uint32_t *major, uint32_t *minor, char *revision)
{
    nrfjprogdll_err_t err = open_jlink(options.family);
    if (err != SUCCESS)
    {
        return false;
    }
    err = NRFJPROG_dll_version(major, minor, revision);
    NRFJPROG_close_dll();
    return true;
}

bool nrfjprog_init()
{
	unsigned int i, up, down;
	nrfjprogdll_err_t error;

    if (options.family == UNKNOWN_FAMILY)
    {
        error = open_jlink(UNKNOWN_FAMILY);
        if (error != SUCCESS)
        {
            R_FATAL("Cannot open JLink library %d!", error);
        }
        if (options.snr)
        {
            error = NRFJPROG_connect_to_emu_with_snr(options.snr, options.speed);
        }
        else
        {
            error = NRFJPROG_connect_to_emu_without_snr(options.speed);
        }
        if (error != SUCCESS)
        {
            R_FATAL("Cannot connect to emmulator to detect device family!");
        }
        if (!options.snr)
        {
            error = NRFJPROG_read_connected_emu_snr(&options.snr);
            if (error != SUCCESS)
            {
                R_FATAL("Cannot read SNR!");
            }
        }
        error = NRFJPROG_read_device_family(&options.family);
        if (error != SUCCESS)
        {
            R_FATAL("Cannot detect device family!");
        }
        NRFJPROG_close_dll();
    }

	error = open_jlink(options.family);
	if (error != SUCCESS)
	{
		R_FATAL("Cannot open JLink library!");
	}

    if (options.snr)
    {
	    error = NRFJPROG_connect_to_emu_with_snr(options.snr, options.speed);
        PRINT_INFO("snr: %d, speed: %d", options.snr, options.speed);
    }
    else
    {
	    error = NRFJPROG_connect_to_emu_without_snr(options.speed);
        PRINT_INFO("speed: %d", options.speed);
    }

	if (error != SUCCESS)
	{
		R_FATAL("Cannot connect to emmulator!");
	}

	error = NRFJPROG_connect_to_device();
	if (error != SUCCESS)
	{
		R_FATAL("Cannot connect to device!");
	}

    if (options.rtt_cb_address != 0)
    {
	    error = NRFJPROG_rtt_set_control_block_address(options.rtt_cb_address);
    	if (error != SUCCESS)
    	{
    		R_FATAL("Cannot set RTT control block address!");
    	}
    }

	error = NRFJPROG_rtt_start();
	if (error != SUCCESS)
	{
		R_FATAL("Cannot start RTT!");
	}

	i = 30;
	while (i--)
	{
		bool rtt_found;

		error = NRFJPROG_rtt_is_control_block_found(&rtt_found);
		if (error != SUCCESS)
		{
			R_FATAL("Cannot check RTT control block status!");
		}

		if (rtt_found)
			break;

		usleep(100 * 1000);
	}

	if (i == 0)
	{
		R_FATAL("RTT Control Block not found!");
	}

	error = NRFJPROG_rtt_read_channel_count(&down, &up);
	if (error != SUCCESS)
	{
		R_FATAL("Cannot fetch RTT channel count!");
	}

	for (i = 0; i < up; i++)
	{
		unsigned int size;
		char name[32 + 1];

		error = NRFJPROG_rtt_read_channel_info(i, UP_DIRECTION, name, sizeof(name), &size);
		if (error != SUCCESS)
		{
			PRINT_ERROR("Cannot fetch RTT channel info!");
            continue;
		}

		if (size != 0)
			PRINT_INFO("RTT[%i] UP:\t\"%s\" (size: %u bytes)", i, name, size);

        if (strcmp(name, "NrfLiteTrace") == 0)
        {
            channel_up = i;
		}
	}

    if (channel_up < 0)
    {
        R_FATAL("Cannot find ethernet RTT channel.");
    }
    return true;
}


uint32_t rtt_read(char * data, uint32_t data_len)
{
    uint32_t read_len = 0;
    nrfjprogdll_err_t err = NRFJPROG_rtt_read(channel_up, data, data_len, &read_len);
    if (err != SUCCESS)
    {
        R_FATAL("RTT READ ERROR: %d", err);
    }
    return read_len;
}
