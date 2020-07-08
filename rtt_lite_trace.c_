/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <debug/rtt_lite_trace.h>
#include <ksched.h>
#include <SEGGER_RTT.h>
#include <nrfx_timer.h>
#include <stdarg.h>


/*
 * Events with 24-bit additional parameter.
 * Bit format:
 *     0000 eeee pppp pppp pppp pppp pppp pppp
 *     e - event numer
 *     p - additional parameter
 */

/** @brief Event indicating cycle of RTT buffer.
 *
 * This event is placed at the end of RTT buffer, so it is send each time
 * RTT buffer cycles back to the beginning.
 * @param additional Unused
 * @param param      If CONFIG_RTT_LITE_TRACE_FAST_OVERFLOW_CHECK is set then
 *         it contains 1 on bit 0 and RTT buffer cycle counter on the rest of
 *         bits. Else it contains minimum free space of RTT buffer if
 *         CONFIG_RTT_LITE_TRACE_BUFFER_STATS is set.
 */
#define EV_BUFFER_CYCLE 0x01000000

/** @brief Event reporting change of thread priority.
 *
 * @param additional Thread priority
 * @param param      Thread id.
 */
#define EV_THREAD_PRIORITY 0x02000000

/** @brief Event that starts series of thread information events.
 * 
 * Thread information is dived into 3-byte parts to fit into events that are
 * constant size and also contains thread id. It have following structure:
 * 
 * +---------+----------------------------------------------------+
 * | Size    | Description                                        |
 * | (bytes) |                                                    |
 * +=========+====================================================+
 * | 3       | Stack size or zero if system does not provide such |
 * |         | information.                                       |
 * +---------+----------------------------------------------------+
 * | 4       | Stack base address or zero if system does not      |
 * |         | provide such information.                          |
 * +---------+----------------------------------------------------+
 * | 1       | Thread priority.                                   |
 * +---------+----------------------------------------------------+
 * | 0..n    | Thread name string or empty string if system does  |
 * |         | not provide such information. It is NOT null       |
 * |         | terminated.                                        |
 * +---------+----------------------------------------------------+
 * | 0..2    | Zero padding to fit into 3-byte parts.             |
 * +---------+----------------------------------------------------+
 *
 * @param additional First 3 bytes of thread information.
 * @param param      Thread id.
 */
#define EV_THREAD_INFO_BEGIN 0x03000000

/** @brief Event that sends next part of thread information.
 *
 * @param additional Next 3 bytes of thread information.
 * @param param      Thread id.
 */
#define EV_THREAD_INFO_NEXT 0x04000000

/** @brief Event that sends last part of thread information.
 *
 * @param additional Last 3 bytes of thread information.
 * @param param      Thread id.
 */
#define EV_THREAD_INFO_END 0x05000000

/** @brief Sends format information for formatted text output.
 * 
 * Buffer is send immediately after this event. It contains null terminated
 * format string at the beginnign and null terminated format arguments list
 * after that. The list contains type of each argument on each byte, see
 * FORMAT_ARG_xyz definitions.
 *
 * @param additional Unused.
 * @param param      Format id.
 */
#define EV_FORMAT 0x06000000

/** @brief Start sending buffer.
 * 
 * Buffers are send immediately after specific events to provide more data.
 * In the same time two thread may send buffers, so the receiving part have to
 * assemble buffer back separately for each thread.
 * 
 * @param additional Next 3 bytes of the buffer.
 * @param param      First 4 bytes of the buffer.
 */
#define EV_BUFFER_BEGIN 0x07000000

/** @brief Continue sending buffer.
 * 
 * @param additional Next 3 bytes of the buffer that follows bytes from param.
 * @param param      Next 4 bytes of the buffer.
 */
#define EV_BUFFER_NEXT 0x08000000

/** @brief Continue sending buffer.
 * 
 * @param additional Next 2 bytes of the buffer that follows bytes from param.
 *         Byte 3 contains number of buffer bytes that are actually in this
 *         event, because last part of the buffer may not fill this event
 *         entirety.
 * @param param      Next 4 bytes of the buffer.
 */
#define EV_BUFFER_END 0x09000000

