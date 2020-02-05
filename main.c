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

/*
 * Events without time stamp.
 * Bit format:
 *     0000 0000 eeee eeee pppp pppp pppp pppp
 *     e - event numer
 *     p - additional parameter
*/
#define EV_BUFFER_CYCLE 0x00010000
#define EV_THREAD_PRIORITY 0x00020000
#define EV_THREAD_INFO_BEGIN 0x00030000
#define EV_THREAD_INFO_NEXT 0x00040000
#define EV_THREAD_INFO_END 0x00050000

/*
 * Events with time stamp.
 * Bit format:
 *     0eee eeee tttt tttt tttt tttt tttt tttt
 *     e - event numer
 *     t - time stamp
*/
#define EV_SYSTEM_RESET 0x01000000
#define EV_BUFFER_OVERFLOW 0x02000000
#define EV_IDLE 0x03000000
#define EV_THREAD_START 0x04000000
#define EV_THREAD_STOP 0x05000000
#define EV_THREAD_CREATE 0x06000000
#define EV_THREAD_SUSPEND 0x07000000
#define EV_THREAD_RESUME 0x08000000
#define EV_THREAD_READY 0x09000000
#define EV_THREAD_PEND 0x0A000000
#define EV_SYS_CALL 0x0B000000
#define EV_SYS_END_CALL 0x0C000000
#define EV_ISR_EXIT 0x0D000000

/*
 * Events with time stamp and IRQ number.
 * Bit format:
 *     eiii iiii tttt tttt tttt tttt tttt tttt
 *     e - event numer
 *     i - IRQ number
 *     t - time stamp
*/
#define EV_ISR_ENTER 0x80000000

#define SYS_TRACE_ID_OFFSET                  (32u)

#define SYS_TRACE_ID_MUTEX_INIT              (1u + SYS_TRACE_ID_OFFSET)
#define SYS_TRACE_ID_MUTEX_UNLOCK            (2u + SYS_TRACE_ID_OFFSET)
#define SYS_TRACE_ID_MUTEX_LOCK              (3u + SYS_TRACE_ID_OFFSET)
#define SYS_TRACE_ID_SEMA_INIT               (4u + SYS_TRACE_ID_OFFSET)
#define SYS_TRACE_ID_SEMA_GIVE               (5u + SYS_TRACE_ID_OFFSET)
#define SYS_TRACE_ID_SEMA_TAKE               (6u + SYS_TRACE_ID_OFFSET)

#define SYS_TRACE_ID_SYSTEM_RESET            (7u + SYS_TRACE_ID_OFFSET)
#define SYS_TRACE_ID_THREAD_SUSPEND          (8u + SYS_TRACE_ID_OFFSET)
#define SYS_TRACE_ID_THREAD_RESUME           (9u + SYS_TRACE_ID_OFFSET)

typedef struct
{
    uint32_t event;
    uint32_t irq_number;
    uint32_t time_stamp;
    uint32_t resoure_id;
    uint32_t param;
} EventInfo_t;

uint32_t time_offset = 0;
uint32_t last_time_stamp = 0;

uint32_t isr_number = 0;
uint32_t global_time_stamp = 0;
bool no_time_stamp = true;

void decode_event(uint8_t* data, EventInfo_t* info)
{
    uint32_t words[2];
    uint32_t new_time_stamp = 0;
    memcpy(words, data, 8);
    memset(info, 0, sizeof(*info));
    if ((words[0] & 0x80000000) != 0)
    {
        info->event = words[0] & 0x80000000;
        info->irq_number = (words[0] >> 24) & 0x7F;
        new_time_stamp = words[0] & 0x00FFFFFF;
    }
    else if ((words[0] & 0x7F000000) != 0)
    {
        info->event = words[0] & 0x7F000000;
        new_time_stamp = words[0] & 0x00FFFFFF;
        info->resoure_id = words[1];
    }
    else if ((words[0] & 0x00FF0000) != 0)
    {
        info->event = words[0] & 0x00FF0000;
        new_time_stamp = last_time_stamp;
        info->param = words[0] & 0x0000FFFF;
        info->resoure_id = words[1];
    }
    else
    {
        U_FATAL("Invalid event format");
        exit(2);
    }

    printf("%d\n", new_time_stamp);

    if (info->event == EV_SYSTEM_RESET)
    {
        time_offset += last_time_stamp;
    }
    else if (new_time_stamp < last_time_stamp)
    {
        time_offset += 0x01000000;
    }
    last_time_stamp = new_time_stamp;
    info->time_stamp = new_time_stamp + time_offset;
}


