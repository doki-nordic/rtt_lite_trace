
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

/** @brief Event reporting priority of new thread or a change of the priority.
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

// TODO: Synchronization pattern events
#define EV_SYNCH_START 0x78
#define EV_SYNCH_END 0x7F


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