/** @brief Send small buffer in one pice.
 * 
 * This event is send when the buffer is smaller than 7 bytes.
 *
 * @param additional Next 2 bytes of the buffer.
 *         Byte 3 contains number of buffer bytes that are actually in this
 *         event, because the buffer may not fill this event entirety.
 * @param param      First 4 bytes of the buffer.
 */
#define EV_BUFFER_BEGIN_END 0x0A000000

/** @brief Event that gives a name of a resource for pritty printing.
 * 
 * Buffer containing resource name is send immediately after this event.
 * 
 * @param additional  Unused.
 * @param param       Resource id which is usually pointer to it.
 */
#define EV_RES_NAME 0x0B000000


/*
 * Events with 24-bit time stamp.
 * Bit format:
 *     0eee eeee tttt tttt tttt tttt tttt tttt
 *     e - event numer
 *     t - time stamp
 */

/** @brief Event send as the first event after the system reset.
 * 
 * @param param      Unused.
 */
#define EV_SYSTEM_RESET 0x11000000

/** @brief Event send if RTT buffer cannot fit next event.
 * 
 * @param param      Number of events that were skipped because of overflow.
 */
#define EV_BUFFER_OVERFLOW 0x12000000

/** @brief Event send when systems goes into idle.
 * 
 * @param param       The same as EV_BUFFER_CYCLE.
 */
#define EV_IDLE 0x13000000

/** @brief Event send when systems starts executing a thread.
 * 
 * @param param       Thread id.
 */
#define EV_THREAD_START 0x14000000

/** @brief Event send when systems stops executing a thread and goes to
 *  the scheduler.
 * 
 * @param param       Thread id.
 */
#define EV_THREAD_STOP 0x15000000

/** @brief Event send when systems creates a new thread.
 * 
 * @param param       New thread id.
 */
#define EV_THREAD_CREATE 0x16000000

/** @brief Event send when a thread was suspended.
 * 
 * @param param       New thread id.
 */
#define EV_THREAD_SUSPEND 0x17000000

/** @brief Event send when a thread was resumed.
 * 
 * @param param       New thread id.
 */
#define EV_THREAD_RESUME 0x18000000

/** @brief Event send when a thread is ready to be executed.
 * 
 * @param param       New thread id.
 */
#define EV_THREAD_READY 0x19000000

/** @brief Event send when a thread is peding.
 * 
 * @param param       New thread id.
 */
#define EV_THREAD_PEND 0x1A000000

/** @brief Event send when a specific system function was called.
 * 
 * @param param       Function id, see SYS_TRACE_ID_xyz for full list.
 */
#define EV_SYS_CALL 0x1B000000

/** @brief Event send when a specific system function returned.
 * 
 * @param param       Function id, see SYS_TRACE_ID_xyz for full list.
 */
#define EV_SYS_END_CALL 0x1C000000

/** @brief Event send when currently running ISR exits.
 * 
 * @param param       Unused.
 */
#define EV_ISR_EXIT 0x1D000000

/** @brief Event send to print when formated text.
 * 
 * Buffer is send immediately after this event. If format id is 0xFFFFFF it
 * contais format string and arguments list (see EV_FORMAT for details). After
 * that buffer contains arguments according to arguments list:
 *     * FORMAT_ARG_INT32 - 4 byte integer,
 *     * FORMAT_ARG_INT64 - 8 byte integer,
 *     * FORMAT_ARG_STRING - null terminated text string.
 * 
 * @param param       Bits 0:23 - Id of previously send format or
 *         0xFFFFFF if format will be contained in the following buffer.
 *         Bits 24:31 - Level of the message, see RTT_LITE_TRACE_LEVEL_xyz.
 */
#define EV_PRINTF 0x1E000000

/** @brief Event send to print a string.
 * 
 * If string is longer than 3 characters buffer is send immediately after this
 * event containign the rest of the string (not including null terminator).
 * 
 * @param param       First 4 characters of the string including null
 *         terminator.
 */
#define EV_PRINT 0x1F000000


/*
 * Events with 24-bit time stamp and 7-bit ISR number.
 * Bit format:
 *     eiii iiii tttt tttt tttt tttt tttt tttt
 *     e - event numer
 *     i - ISR number
 *     t - time stamp
 */

