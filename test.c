
#include <stdio.h>
#include <stdlib.h>

#include "SEGGER_RTT.h"
#include "SEGGER_SYSVIEW.h"
#include "SEGGER_SYSVIEW_Conf.h"
#include "SEGGER_SYSVIEW_ConfDefaults.h"

#include <kernel.h>

#include "debug/rtt_lite_trace.h"

#include "events.h"

#include <search.h>

struct _mock_kernel _kernel;
struct _mock_timer *NRF_TIMER0;
uint8_t _mock_isr_number;
k_tid_t _mock_idle_thread;
k_tid_t _mock_current_thread;

//SEGGER_SYSVIEW_RTT_CHANNEL

struct thread_info
{
	uint32_t id; /* must be first field */
	uint32_t stack_base;
	uint32_t stack_size;
	uint32_t priority;
	uint8_t buffer[32 * 3 + 1];
	uint32_t buffer_size;
	char* name;
};

void *thread_tree = NULL;

#define ID_TRACE_CORRUPTED 0x100

#define INFO(text, ...) printf("\x1b[36m" text "\x1b[0m", ##__VA_ARGS__)

void set_time(uint32_t t);
void set_isr(uint32_t x);

void signal_corrupted()
{
	// TODO: Translate into macro with VAR_ARGS that calls RecordVoid and prints formated text (client formated)
	SEGGER_SYSVIEW_RecordVoid(ID_TRACE_CORRUPTED);
}

int thread_tree_id_compare(const void * a, const void * b)
{
	struct thread_info * va = (struct thread_info *)a;
	struct thread_info * vb = (struct thread_info *)b;
	return va->id < vb->id ? -1 : va->id > vb->id ? 1 : 0;
}

struct thread_info* find_thread(uint32_t id)
{
	return *(struct thread_info**)tfind(&id, &thread_tree, thread_tree_id_compare);
}

void send_tharead_info(struct thread_info* info)
{
	SEGGER_SYSVIEW_TASKINFO task_info;
	task_info.TaskID = info->id;
	task_info.Prio = info->priority;
	task_info.StackBase = info->stack_base;
	task_info.StackSize = info->stack_size;
	task_info.sName = info->name;
	SEGGER_SYSVIEW_SendTaskInfo(&task_info);
}

struct thread_info* get_thread(uint32_t id)
{
	void * p = tfind(&id, &thread_tree, thread_tree_id_compare);
	if (!p) {
		struct thread_info* r = (struct thread_info*)calloc(1, sizeof(struct thread_info));
		r->id = id;
		p = tsearch(r, &thread_tree, thread_tree_id_compare);
		r = *(struct thread_info**)p;
	}
	return *(struct thread_info**)p;
}

void ev_system_reset(uint32_t time_stamp)
{
	INFO("--- SYSTEM RESET ---\n");
}

void ev_thread_create(uint32_t thread_id, uint32_t time_stamp)
{
	set_time(time_stamp);
	SEGGER_SYSVIEW_OnTaskCreate(thread_id);
	INFO("Thread 0x%08X: Created\n", thread_id);
}


void ev_thread_priority(uint32_t thread_id, uint32_t priority)
{
	struct thread_info* info = get_thread(thread_id);
	info->priority = priority;
	send_tharead_info(info);
}

struct thread_info* ev_thread_info_data(uint32_t thread_id, uint8_t* data, bool first)
{
	struct thread_info* info = get_thread(thread_id);
	if (first) {
		info->buffer_size = 0;
	}
	if (info->buffer_size + 4 <= sizeof(info->buffer)) {
		memcpy(&info->buffer[info->buffer_size], data, 3);
		info->buffer_size += 3;
	} else {
		signal_corrupted();
	}
	INFO("Thread 0x%08X: info chunk, total %d\n", thread_id, info->buffer_size);
	return info;
}

