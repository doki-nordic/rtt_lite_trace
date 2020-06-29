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
#include <nrfjprog.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <getopt.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <dlfcn.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> 


#include "options.h"
#include "logs.h"
#include "rtt.h"

#include "SEGGER_RTT.h"
#include "SEGGER_SYSVIEW.h"


#include "common.h"



#define FATAL(text, ...) do { fprintf(stderr, "FATAL ERROR!!!\n" text "\n", ##__VA_ARGS__); exit(1); } while (0)


int parse_header(const char *str, int len)
{
	const char *start = str;

	if (*str != '#')
		return 0;
	str++;
	while (*str != '\r') {
		if (*str < ' ' || *str >= '\x7F')
			return 0;
		str++;
	}
	str++;
	if (*str != '\n')
		return 0;
	str++;

	return str - start;
}


#include <string>
#include <functional>
#include <vector>

class LogReader
{
public:
	LogReader(const std::string& file_name);
	~LogReader();
	bool readEvent(uint32_t &event, uint32_t &param);
private:
	FILE* f;
	std::vector<std::string> headers;
	int64_t data_start;
	int64_t data_end;
	int64_t data_pos;

	void readHeaders();
	void readFooter();
	static int parseHeader(const char *str);
	static int parseFooter(const char *str, size_t len);
	void synchronize();
	int read(void* buffer, int length);
	int seek(int offset);
};

int LogReader::read(void* buffer, int length)
{
	int64_t read_end = data_pos + length;
	if (read_end > data_end) {
		read_end = data_end;
	}
	int res = fread(buffer, 1, read_end - data_pos, f);
	if (res >= 0) {
		data_pos += res;
	}
	return res;
}

int LogReader::seek(int offset)
{
	data_pos += (int64_t)offset;
	return fseek(f, offset, SEEK_CUR);
}

LogReader::LogReader(const std::string& file_name)
{
	f = fopen(file_name.c_str(), "rb");
	if (f == NULL) {
		FATAL("Cannot open input file");
	}
	readHeaders();
}

LogReader::~LogReader()
{
	if (f != NULL) {
		fclose(f);
	}
}

void LogReader::readHeaders()
{
	char line[1024 + 1];
	char c;
	int read_len;
	int header_len;
	int first_len = -1;

	do {
		read_len = fread(line, 1, sizeof(line) - 1, f);
		if (read_len < 0) {
			FATAL("File read error!");
		} else if (read_len == 0) {
			FATAL("Empty file!");
		}
		if (first_len < 0) {
			first_len = read_len;
		}
		line[read_len] = 0;
		header_len = parseHeader(line);
		if (fseek(f, header_len - read_len, SEEK_CUR) < 0) {
			FATAL("File read error!");
		}
		if (header_len > 2) {
			std::string h(line, header_len - 2);
			headers.push_back(h);
			fprintf(stderr, "FILE HEADER: %s\n", h.c_str());
		}
	} while (header_len > 0);

	data_start = ftello64(f);
	if (data_start < 0) {
		FATAL("File read error %d!", (int)data_start);
	}

	if (fseek(f, -first_len, SEEK_END) < 0) {
		FATAL("File read error!");
	}

	read_len = fread(&line[1], 1, first_len, f);
	line[0] = 0;
	if (read_len != first_len) {
		FATAL("File read error!");
	}
	header_len = parseFooter(line, read_len + 1);
	if (header_len > 4) {
		std::string h(&line[read_len + 1 - header_len + 2], header_len - 4);
		headers.push_back(h);
		fprintf(stderr, "FILE FOOTER: %s\n", h.c_str());
	}

	data_end = ftello64(f);
	if (data_end < 0) {
		FATAL("File read error %d!", (int)data_end);
	}
	data_end -= header_len;

	if (fseek(f, data_start, SEEK_SET) < 0) {
		FATAL("File read error!");
	}
	data_pos = data_start;
}

