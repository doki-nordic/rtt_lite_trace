/*
 * Copyright (c) 2019 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <getopt.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <dlfcn.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> 

#include <string>
#include <functional>
#include <vector>
#include <deque>
#include <map>

#include "SEGGER_RTT.h"
#include "SEGGER_SYSVIEW.h"


static void sendSystemDesc(void)
{
	SEGGER_SYSVIEW_SendSysDesc("N=Test App,D=Cortex-M33");
#	define TRACE_MARK(id, name)
#	define TRACE_CALL(id, name) SEGGER_SYSVIEW_SendSysDesc("I#" #id "=" #name);
#	include "ltrace_ids.h"
#	undef TRACE_MARK
#	undef TRACE_CALL
}

static int getInterrupt(uint32_t inputId)
{
#	define TRACE_MARK(id, name)
#	define TRACE_CALL(id, name) if (id == inputId) return id;
#	include "ltrace_ids.h"
#	undef TRACE_MARK
#	undef TRACE_CALL
	return -1;
}

static const char* getNameFromId(uint32_t inputId)
{
#	define TRACE_MARK(id, name) if (id == inputId) return #name;
#	define TRACE_CALL(id, name) if (id == inputId) return #name;
#	include "ltrace_ids.h"
#	undef TRACE_MARK
#	undef TRACE_CALL
	return NULL;
}

uint32_t *markers;
uint32_t markersCount;
int channelID;
uint64_t timeAbsolute = 0;
int interruptNumber;

void SEGGER_SYSVIEW_Conf(void)
{
    static SEGGER_SYSVIEW_OS_API api = {
        .pfGetTime = NULL,
        .pfSendTaskList = NULL,
    };
    SEGGER_SYSVIEW_Init(64000000, 64000000, &api, sendSystemDesc);
    SEGGER_SYSVIEW_SetRAMBase(0);
    channelID = SEGGER_SYSVIEW_GetChannelID();
    SEGGER_SYSVIEW_Start();
}

static uint8_t buffer[65536];
FILE* output;

void outputRTT() {
    uint32_t n = SEGGER_RTT_ReadUpBufferNoLock(channelID, buffer, sizeof(buffer));
	if (n > 0) {
		fwrite(buffer, 1, n, output);
	}
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <file>\n", argv[0]);
		return 1;
	}

	char* outName = new char[strlen(argv[1]) + 100];
	strcpy(outName, argv[1]);
	strcat(outName, ".SVDat");

	output = fopen(outName, "wb");
	if (!output) {
		fprintf(stderr, "Cannot open '%s' file.\n", argv[1]);
		return 4;
	}

	SEGGER_SYSVIEW_Conf();

	FILE* input = fopen(argv[1], "rb");
	if (!input) {
		fprintf(stderr, "Cannot open '%s' file.\n", argv[1]);
		return 2;
	}

	fseek(input, 0, SEEK_END);
	markersCount = ftell(input) / sizeof(uint32_t);
	printf("Found %d markers\n", markersCount);
	fseek(input, 0, SEEK_SET);
	markers = new uint32_t[markersCount];
	int n = fread(markers, 1, markersCount * sizeof(uint32_t), input);
	fclose(input);
	if (n != sizeof(uint32_t) * markersCount) {
		fprintf(stderr, "Error reading file.\n");
		return 3;
	}

	uint32_t lastTime = 0;
	uint32_t smallestDelta = 64000;

	for (int i = 0; i < markersCount; i++) {
		uint32_t id = markers[i] >> 24;
		uint32_t time = 0xFFFFFF - (markers[i] & 0xFFFFFF);
		uint32_t delta = time - lastTime;
		lastTime = time;
		if (delta >= 0x80000000) delta = (delta + 0x01000000) & 0xFFFFFF;
		//printf("DELTA = %d\n", delta);
		if (delta < smallestDelta) {
			smallestDelta = delta;
		}
	}

	uint32_t overhead = smallestDelta;// * 95 / 100;

	timeAbsolute = 0;
	lastTime = 0;

	printf("Overhead: %d ticks = %.3fus\n", overhead, (double)overhead / 64.0);

	for (int i = 0; i < markersCount; i++) {
		outputRTT();
		uint32_t id = markers[i] >> 24;
		uint32_t time = 0xFFFFFF - (markers[i] & 0xFFFFFF);
		uint32_t delta = time - lastTime;
		lastTime = time;
		if (delta >= 0x80000000) delta = (delta + 0x01000000) & 0xFFFFFF;
		timeAbsolute += delta - overhead;
		printf("%10lld   ", (long long)timeAbsolute);
		if (id == 254) {
			printf("%10.6f Return with error\n", (double)timeAbsolute / 64000.0);
			SEGGER_SYSVIEW_Error("Return Error");
			SEGGER_SYSVIEW_RecordExitISR();
			continue;
		} else if (id == 255) {
			printf("%10.6f Return with success\n", (double)timeAbsolute / 64000.0);
			SEGGER_SYSVIEW_RecordExitISR();
			continue;
		}
		const char* name = getNameFromId(id);
		interruptNumber = getInterrupt(id);
		if (interruptNumber >= 0) {
			printf("%10.6f Call %s\n", (double)timeAbsolute / 64000.0, name);
			SEGGER_SYSVIEW_RecordEnterISR();
			continue;
		}
		const char* markerName = getNameFromId(id);
		if (markerName != NULL) {
			printf("%10.6f Marker %s\n", (double)timeAbsolute / 64000.0, name);
			SEGGER_SYSVIEW_Print(markerName);
			continue;
		}
		fprintf(stderr, "Unknown marker: %u. Invalid or corrupted input file.\n", id);
		return 4;
	}

	outputRTT();

	fclose(output);

	return 0;
}

extern "C"
U32 SEGGER_SYSVIEW_X_GetTimestamp()
{
	return timeAbsolute;
}


extern "C"
U32 SEGGER_SYSVIEW_X_GetInterruptId(void)
{
    return interruptNumber;
}
