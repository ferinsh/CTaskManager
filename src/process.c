#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

#include "process.h"

#define MAX_CACHE_ENTRIES 2048

static DWORD cpu_count = 0;

typedef struct {
    DWORD pid;

    ULONGLONG creation_time;

    ULONGLONG kernel_time;
    ULONGLONG user_time;

    LARGE_INTEGER timestamp;

    int valid;

} ProcessCacheEntry;

static ProcessCacheEntry process_cache[MAX_CACHE_ENTRIES];


/*
 * Convert FILETIME into a 64-bit integer.
 *
 * FILETIME is expressed in 100-nanosecond units.
 */
static ULONGLONG filetime_to_uint64(FILETIME time)
{
    return
        ((ULONGLONG)time.dwHighDateTime << 32) |
        time.dwLowDateTime;
}


/*
 * Get high-resolution current time.
 */
static LARGE_INTEGER get_current_time(void)
{
    LARGE_INTEGER time;

    QueryPerformanceCounter(&time);

    return time;
}


/*
 * Find a process in the cache.
 *
 * PID alone isn't enough because Windows can reuse PIDs.
 */
static ProcessCacheEntry *find_cache_entry(
    DWORD pid,
    ULONGLONG creation_time
)
{
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {

        if (!process_cache[i].valid) {
            continue;
        }

        if (process_cache[i].pid == pid &&
            process_cache[i].creation_time == creation_time) {

            return &process_cache[i];
        }
    }

    return NULL;
}


/*
 * Create a cache entry.
 */
static ProcessCacheEntry *create_cache_entry(
    DWORD pid,
    ULONGLONG creation_time
)
{
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {

        if (!process_cache[i].valid) {

            process_cache[i].valid = 1;

            process_cache[i].pid = pid;

            process_cache[i].creation_time =
                creation_time;

            process_cache[i].kernel_time = 0;

            process_cache[i].user_time = 0;

            process_cache[i].timestamp =
                get_current_time();

            return &process_cache[i];
        }
    }

    return NULL;
}


/*
 * Get process creation time and CPU time.
 */
static int get_process_times_info(
    HANDLE process_handle,
    ULONGLONG *creation_time,
    ULONGLONG *kernel_time,
    ULONGLONG *user_time
)
{
    FILETIME creation;
    FILETIME exit_time;
    FILETIME kernel;
    FILETIME user;

    if (!GetProcessTimes(
        process_handle,
        &creation,
        &exit_time,
        &kernel,
        &user
    )) {
        return 0;
    }

    *creation_time =
        filetime_to_uint64(creation);

    *kernel_time =
        filetime_to_uint64(kernel);

    *user_time =
        filetime_to_uint64(user);

    return 1;
}


int get_processes(
    ProcessInfo *processes,
    int max_processes
)
{
    if (cpu_count == 0) {
        SYSTEM_INFO system_info;
        GetSystemInfo(&system_info);

        cpu_count = system_info.dwNumberOfProcessors;
    }

    HANDLE snapshot;

    snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPPROCESS,
        0
    );

    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }


    PROCESSENTRY32 process;

    process.dwSize =
        sizeof(PROCESSENTRY32);


    if (!Process32First(
        snapshot,
        &process
    )) {

        CloseHandle(snapshot);

        return 0;
    }


    int count = 0;


    do {

        if (count >= max_processes) {
            break;
        }


        /*
         * Basic information.
         */
        processes[count].pid =
            process.th32ProcessID;

        lstrcpy(
            processes[count].name,
            process.szExeFile
        );

        processes[count].memory_usage = 0;

        processes[count].cpu_usage = 0.0;


        /*
         * Open process.
         */
        HANDLE process_handle = OpenProcess(
            PROCESS_QUERY_INFORMATION |
            PROCESS_VM_READ,
            FALSE,
            process.th32ProcessID
        );


        if (process_handle != NULL) {


            /*
             * =====================================
             * MEMORY
             * =====================================
             */

            PROCESS_MEMORY_COUNTERS memory_info;


            if (GetProcessMemoryInfo(
                process_handle,
                &memory_info,
                sizeof(memory_info)
            )) {

                processes[count].memory_usage =
                    memory_info.WorkingSetSize;
            }


            /*
             * =====================================
             * CPU
             * =====================================
             */

            ULONGLONG creation_time;
            ULONGLONG kernel_time;
            ULONGLONG user_time;


            if (get_process_times_info(
                process_handle,
                &creation_time,
                &kernel_time,
                &user_time
            )) {

                LARGE_INTEGER current_timestamp =
                    get_current_time();


                ProcessCacheEntry *cache =
                    find_cache_entry(
                        process.th32ProcessID,
                        creation_time
                    );


                /*
                 * First time we've seen this
                 * process.
                 */
                if (cache == NULL) {

                    cache =
                        create_cache_entry(
                            process.th32ProcessID,
                            creation_time
                        );

                    if (cache != NULL) {

                        cache->kernel_time =
                            kernel_time;

                        cache->user_time =
                            user_time;

                        cache->timestamp =
                            current_timestamp;
                    }

                    processes[count].cpu_usage =
                        0.0;
                }


                /*
                 * We've seen this process before.
                 */
                else {

                    ULONGLONG previous_cpu =
                        cache->kernel_time +
                        cache->user_time;


                    ULONGLONG current_cpu =
                        kernel_time +
                        user_time;


                    /*
                     * CPU time must never go backwards.
                     */
                    if (current_cpu >= previous_cpu) {

                        ULONGLONG cpu_delta =
                            current_cpu -
                            previous_cpu;


                        LARGE_INTEGER time_delta;

                        time_delta.QuadPart =
                            current_timestamp.QuadPart -
                            cache->timestamp.QuadPart;


                        LARGE_INTEGER frequency;

                        QueryPerformanceFrequency(
                            &frequency
                        );


                        /*
                         * Convert elapsed performance-counter
                         * ticks into 100-nanosecond units.
                         */
                        if (frequency.QuadPart > 0 &&
                            time_delta.QuadPart > 0) {

                            double elapsed_100ns =
                                (
                                    (double)time_delta.QuadPart /
                                    (double)frequency.QuadPart
                                ) *
                                10000000.0;


                            processes[count].cpu_usage =
                                (
                                    (double)cpu_delta /
                                    elapsed_100ns
                                ) *
                                100.0 /
                                (double)cpu_count;
                        }
                    }


                    /*
                     * Save current measurement.
                     */
                    cache->kernel_time =
                        kernel_time;

                    cache->user_time =
                        user_time;

                    cache->timestamp =
                        current_timestamp;
                }
            }


            /*
             * Always close the process handle.
             */
            CloseHandle(process_handle);
        }


        count++;


    } while (
        Process32Next(
            snapshot,
            &process
        )
    );


    CloseHandle(snapshot);


    return count;
}