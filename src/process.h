#ifndef PROCESS_H
#define PROCESS_H

#include <windows.h>

#define MAX_PROCESSES 1024

typedef struct {
    DWORD pid;
    char name[MAX_PATH];

    SIZE_T memory_usage;
    double cpu_usage;

} ProcessInfo;

int get_processes(
    ProcessInfo *processes,
    int max_processes
);

#endif