/** @brief Event send when ISR was started.
 * 
 * @param isr       Number of the ISR that was started.
 * @param param     Unused.
 */
#define EV_ISR_ENTER 0x80000000


#define FORMAT_ARG_END 0
#define FORMAT_ARG_INT32 1
#define FORMAT_ARG_INT64 2
#define FORMAT_ARG_STRING 3


/* RTT channel name used to identify transfer channel. */
#define CHANNEL_NAME "NrfLiteTrace"


#if defined CONFIG_RTT_LITE_TRACE_BUFFER_SIZE_512B
#define RTT_BUFFER_BYTES 512
#elif defined CONFIG_RTT_LITE_TRACE_BUFFER_SIZE_1KB
#define RTT_BUFFER_BYTES 1024
#elif defined CONFIG_RTT_LITE_TRACE_BUFFER_SIZE_2KB
#define RTT_BUFFER_BYTES 2048
#elif defined CONFIG_RTT_LITE_TRACE_BUFFER_SIZE_4KB
#define RTT_BUFFER_BYTES 4096
#elif defined CONFIG_RTT_LITE_TRACE_BUFFER_SIZE_8KB
#define RTT_BUFFER_BYTES 8192
#elif defined CONFIG_RTT_LITE_TRACE_BUFFER_SIZE_16KB
#define RTT_BUFFER_BYTES 16384
#elif defined CONFIG_RTT_LITE_TRACE_BUFFER_SIZE_32KB
#define RTT_BUFFER_BYTES 32768
#elif defined CONFIG_RTT_LITE_TRACE_BUFFER_SIZE_64KB
#define RTT_BUFFER_BYTES 65536
#else
#error CONFIG_RTT_LITE_TRACE_BUFFER_SIZE_xyz not defined!
#endif

#if defined CONFIG_RTT_LITE_TRACE_TIMER0
static const nrfx_timer_t timer = NRFX_TIMER_INSTANCE(0);
#define NRF_TIMER_INSTANCE NRF_TIMER0
#elif defined CONFIG_RTT_LITE_TRACE_TIMER1
static const nrfx_timer_t timer = NRFX_TIMER_INSTANCE(1);
#define NRF_TIMER_INSTANCE NRF_TIMER1
#elif defined CONFIG_RTT_LITE_TRACE_TIMER2
static const nrfx_timer_t timer = NRFX_TIMER_INSTANCE(2);
#define NRF_TIMER_INSTANCE NRF_TIMER2
#elif defined CONFIG_RTT_LITE_TRACE_TIMER3
static const nrfx_timer_t timer = NRFX_TIMER_INSTANCE(3);
#define NRF_TIMER_INSTANCE NRF_TIMER3
#elif defined CONFIG_RTT_LITE_TRACE_TIMER4
static const nrfx_timer_t timer = NRFX_TIMER_INSTANCE(4);
#define NRF_TIMER_INSTANCE NRF_TIMER4
#else
#error CONFIG_RTT_LITE_TRACE_TIMERx not defined!
#endif


#define RTT_BUFFER_INDEX (*(volatile unsigned int*) \
		(&_SEGGER_RTT.aUp[CONFIG_RTT_LITE_TRACE_RTT_CHANNEL].WrOff))
#define RTT_BUFFER_READ_INDEX (*(volatile unsigned int*) \
		(&_SEGGER_RTT.aUp[CONFIG_RTT_LITE_TRACE_RTT_CHANNEL].RdOff))
#define RTT_BUFFER_WORDS (RTT_BUFFER_BYTES / sizeof(u32_t))
#define RTT_BUFFER_INDEX_MASK (RTT_BUFFER_BYTES - 1)
#define RTT_BUFFER_U8(byte_index) (((volatile u8_t*) \
		(&rtt_buffer))[byte_index])
#define RTT_BUFFER_U32(byte_index) (*(volatile u32_t*) \
		(&RTT_BUFFER_U8(byte_index)))

#define INIT_SEND_BUFFER_CONTEXT { .used = 0, .data = { 0, EV_BUFFER_BEGIN } }