static volatile bool exit_loop = false;

void my_handler(int s)
{
    PRINT_DEBUG("Caught signal %d in %d (parent %d)",s, getpid(), getppid());
    if (exit_loop)
    {
        PRINT_ERROR("Forcing process to terminate.");
        exit(TERMINATION_EXIT_CODE);
    }
    exit_loop = true;
}

bool is_hang()
{
    if (options.hang_file && access(options.hang_file, 0 ) >= 0)
    {
        PRINT_INFO("Link is hang");
        return true;
    }
    return false;
}


static void sendSystemDesc(void)
{
    SEGGER_SYSVIEW_SendSysDesc("N=Test App,D=Cortex-M4");
    SEGGER_SYSVIEW_SendSysDesc("I#15=SysTick");
}

int channelID;

U64 GetTime(void)
{
    if (no_time_stamp)
    {
        time_offset++;
        return time_offset;
    }
    return global_time_stamp;
}

void SEGGER_SYSVIEW_Conf(void)
{
    static SEGGER_SYSVIEW_OS_API api = {
        .pfGetTime = NULL,
        .pfSendTaskList = NULL,
    };
    SEGGER_SYSVIEW_Init(16000000, 64000000, &api, sendSystemDesc);
    SEGGER_SYSVIEW_SetRAMBase(0);
    channelID = SEGGER_SYSVIEW_GetChannelID();
    SEGGER_SYSVIEW_Start();
    SEGGER_SYSVIEW_RecordSystime();
    SEGGER_SYSVIEW_OnTaskStopExec();
    SEGGER_SYSVIEW_OnIdle();
}

FILE* out = NULL;

void parse_event(uint8_t* data)
{
    EventInfo_t info;
    decode_event(data, &info);
    printf("ev = 0x%08X  t = %8d  id = 0x%08X  irq = %2d  p = %6d\n", info.event, info.time_stamp, info.resoure_id, info.irq_number, (int16_t)info.param);
    global_time_stamp = info.time_stamp;
    if (no_time_stamp)
    {
        uint32_t diff = global_time_stamp - time_offset;
        time_offset = time_offset - diff + 1;
        global_time_stamp = time_offset + diff;
        no_time_stamp = false;
    }
    switch (info.event)
    {
    case EV_BUFFER_CYCLE:
        //TODO: overflow detection
        break;

    case EV_BUFFER_OVERFLOW:
        //TODO: overflow detection
        break;

    case EV_THREAD_PRIORITY:
        //TODO: send thread information
        break;

    case EV_SYSTEM_RESET:
        SEGGER_SYSVIEW_RecordVoid(SYS_TRACE_ID_SYSTEM_RESET);
        break;

    case EV_IDLE:
        SEGGER_SYSVIEW_OnIdle();
        break;

    case EV_THREAD_START:
        SEGGER_SYSVIEW_OnTaskStartExec(info.resoure_id);
        //idle = false;
        break;

    case EV_THREAD_STOP:
        SEGGER_SYSVIEW_OnTaskStopExec();
        break;

    case EV_THREAD_CREATE:
        SEGGER_SYSVIEW_OnTaskCreate(info.resoure_id);
        break;

    case EV_THREAD_SUSPEND:
        SEGGER_SYSVIEW_RecordU32(SYS_TRACE_ID_THREAD_SUSPEND, info.resoure_id);
        break;

    case EV_THREAD_RESUME:
        SEGGER_SYSVIEW_RecordU32(SYS_TRACE_ID_THREAD_RESUME, info.resoure_id);
        break;

    case EV_THREAD_READY:
        SEGGER_SYSVIEW_OnTaskStartReady(info.resoure_id);
        break;

    case EV_THREAD_PEND:
        SEGGER_SYSVIEW_OnTaskStopReady(info.resoure_id, 2);
        break;

    case EV_SYS_CALL:
        SEGGER_SYSVIEW_RecordVoid(info.resoure_id);
        break;

    case EV_SYS_END_CALL:
        SEGGER_SYSVIEW_RecordEndCall(info.resoure_id);
        break;

    case EV_ISR_EXIT:
        SEGGER_SYSVIEW_RecordExitISR();
        break;

    case EV_ISR_ENTER:
        isr_number = info.irq_number;
        SEGGER_SYSVIEW_RecordEnterISR();
        break;

    case EV_THREAD_INFO_BEGIN:
        // TODO: thread info decode
        break;

    case EV_THREAD_INFO_NEXT:
        // TODO: thread info decode
        break;

    case EV_THREAD_INFO_END:
        // TODO: thread info decode
        break;


    default:
        U_FATAL("Uknown event 0x%08X\n", info.event);
        break;
    }
    static uint8_t buffer[65536];
    uint32_t NumBytes = SEGGER_RTT_ReadUpBufferNoLock(channelID, buffer, sizeof(buffer));
    //printf("   =>   %d\n", NumBytes);
    fwrite(buffer, 1, NumBytes, out);
}