int LogReader::parseFooter(const char *str, size_t len)
{
	const char *ptr = str + len;
	const char *start = ptr;

	ptr--;
	if (*ptr != '\n')
		return 0;
	ptr--;
	if (*ptr != '\r')
		return 0;
	ptr--;
	while (*ptr != '\n') {
		if (*ptr < ' ' || *ptr >= '\x7F')
			return 0;
		ptr--;
	}
	if (ptr[1] != '#')
		return 0;
	ptr--;
	if (*ptr != '\r')
		return 0;

	return start - ptr;
}

int LogReader::parseHeader(const char *str)
{
	const char *start = str;

	if (*str != '#')
		return 0;
	str++;
	while (*str != '\r') {
		if (*str < ' ' || *str >= '\x7F')
			return 0;
		str++;
	}
	str++;
	if (*str != '\n')
		return 0;
	str++;

	return str - start;
}

bool LogReader::readEvent(uint32_t &event, uint32_t &param)
{
	int len;
	uint32_t buf[2];
	uint32_t id;

	do {
		len = read(buf, sizeof(buf));
		if (len < 0) {
			FATAL("Input file read error!");
		} else if (len < sizeof(buf)) {
			return false;
		}

		id = buf[0] & 0xFF000000;
		if (id & 0x80000000) {
			id = 0x80000000;
		}

		switch (id)
		{
		default:
			if (id < _RTT_LITE_TRACE_EV_USER_FIRST || id > _RTT_LITE_TRACE_EV_USER_LAST) {
				// corrupted data - synchronize the stream
				break;
			}
			// no break - fall into valid event
		case EV_BUFFER_CYCLE:
		case EV_THREAD_PRIORITY:
		case EV_THREAD_INFO_BEGIN:
		case EV_THREAD_INFO_NEXT:
		case EV_THREAD_INFO_END:
		case EV_FORMAT:
		case EV_BUFFER_BEGIN:
		case EV_BUFFER_NEXT:
		case EV_BUFFER_END:
		case EV_BUFFER_BEGIN_END:
		case EV_RES_NAME:
		case EV_SYSTEM_RESET:
		case EV_BUFFER_OVERFLOW:
		case EV_IDLE:
		case EV_THREAD_START:
		case EV_THREAD_STOP:
		case EV_THREAD_CREATE:
		case EV_THREAD_SUSPEND:
		case EV_THREAD_RESUME:
		case EV_THREAD_READY:
		case EV_THREAD_PEND:
		case EV_SYS_CALL:
		case EV_SYS_END_CALL:
		case EV_ISR_EXIT:
		case EV_PRINTF:
		case EV_PRINT:
		case EV_ISR_ENTER:
		case _RTT_LITE_TRACE_EV_MARK_START:
		case _RTT_LITE_TRACE_EV_MARK:
		case _RTT_LITE_TRACE_EV_MARK_STOP:
			event = buf[0];
			param = buf[1];
			return true;
		case EV_SYNC_FIRST:
			if (buf[0] == (EV_SYNC_FIRST | SYNC_ADDITIONAL) && buf[1] == SYNC_PARAM) {
				continue; // valid sync - skip it and go to the next event
			}
			break; // invalid sync - synchronize the stream
		}

		synchronize();

		event = EV_INTERNAL_CORRUPTED;
		param = 0;
		return true;
		
	} while (true);
}


void LogReader::synchronize()
{
	uint8_t buffer[1024];
	int res;
	int len;
	int total = 0;
	
	res = seek(-7);
	if (res < 0) {
		FATAL("Input file read error %d!", res);
	}

	fprintf(stderr, "Stream corrupted. Synchronizing...\n");

	len = 0;
	do {
		res = read(&buffer[len], sizeof(buffer) - len);
		if (res < 0) {
			FATAL("Input file read error %d!", res);
		}

		len += res;

		if (len < 8) {
			return;
		}

		for (int i = 0; i <= len - 8; i++) {
			if (memcmp(&buffer[i], "y~|x{z}\x7F", 8) == 0) {
				res = seek(i - len + 8);
				if (res < 0) {
					FATAL("Input file read error %d!", res);
				}
				fprintf(stderr, "Stream synchronized after %d bytes\n", total + i);
				return;
			}
		}

		total += res;

		memmove(&buffer[0], &buffer[len - 7], 7);
		len = 7;

	} while (true);
}

