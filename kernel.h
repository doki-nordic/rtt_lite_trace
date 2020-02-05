#ifndef _MOCK_KERNEL_H_
#define _MOCK_KERNEL_H_

#include <memory.h>
#include <stdint.h>
#include <stdbool.h>

#define CONFIG_RTT_LITE_TRACE_FORMAT_ONCE 1
#define CONFIG_RTT_LITE_TRACE_THREAD_INFO 1
#define CONFIG_RTT_LITE_TRACE_FAST_OVERFLOW_CHECK 0
#define CONFIG_RTT_LITE_TRACE_BUFFER_STATS 1
#define CONFIG_RTT_LITE_TRACE_IRQ 1
#define CONFIG_RTT_LITE_TRACE_RTT_CHANNEL 2
#define CONFIG_RTT_LITE_TRACE_PRINTF_MAX_ARGS 10
#define CONFIG_RTT_LITE_TRACE_BUFFER_SIZE_1KB 1
#define CONFIG_RTT_LITE_TRACE_TIMER0 1

#define CONFIG_THREAD_NAME 1
#define CONFIG_THREAD_STACK_INFO 1

#define IS_ENABLED(x) (x)

#define ALWAYS_INLINE inline

typedef uint8_t u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;
typedef uint64_t u64_t;

typedef int8_t s8_t;
typedef int16_t s16_t;
typedef int32_t s32_t;
typedef int64_t s64_t;

struct k_thread {
	struct {
		int8_t prio;
	} base;
	struct {
		uint32_t size;
		uint32_t start;
	} stack_info;

	struct k_thread * next_thread;
	const char* name;
};

typedef struct k_thread *k_tid_t;

struct _mock_kernel
{
	k_tid_t threads;
};

extern struct _mock_kernel _kernel;

struct _mock_timer
{
	int TASKS_CAPTURE[1];
	uint32_t CC[1];
};

extern struct _mock_timer *NRF_TIMER0;


#define irq_lock() (0)
#define irq_unlock(...)


extern uint8_t _mock_isr_number;
extern k_tid_t _mock_idle_thread;
extern k_tid_t _mock_current_thread;

#define __get_IPSR() (_mock_isr_number)
#define z_is_idle_thread_object(x) ((x) == _mock_idle_thread)
#define k_current_get() (_mock_current_thread)

typedef int nrfx_timer_t;
typedef struct { int frequency; int bit_width; } nrfx_timer_config_t;
#define NRFX_TIMER_DEFAULT_CONFIG {}
#define nrfx_timer_enable(...)
#define nrfx_timer_init(...)
#define NRFX_TIMER_INSTANCE(...) (0)

#define NRF_TIMER_BIT_WIDTH_24 0
#define NRF_TIMER_FREQ_16MHz 0

#define k_thread_name_get(x) ((x)->name)

#endif