void ev_thread_info_end(uint32_t thread_id, uint8_t* data)
{
	struct thread_info* info = ev_thread_info_data(thread_id, data, false);
	info->buffer[info->buffer_size] = 0;
	if (info->buffer_size < 8) {
		signal_corrupted();
		return;
	}
	info->stack_size = 0;
	memcpy(&info->stack_size, &info->buffer[0], 3);
	memcpy(&info->stack_base, &info->buffer[3], 4);
	info->priority = info->buffer[7];
	info->name = (char *)&info->buffer[8];
	send_tharead_info(info);
	INFO("Thread 0x%08X: prio=%d, stack=%d@0x%08X, name='%s'\n", thread_id, info->priority, info->stack_size, info->stack_base, info->name);
}

void buffer_overflow(uint32_t count, uint32_t time_stamp)
{
	set_time(time_stamp);
	SEGGER_SYSVIEW_RecordU32(SYSVIEW_EVTID_OVERFLOW, count);
}

static void parse_event(uint32_t first, uint32_t param)
{
	uint32_t event = 0;
	static uint32_t time_stamp = 0;
	uint32_t additional = 0;
	uint32_t isr = 0;
	if (first < 0x10000000)
	{
		event = first & 0xFF000000u;
		additional = first & 0x00FFFFFFu;
		printf("0x%08X: param = 0x%08X 0x%06X\n", event, param, additional);
	}
	else if (first < 0x80000000u)
	{
		event = first & 0xFF000000u;
		time_stamp = first & 0x00FFFFFFu;
		printf("0x%08X: param = 0x%08X, time = 0x%08X\n", event, param, time_stamp);
	}
	else
	{
		event = first & 0x80000000u;
		time_stamp = first & 0x00FFFFFFu;
		isr = (event >> 24) & 0x7F;
		printf("0x%08X: param = 0x%08X, time = 0x%08X, isr=%d\n", event, param, time_stamp, isr);
	}
	switch (event)
	{
	case EV_SYSTEM_RESET:
		ev_system_reset(time_stamp);
		break;

	case EV_THREAD_CREATE:
		ev_thread_create(param, time_stamp);
		break;

	case EV_THREAD_PRIORITY:
		ev_thread_priority(param, additional);
		break;

	case EV_THREAD_INFO_BEGIN:
		ev_thread_info_data(param, (uint8_t*)&additional, true);
		break;

	case EV_THREAD_INFO_NEXT:
		ev_thread_info_data(param, (uint8_t*)&additional, false);
		break;

	case EV_THREAD_INFO_END:
		ev_thread_info_end(param, (uint8_t*)&additional);
		break;
	
	case EV_SYNCH_START:
		INFO("Sync\n");
		break;

	case EV_THREAD_READY:
		set_time(time_stamp);
		SEGGER_SYSVIEW_OnTaskStartReady(param);
		INFO("Thread 0x%08X: Ready\n", param);
		break;

	case EV_THREAD_START:
		set_time(time_stamp);
		SEGGER_SYSVIEW_OnTaskStartExec(param);
		INFO("Thread 0x%08X: Exec\n", param);
		break;
	
	case EV_THREAD_STOP:
		set_time(time_stamp);
		SEGGER_SYSVIEW_OnTaskStopExec();
		INFO("Thread [current]: Stop\n");
		break;
	
	case EV_SYS_CALL:
		set_time(time_stamp);
		SEGGER_SYSVIEW_RecordVoid(param);
		INFO("Call 0x%08X\n", param);
		break;
	
	case EV_SYS_END_CALL:
		set_time(time_stamp);
		SEGGER_SYSVIEW_RecordEndCall(param);
		INFO("Exit 0x%08X\n", param);
		break;
	
	case EV_ISR_ENTER:
		set_time(time_stamp);
		set_isr(isr);
		SEGGER_SYSVIEW_RecordEnterISR();
		INFO("ISR %d: Enter\n", isr);
		break;
	
	case EV_ISR_EXIT:
		set_time(time_stamp);
		SEGGER_SYSVIEW_RecordExitISR();
		INFO("ISR [current]: Exit\n");
		break;

	case EV_THREAD_PEND:
		set_time(time_stamp);
		SEGGER_SYSVIEW_OnTaskStopReady(param, 0);
		INFO("Thread 0x%08X: Pending\n", param);
		break;

	case EV_IDLE:
		set_time(time_stamp);
		SEGGER_SYSVIEW_OnIdle();
		INFO("Idle\n");
		break;

	case EV_BUFFER_OVERFLOW:
		INFO("\x1b[31m" "--- BUFFER OVERFLOW ---\n");
		INFO("Lost %d events\n", param);
		buffer_overflow(param, time_stamp);
		break;

	case EV_BUFFER_CYCLE:
		INFO("Cycle with counter %d\n", param);
		break;
	default:
		printf("0x%08X\n\n", event);
		exit(1);
		break;
	}
}