int main()
{

	LogReader reader("./test.log");

	uint32_t event;
	uint32_t param;
	int i = 0;

	while (reader.readEvent(event, param)) {
		printf("0x%08X  0x%08X\n", event, param);
		i++;
		if (i == 20) break;
	}

	return 0;
}



#if 0

#define VAL_UNKNOWN ((uint32_t)-1)
#define INDEX_UNKNOWN ((size_t)-1)
#define OVERFLOW_UNKNOWN 1000000000

struct state_info
{
	uint32_t buffer_counter;
	size_t last_cycle;
};


struct session
{
	size_t begin;
	size_t end;
	uint64_t time_offset;
};

struct state_info state;

struct Session *sessions;
size_t sessions_size;
size_t sessions_len;

void parse_bufer_cycle(uint64_t time, uint32_t param)
{
	if (param & 1) {
		uint32_t counter = param >> 1;
		if (state.buffer_counter == VAL_UNKNOWN) {
			add_event(state.time, EV_BUFFER_OVERFLOW, 0, OVERFLOW_UNKNOWN);
			events_len = state.last_cycle;
		} else if (counter < state.buffer_counter + 1) {
			add_event(state.time, EV_BUFFER_OVERFLOW, 0, OVERFLOW_UNKNOWN);
			events_len = state.last_cycle;
			do_reset();
			add_event(state.time, EV_BUFFER_OVERFLOW, 0, OVERFLOW_UNKNOWN);
		} else if (counter > state.buffer_counter + 1) {
			add_event(state.time, EV_BUFFER_OVERFLOW, 0, OVERFLOW_UNKNOWN);
			events_len = state.last_cycle;
		}
		state.buffer_counter = counter;
		state.last_cycle = events_len;
	} else {
		uint32_t min_size = param;
		if (current_session->min_size > min_size) {
			current_session->min_size = min_size;
		}
	}
}

void read_commands(FILE* f)
{
	int res;
	uint32_t buf[2];
	uint32_t cmd;
	uint32_t additional;
	uint32_t param;

	while (true) {
		res = fread(&buf, 0, sizeof(buf), f);

		if (res < 0) {
			FATAL("File read error %d", res);
		} else if (res != sizeof(buf)) {
			break;
		}

		cmd = buf[0] >> 24;
		if (cmd & 0x80) {
			cmd = 0x80;
		}
		additional = cmd & 0xFFFFFF;
		param = buf[1];
		

		switch (cmd) {
		
		case EV_BUFFER_CYCLE:
			parse_bufer_cycle(param);
			break;

		case EV_THREAD_PRIORITY:
		case EV_THREAD_INFO_BEGIN:
		case EV_THREAD_INFO_NEXT:
		case EV_THREAD_INFO_END:
		case EV_FORMAT:
		case EV_BUFFER_BEGIN:
		case EV_BUFFER_NEXT:
		case EV_BUFFER_END:
		case EV_BUFFER_BEGIN_END:
		case EV_RES_NAME:
		case EV_SYSTEM_RESET:
		case EV_BUFFER_OVERFLOW:
		case EV_IDLE:
		case EV_THREAD_START:
		case EV_THREAD_STOP:
		case EV_THREAD_CREATE:
		case EV_THREAD_SUSPEND:
		case EV_THREAD_RESUME:
		case EV_THREAD_READY:
		case EV_THREAD_PEND:
		case EV_SYS_CALL:
		case EV_SYS_END_CALL:
		case EV_ISR_EXIT:
		case EV_PRINTF:
		case EV_PRINT:
		case EV_SYNC_FIRST:
		case EV_ISR_ENTER:
		default:
			// TODO: corrupted
			FATAL("TODO: corrupted");
		}

	}
}

#endif