struct send_buffer_context {
	size_t used;
	u32_t data[2];
};


static u32_t rtt_buffer[RTT_BUFFER_WORDS + 2];


static ALWAYS_INLINE u32_t get_isr_number(void)
{
	return __get_IPSR();
}

static ALWAYS_INLINE u32_t get_time(void)
{
	NRF_TIMER_INSTANCE->TASKS_CAPTURE[0] = 1;
	return NRF_TIMER_INSTANCE->CC[0];
}

static ALWAYS_INLINE void send_event_inner(u32_t event, u32_t param, u32_t time,
		bool with_param)
{
	u32_t index;
	u32_t left;
	u32_t cnt;
	int key;

	event = event | time;

	key = irq_lock();

	index = RTT_BUFFER_INDEX;

	if (IS_ENABLED(CONFIG_RTT_LITE_TRACE_FAST_OVERFLOW_CHECK)) {

		RTT_BUFFER_U32(index) = event;
		if (with_param) {
			RTT_BUFFER_U32(index + 4) = param;
		}
		index = index + 8;
		if (index == RTT_BUFFER_BYTES) {
			RTT_BUFFER_U32(RTT_BUFFER_BYTES + 4) += 2;
			index = 0;
		}

	} else {

		left = (RTT_BUFFER_READ_INDEX - index - 1)
				& (RTT_BUFFER_INDEX_MASK & ~7);

		if (left <= 8) {
			if (left == 0) {
				cnt = (index - 4) & RTT_BUFFER_INDEX_MASK;
				RTT_BUFFER_U32(cnt)++;
				goto unlock_and_return;
			}
			event = EV_BUFFER_OVERFLOW | get_time();
			param = 1;
		}
		RTT_BUFFER_U32(index) = event;
		RTT_BUFFER_U32(index + 4) = param;
		index = (index + 8) & RTT_BUFFER_INDEX_MASK;

		if (IS_ENABLED(CONFIG_RTT_LITE_TRACE_BUFFER_STATS)) {
			left -= 8;
			if (left < RTT_BUFFER_U32(RTT_BUFFER_BYTES + 4)) {
				RTT_BUFFER_U32(RTT_BUFFER_BYTES + 4) = left;
			}
		}
	}

	RTT_BUFFER_INDEX = index;

unlock_and_return:
	irq_unlock(key);
}

static void send_event(u32_t event, u32_t param)
{
	send_event_inner(event, param, get_time(), true);
}

static void send_timeless(u32_t event, u32_t param)
{
	send_event_inner(event, param, 0, true);
}

static void send_short(u32_t event)
{
	send_event_inner(event, 0, get_time(), false);
}

static void send_thread_info(k_tid_t thread)
{
	u32_t param;
	u32_t size = 0;
	u32_t start = 0;
	const u8_t *name = (const u8_t *)k_thread_name_get(thread);
	u8_t prio = (u8_t)thread->base.prio;

#if defined(CONFIG_THREAD_STACK_INFO)
	size = thread->stack_info.size;
	start = thread->stack_info.start;
#endif /* CONFIG_THREAD_STACK_INFO */

	send_timeless(EV_THREAD_INFO_BEGIN | (size & 0xFFFFFF), (u32_t)thread);
	send_timeless(EV_THREAD_INFO_NEXT | (start & 0xFFFFFF), (u32_t)thread);
	param = (start >> 24) | ((u32_t)prio << 8);
	if (IS_ENABLED(CONFIG_THREAD_NAME) && name != NULL && name[0] != 0) {
		param |= (u32_t)name[0] << 16;
		name++;
		while (name[-1] != 0 && name[0] != 0 && name[1] != 0) {
			send_timeless(EV_THREAD_INFO_NEXT | param,
					(u32_t)thread);
			param = (u32_t)name[0] | ((u32_t)name[1] << 8)
					| ((u32_t)name[2] << 16);
			name += 3;
		}
		if (name[-1] != 0 && name[0] != 0) {
			send_timeless(EV_THREAD_INFO_NEXT | param,
					(u32_t)thread);
			param = (u32_t)name[0];
		}
	}
	send_timeless(EV_THREAD_INFO_END | param, (u32_t)thread);
}