#if !DO_TESTING

int main(int argc, char* argv[])
{
    parse_args(argc, argv);
    //tap_create();

    SEGGER_SYSVIEW_Conf();

    signal(SIGINT, my_handler);

    if (!options.no_rtt_retry)
    {
        do
        {
            while (is_hang() && !exit_loop)
            {
                usleep(1000 * 1000);
            }

            if (exit_loop)
            {
                //tap_delete();
                PRINT_INFO("TERMINATED parent process");
                exit(TERMINATION_EXIT_CODE);
            }

            int pid = fork();

            if (pid < 0)
            {
                U_ERRNO_FATAL("Cannot fork process!");
            }
            else if (pid == 0)
            {
                if (is_hang())
                {
                    exit(RECOVERABLE_EXIT_CODE);
                }
                break;
            }
            else
            {
                int wstatus = 0;
                PRINT_INFO("Child process %d", pid);
                pid_t r = waitpid(pid, &wstatus, 0);
                //tap_set_state(false);
                PRINT_INFO("PID %d, %d, %d", r, WEXITSTATUS(wstatus), wstatus);
                if (WIFSIGNALED(wstatus) && !exit_loop)
                {
                    PRINT_ERROR("Unexpected child %d exit, signal %d", pid, WTERMSIG(wstatus));
                    usleep(1000 * 1000);
                }
                else if (WEXITSTATUS(wstatus) == TERMINATION_EXIT_CODE && r >= 0)
                {
                    exit_loop = true;
                }
                else if (WEXITSTATUS(wstatus) != RECOVERABLE_EXIT_CODE && r >= 0)
                {
                    exit(wstatus);
                }
                else if (!exit_loop)
                {
                    usleep(1000 * 1000);
                }
            }
        }
        while (true);
    }

    nrfjprog_init(true);

    //tap_set_state(true);

    PRINT_INFO("BEGIN");

    int last_hang_check;
    if (options.hang_file)
    {
        last_hang_check = time(NULL);
    }

    uint8_t buffer[65536];
    uint32_t i;
    uint32_t total = 0;
    uint32_t left_over = 0;

    out = fopen("out.SVDat", "w");
    fclose(out);

    while (!exit_loop)
    {
        /*uint32_t NumBytes = SEGGER_RTT_ReadUpBufferNoLock(channelID, buffer, sizeof(buffer));
        printf("s=%d\n", NumBytes);
        FILE* f = fopen("out.SVDat", "w");
        fwrite(buffer, 1, NumBytes, f);
        fclose(f);
        exit(0);*/
        uint32_t num = rtt_read(&buffer[left_over], sizeof(buffer) - left_over);
        num += left_over;
        if (num)
        {
            out = fopen("out.SVDat", "a");
            for (i = 0; i <= num - 8; i += 8)
            {
                parse_event(&buffer[i]);
            }
            fclose(out);
            left_over = num - i;
            memmove(&buffer[0], &buffer[i], left_over);
        }
        else
        {
            sleep(1);
        }
    }

    PRINT_INFO("TERMINATED");

    NRFJPROG_close_dll();

    if (options.no_rtt_retry)
    {
        //tap_delete();
    }

    return TERMINATION_EXIT_CODE;
}

#endif // !DO_TESTING

U32 SEGGER_SYSVIEW_X_GetTimestamp()
{
    static uint32_t t = 0;
    t++;
    return GetTime();
}


U32 SEGGER_SYSVIEW_X_GetInterruptId(void)
{
    return isr_number;
}