void parseEvents()
{
	static u32_t buffer[65536];
	u32_t *ptr = buffer;
	uint32_t NumBytes = SEGGER_RTT_ReadUpBufferNoLock(CONFIG_RTT_LITE_TRACE_RTT_CHANNEL, buffer, sizeof(buffer));
	// TODO: Magic bytes for stream synchronization in case some byte are lost during reset/reconnect:
	/*
		8 bytes in range 0x78..0x7F
		SYSTEM RESET event with param = magic bytes
		e.g.
		0x78 0x7E 0x7B 0x7F   0x7C 0x7D 0x7A 0x79
		TTTT TTTT TTTT 0x11   0xe2 0x55 0x6c 0xcb

		Additionally when system goes into idle:
		0x78 0x7E 0x7B 0x7F   0x7C 0x7D 0x7A 0x79
		TTTT TTTT TTTT 0x11   CCCC CCCC CCCC CCCT

	*/
	while (NumBytes >= 8)
	{
		parse_event(ptr[0], ptr[1]);
		ptr += 2;
		NumBytes -= 8;
	}
	if (NumBytes)
	{
		puts("Invalid number of bytes in RTT buffer!");
		exit(1);
	}
}

void test1()
{
	static struct _mock_timer t;
	static struct k_thread idle;
	NRF_TIMER0 = &t;
	_mock_idle_thread = &idle;
	_kernel.threads = &idle;
	idle.next_thread = NULL;
	idle.name = "IDLE";
	idle.base.prio = 0;
	idle.stack_info.size = 12345;
	idle.stack_info.start = (u32_t)(&idle) + 4;
	_mock_isr_number = 0;
	_mock_current_thread = &idle;
	t.CC[0] = 0x123;
	SEGGER_RTT_Init();
	sys_trace_thread_create(&idle);
	parseEvents();
}


void test()
{
        FILE* out = fopen("out.SVDat", "a");
	SEGGER_RTT_Init();
    	SEGGER_SYSVIEW_Conf();
	FILE* f = fopen("/home/doki/test.log", "r");
	bool start = true;
	uint8_t buf[65536];
	while (!feof(f))
	{
		int n = fread(buf, 1, sizeof(buf), f);
		if (n == 0) break;
		if (n < 0) exit(12);
		uint8_t* ptr = buf;
		if (start)
		{
			int i;
			for (i = 0; i < n - 4; i++)
			{
				if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] != '#')
				{
					ptr = &buf[i + 2];
					break;
				}
			}
			start = false;
		}
		uint32_t tab[2];
		while (n >= 8)
		{
			memcpy(tab, ptr, 8);
			n -= 8;
			ptr += 8;
			parse_event(tab[0], tab[1]);
			static uint8_t buffer[65536];
			uint32_t NumBytes = SEGGER_RTT_ReadUpBufferNoLock(SEGGER_SYSVIEW_GetChannelID(), buffer, sizeof(buffer));
			fwrite(buffer, 1, NumBytes, out);
		}
	}
	fclose(f);
	fclose(out);
}


#if DO_TESTING
int main()
{
	test();
	return 0;
}
#endif