static void send_periodic_thread_info(void)
{
	static struct k_thread *next_thread; /* zero-initialized after reset */
	struct k_thread *thread = NULL;

	for (thread = _kernel.threads; thread; thread = thread->next_thread) {
		if (thread == next_thread) {
			break;
		}
	}
	if (!thread) {
		thread = _kernel.threads;
	}
	next_thread = thread->next_thread;
	send_thread_info(thread);
}

static void initialize(void)
{
	static bool initialized; /* zero-initialized after reset */

	if (!initialized) {
		nrfx_timer_config_t timer_conf = NRFX_TIMER_DEFAULT_CONFIG;
		SEGGER_RTT_BUFFER_UP *up;

		/*
		 * Directly initialize RTT up channel to avoid unexpected traces
		 * before initialization.
		 */
		up = &_SEGGER_RTT.aUp[CONFIG_RTT_LITE_TRACE_RTT_CHANNEL];
		up->sName = CHANNEL_NAME;
		up->pBuffer = (char *)(&rtt_buffer[0]);
		up->SizeOfBuffer = sizeof(rtt_buffer);
		up->RdOff = 0u;
		up->WrOff = 0u;
		up->Flags = SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL;

		RTT_BUFFER_U32(RTT_BUFFER_BYTES) = EV_BUFFER_CYCLE;
		if (IS_ENABLED(CONFIG_RTT_LITE_TRACE_FAST_OVERFLOW_CHECK)) {
			RTT_BUFFER_U32(RTT_BUFFER_BYTES + 4) = 1;
		} else {
			RTT_BUFFER_U32(RTT_BUFFER_BYTES + 4) = RTT_BUFFER_BYTES;
		}

		timer_conf.frequency = NRF_TIMER_FREQ_16MHz;
		timer_conf.bit_width = NRF_TIMER_BIT_WIDTH_24;

		nrfx_timer_init(&timer, &timer_conf, NULL);
		nrfx_timer_enable(&timer);

		send_short(EV_SYSTEM_RESET);

		initialized = true;
	}
}

void sys_trace_thread_switched_in(void)
{
	k_tid_t thread = k_current_get();

	if (z_is_idle_thread_object(thread)) {
		send_event(EV_IDLE, RTT_BUFFER_U32(RTT_BUFFER_BYTES + 4));
	} else {
		send_event(EV_THREAD_START, (u32_t)thread);
	}
}

void sys_trace_thread_switched_out(void)
{
	send_short(EV_THREAD_STOP);
}

void sys_trace_isr_enter(void)
{
	if (IS_ENABLED(CONFIG_RTT_LITE_TRACE_IRQ)) {
		send_short(EV_ISR_ENTER | (get_isr_number() << 24));
	}
}

void sys_trace_isr_exit(void)
{
	if (IS_ENABLED(CONFIG_RTT_LITE_TRACE_IRQ)) {
		send_short(EV_ISR_EXIT);
	}
}

void sys_trace_idle(void)
{
	send_event(EV_IDLE, RTT_BUFFER_U32(RTT_BUFFER_BYTES + 4));
	if (IS_ENABLED(CONFIG_RTT_LITE_TRACE_THREAD_INFO)) {
		send_periodic_thread_info();
	}
}

void sys_trace_thread_priority_set(k_tid_t thread)
{
	u8_t prio = (u8_t)thread->base.prio;

	send_timeless(EV_THREAD_PRIORITY | (u32_t)prio, (u32_t)thread);
}

void sys_trace_thread_create(k_tid_t thread)
{
	initialize();
	send_event(EV_THREAD_CREATE, (u32_t)thread);
	if (IS_ENABLED(CONFIG_RTT_LITE_TRACE_THREAD_INFO)) {
		send_thread_info(thread);
	} else {
		sys_trace_thread_priority_set(thread);
	}
}

void sys_trace_thread_suspend(k_tid_t thread)
{
	send_event(EV_THREAD_SUSPEND, (u32_t)thread);
}

void sys_trace_thread_resume(k_tid_t thread)
{
	send_event(EV_THREAD_RESUME, (u32_t)thread);
}

