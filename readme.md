
# CTaskManager

A lightweight Windows Task Manager clone written in C using the Win32 API.

CTaskManager provides a native Windows interface for viewing and managing running processes, monitoring CPU and memory usage, terminating processes, and inspecting executable information.

## Features

- View running processes and their process IDs (PID)
- Monitor per-process CPU and memory usage
- Automatically refresh process information every second
- End processes using the Delete key or context menu
- Open a process's executable file location
- View process properties, including its executable path
- Right-click context menu
- Native Windows GUI with a custom application icon

## Download

**[Download CTaskManager for Windows](../../releases/latest/download/CTaskManager.exe)**

Alternatively, visit the [Releases page](../../releases/latest) to download the latest version.


## Requirements

- Windows 10 or Windows 11
- 64-bit Windows

Some system processes may require administrator privileges to access or terminate.

## Building from Source

### Prerequisites

- GCC (MinGW-w64)
- GNU Make
- GNU Windows Resource Compiler (`windres`)

The project was developed using the MSYS2 UCRT64 environment.

### Clone the Repository

```bash
git clone https://github.com/ferinsh/CTaskManager.git
cd CTaskManager
```


### Build

Run the following command from the project directory:

```cmd
mingw32-make
```

This generates `CTaskManager.exe`.

### Run

```cmd
./CTaskManager.exe
```

### Clean

```cmd
mingw32-make clean
```

## Project Structure

```text
CTaskManager/
├── src/
│   ├── main.c
│   ├── process.c
│   └── process.h
├── resources/
│   ├── app.ico
│   └── resource.rc
├── Makefile
├── README.md
└── .gitignore
```

## Architecture

CTaskManager uses the Win32 API directly, without an external GUI framework.

### Graphical Interface

The interface uses Windows Common Controls, including a ListView for displaying process information.

The application updates existing rows, adds newly detected processes, and removes processes that have terminated.

### Process Enumeration

Running processes are enumerated using the Windows Tool Help API.

Process information is retrieved using:

- `CreateToolhelp32Snapshot`
- `Process32First`
- `Process32Next`
- `OpenProcess`
- `GetProcessMemoryInfo`
- `GetProcessTimes`

### CPU Monitoring

CPU usage is calculated by comparing process CPU time across consecutive measurements.

The calculation uses process kernel time, user time, elapsed wall-clock time, and the number of logical processors.

### Memory Monitoring

Memory usage is obtained using `GetProcessMemoryInfo` and displayed in megabytes.

### Process Management

Processes can be terminated using the Windows `TerminateProcess` API.

Executable paths are retrieved using Windows process APIs, allowing users to inspect process properties or open executable locations in File Explorer.

## Controls

| Action | Description |
|---|---|
| Left-click | Select a process |
| Right-click | Open the process context menu |
| Delete | Terminate the selected process |
| End task | Terminate the selected process |
| Open file location | Locate the executable in File Explorer |
| Properties | Display detailed process information |

## Dependencies

CTaskManager links against the following Windows system libraries:

- `gdi32`
- `comctl32`
- `psapi`
- `shell32`

No external GUI framework is required.

<!-- ## Planned Features

- [ ] Sort processes by name, PID, CPU, or memory
- [ ] Search and filter processes
- [ ] Process tree view
- [ ] System-wide CPU and memory monitoring
- [ ] Real-time performance graphs
- [ ] Disk and network monitoring
- [ ] GPU monitoring
- [ ] Dark mode
- [ ] More detailed process properties -->

## About

CTaskManager is a learning project focused on C programming, operating-system concepts, Windows system programming, and native GUI development.

The project demonstrates how to interact directly with Windows system APIs to enumerate processes, monitor resource usage, and manage running applications.