void sys_trace_thread_ready(k_tid_t thread)
{
	send_event(EV_THREAD_READY, (u32_t)thread);
}

void sys_trace_thread_pend(k_tid_t thread)
{
	send_event(EV_THREAD_PEND, (u32_t)thread);
}

#ifdef CONFIG_RTT_LITE_TRACE_THREAD_INFO

void sys_trace_thread_name_set(k_tid_t thread)
{
	send_thread_info(thread);
}

#endif /* CONFIG_RTT_LITE_TRACE_THREAD_INFO */

#ifdef CONFIG_RTT_LITE_TRACE_SYNCHRO

void sys_trace_void(u32_t id)
{
	send_event(EV_SYS_CALL, id);
}

void sys_trace_end_call(u32_t id)
{
	send_event(EV_SYS_END_CALL, id);
}

#endif /* CONFIG_RTT_LITE_TRACE_SYNCHRO */

static void send_buffers(struct send_buffer_context *buf, const void *data,
		size_t size)
{
	size_t left;
	const u8_t *src = (u8_t *)data;
	u8_t *dst;

	while (size > 0) {
		dst = &((u8_t *)&buf->data[0])[buf->used];
		left = 7 - buf->used;
		if (size < left) {
			left = size;
		}
		memcpy(dst, src, left);
		buf->used += left;
		src += left;
		if (buf->used == 7) {
			send_timeless(buf->data[1], buf->data[0]);
			buf->used = 0;
			buf->data[1] &= 0x00FFFFFF;
			buf->data[1] |= EV_BUFFER_NEXT;
		}
	}
}

static void done_buffers(struct send_buffer_context *buf)
{
	u32_t event = buf->data[1] & 0xFF000000;

	buf->data[1] &= 0x0000FFFF;
	buf->data[1] |= buf->used << 16;
	if (event == EV_BUFFER_BEGIN) {
		buf->data[1] |= EV_BUFFER_BEGIN_END;
	} else {
		buf->data[1] |= EV_BUFFER_END;
	}
	send_timeless(buf->data[1], buf->data[0]);
	buf->used = 0;
	buf->data[1] = EV_BUFFER_BEGIN;
}

static u8_t parse_format_arg(const char **pp)
{
	static const u8_t table[] = {
		/*    !  "  #  $  %  &  '  (  )  *  +  ,  -  .  / */
		   9, 0, 0, 9, 9, 0, 0, 0, 0, 0, 0, 9, 0, 9, 9, 0,
		/* 0  9  2  3  4  5  6  7  8  9  :  ;  <  =  >  ? */
		   9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 0, 0, 0, 0, 0, 0,
		/* @  A  B  C  D  E  F  G  H  I  J  K  L  M  N  O */
		   0, 0, 0, 0, 0, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0,
		/* P  Q  R  S  T  U  V  W  X  Y  Z  [  \  ]  ^  _ */
		   0, 0, 0, 0, 0, 0, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0,
		/* `  a  b  c  d  e  f  g  h  i  j  k  l  m  n  o */
		   0, 0, 0, 7, 7, 2, 2, 2, 9, 7, 0, 0, 8, 0, 0, 7,
		/* p  q  r  s  t  u  v  w  x  y  z  {  |  }  ~    */
		   7, 0, 0, 3, 0, 7, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0,
	};
	const char *p = *pp;
	int long_count = 0;
	u8_t type;
	u8_t result = FORMAT_ARG_END;

	while (*p) {
		type = (*p < ' ' || *p > '~') ? 0 : table[*p - ' '];
		p++;
		if (type < 7) {
			result = type;
			break;
		} else if (type == 7) {
			result = (long_count <= 1) ? FORMAT_ARG_INT32 :
				(long_count == 2) ? FORMAT_ARG_INT64 :
				FORMAT_ARG_END;
			break;
		} else if (type == 8) {
			long_count++;
		}
	}
	*pp = p;
	return result;
}

static void parse_format_args(struct rtt_lite_trace_format *format)
{
	const char *p = format->text;
	u8_t *arg = &format->args[0];
	u8_t *arg_last = &format->args[sizeof(format->args) - 1];

	while (*p && arg < arg_last) {
		if (*p == '%') {
			p++;
			if (*p == '%') {
				p++;
			} else {
				*arg = parse_format_arg(&p);
				arg++;
			}
		} else {
			p++;
		}
	}
	*arg = FORMAT_ARG_END;
}

static void prepare_format(struct rtt_lite_trace_format *format)
{
	static volatile u32_t last_format_id; /* zero-initialied after reset */
	int key;

	parse_format_args(format);

	if (IS_ENABLED(CONFIG_RTT_LITE_TRACE_FORMAT_ONCE)) {
		key = irq_lock();
		format->id = last_format_id + 1;
		last_format_id = format->id;
		irq_unlock(key);
	} else {
		format->id = 0x00FFFFFF;
	}

	format->id |= ((u32_t)format->level << 24);

	if (IS_ENABLED(CONFIG_RTT_LITE_TRACE_FORMAT_ONCE)) {
		struct send_buffer_context buf = INIT_SEND_BUFFER_CONTEXT;

		send_timeless(EV_FORMAT, format->id);
		send_buffers(&buf, format->text, strlen(format->text) + 1);
		send_buffers(&buf, format->args, strlen(format->args) + 1);
		done_buffers(&buf);
	}
}

void rtt_lite_trace_printf(struct rtt_lite_trace_format *format, ...)
{
	u32_t val32;
	u64_t val64;
	const char *val_str;
	va_list vl;
	u8_t *p;
	struct send_buffer_context buf = INIT_SEND_BUFFER_CONTEXT;

	if (format->id == 0) {
		prepare_format(format);
	}
	rtt_lite_trace_event(EV_PRINTF, format->id);
	if (!IS_ENABLED(CONFIG_RTT_LITE_TRACE_FORMAT_ONCE)) {
		send_buffers(&buf, format->text, strlen(format->text) + 1);
		send_buffers(&buf, format->args, strlen(format->args) + 1);
	}
	p = format->args;
	va_start(vl, format);
	while (*p != FORMAT_ARG_END) {
		switch (*p) {
		case FORMAT_ARG_INT32:
			val32 = va_arg(vl, u32_t);
			send_buffers(&buf, &val32, 4);
			break;
		case FORMAT_ARG_INT64:
			val64 = va_arg(vl, u64_t);
			send_buffers(&buf, &val64, 8);
			break;
		case FORMAT_ARG_STRING:
			val_str = va_arg(vl, const char *);
			send_buffers(&buf, val_str, strlen(val_str) + 1);
			break;
		}
		p++;
	}
	va_end(vl);
	done_buffers(&buf);
}

u32_t rtt_lite_trace_time(void)
{
	return get_time();
}

void rtt_lite_trace_print(u32_t level, const char *text)
{
	union {
		u32_t out;
		char in[4];
	} conv;
	size_t len = strlen(text);

	if (len <= 3) {
		strcpy(conv.in, text);
		rtt_lite_trace_event(EV_PRINT, conv.out);
	} else {
		struct send_buffer_context buf = INIT_SEND_BUFFER_CONTEXT;

		memcpy(conv.in, text, 4);
		rtt_lite_trace_event(EV_PRINT, conv.out);
		send_buffers(&buf, &text[4], len - 4);
		done_buffers(&buf);
	}
}

void rtt_lite_trace_event(u32_t event, u32_t param)
{
	send_event(event, param);
}

void rtt_lite_trace_call_v(u32_t event, u32_t num_args, u32_t arg1, ...)
{
	u32_t i;
	va_list vl;
	u32_t val;
	struct send_buffer_context buf = INIT_SEND_BUFFER_CONTEXT;

	rtt_lite_trace_event(event, arg1);
	va_start(vl, arg1);
	for (i = 1; i < num_args; i++) {
		val = va_arg(vl, u32_t);
		send_buffers(&buf, &val, 4);
	}
	va_end(vl);
	done_buffers(&buf);
}

void rtt_lite_trace_name(u32_t resource_id, const char *name)
{
	struct send_buffer_context buf = INIT_SEND_BUFFER_CONTEXT;

	send_timeless(EV_RES_NAME, resource_id);
	send_buffers(&buf, name, strlen(name) + 1);
	done_buffers(&buf);
}
