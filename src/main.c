#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <shellapi.h>
#include <psapi.h>

#include "process.h"

#define IDC_PROCESS_LIST 1001

#define TIMER_PROCESS_REFRESH 1
#define PROCESS_REFRESH_INTERVAL 1000

#define PROCESS_PROPERTIES_CLASS "ProcessProperties"
#define IDC_PROPERTIES_CLOSE 2001

HWND process_list;
DWORD selected_pid = 0;


static void open_process_location(void);
static void show_process_properties(void);

static LRESULT CALLBACK PropertiesWindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
);


/*
 * Find the ListView row belonging to a PID.
 */
static int find_process_row(DWORD pid)
{
    int count = ListView_GetItemCount(
        process_list
    );

    for (int i = 0; i < count; i++) {

        LVITEMA item = {0};

        item.mask = LVIF_PARAM;
        item.iItem = i;

        if (ListView_GetItem(
            process_list,
            &item
        )) {

            if ((DWORD)item.lParam == pid) {
                return i;
            }
        }
    }

    return -1;
}


/*
 * Populate the ListView with running processes.
 */
void populate_process_list(void)
{
    ProcessInfo processes[MAX_PROCESSES];

    int process_count = get_processes(
        processes,
        MAX_PROCESSES
    );

    /*
     * Process every currently running process.
     */
    for (int i = 0; i < process_count; i++) {

        int row = find_process_row(
            processes[i].pid
        );

        /*
         * Process is new.
         */
        if (row == -1) {

            LVITEMA item = {0};

            item.mask =
                LVIF_TEXT |
                LVIF_PARAM;

            item.iItem =
                ListView_GetItemCount(
                    process_list
                );

            item.iSubItem = 0;

            item.pszText =
                processes[i].name;

            /*
             * Store PID in the ListView item.
             */
            item.lParam =
                (LPARAM)processes[i].pid;

            row = ListView_InsertItem(
                process_list,
                &item
            );

            if (row == -1) {
                continue;
            }
        }

        /*
         * PID column.
         */
        char pid_text[32];

        wsprintfA(
            pid_text,
            "%lu",
            processes[i].pid
        );

        ListView_SetItemText(
            process_list,
            row,
            1,
            pid_text
        );

        /*
         * Memory column.
         */
        char memory_text[32];

        double memory_mb =
            (double)processes[i].memory_usage /
            (1024.0 * 1024.0);

        snprintf(
            memory_text,
            sizeof(memory_text),
            "%.1f MB",
            memory_mb
        );

        ListView_SetItemText(
            process_list,
            row,
            2,
            memory_text
        );

        /*
         * CPU column.
         */
        char cpu_text[32];

        snprintf(
            cpu_text,
            sizeof(cpu_text),
            "%.1f%%",
            processes[i].cpu_usage
        );

        ListView_SetItemText(
            process_list,
            row,
            3,
            cpu_text
        );
    }

    /*
     * Remove processes that no longer exist.
     *
     * Work backwards because deleting a row
     * changes the indexes of the rows after it.
     */
    for (
        int i = ListView_GetItemCount(process_list) - 1;
        i >= 0;
        i--
    ) {

        LVITEMA item = {0};

        item.mask = LVIF_PARAM;
        item.iItem = i;

        if (!ListView_GetItem(
            process_list,
            &item
        )) {
            continue;
        }

        DWORD pid =
            (DWORD)item.lParam;

        int found = 0;

        for (int j = 0; j < process_count; j++) {

            if (processes[j].pid == pid) {
                found = 1;
                break;
            }
        }

        if (!found) {

            if (pid == selected_pid) {
                selected_pid = 0;
            }

            ListView_DeleteItem(
                process_list,
                i
            );
        }
    }
}


/*
 * End the currently selected process.
 */
static void end_selected_process(void)
{
    if (selected_pid == 0) {
        return;
    }

    HANDLE process_handle = OpenProcess(
        PROCESS_TERMINATE,
        FALSE,
        selected_pid
    );

    if (process_handle == NULL) {

        MessageBoxA(
            NULL,
            "Could not open the selected process.",
            "Error",
            MB_OK | MB_ICONERROR
        );

        return;
    }

    if (!TerminateProcess(
        process_handle,
        1
    )) {

        MessageBoxA(
            NULL,
            "Could not terminate the selected process.",
            "Error",
            MB_OK | MB_ICONERROR
        );
    }

    CloseHandle(process_handle);

    selected_pid = 0;
}


/*
 * Show the process context menu.
 */
static void show_process_context_menu(
    HWND hwnd,
    int x,
    int y
)
{
    if (selected_pid == 0) {
        return;
    }

    HMENU menu = CreatePopupMenu();

    if (menu == NULL) {
        return;
    }

    AppendMenuA(
        menu,
        MF_STRING,
        1,
        "End task"
    );

    AppendMenuA(
        menu,
        MF_SEPARATOR,
        0,
        NULL
    );

    AppendMenuA(
        menu,
        MF_STRING,
        2,
        "Open file location"
    );

    AppendMenuA(
        menu,
        MF_STRING,
        3,
        "Properties"
    );

    int command = TrackPopupMenu(
        menu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON,
        x,
        y,
        0,
        hwnd,
        NULL
    );

    if (command == 1) {

        end_selected_process();

    }
    else if (command == 2) {

        open_process_location();

    }
    else if (command == 3) {

        show_process_properties();
    }

    DestroyMenu(menu);
}


/*
 * Open the folder containing the process executable.
 */
static void open_process_location(void)
{
    if (selected_pid == 0) {
        return;
    }

    HANDLE process_handle = OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION,
        FALSE,
        selected_pid
    );

    if (process_handle == NULL) {

        MessageBoxA(
            NULL,
            "Could not open the selected process.",
            "Error",
            MB_OK | MB_ICONERROR
        );

        return;
    }

    char path[MAX_PATH];
    DWORD path_size = sizeof(path);

    if (!QueryFullProcessImageNameA(
        process_handle,
        0,
        path,
        &path_size
    )) {

        CloseHandle(process_handle);

        MessageBoxA(
            NULL,
            "Could not determine the executable path.",
            "Error",
            MB_OK | MB_ICONERROR
        );

        return;
    }

    CloseHandle(process_handle);

    char explorer_command[MAX_PATH + 20];

    snprintf(
        explorer_command,
        sizeof(explorer_command),
        "/select,\"%s\"",
        path
    );

    ShellExecuteA(
        NULL,
        "open",
        "explorer.exe",
        explorer_command,
        NULL,
        SW_SHOWNORMAL
    );
}


/*
 * Properties window procedure.
 */
static LRESULT CALLBACK PropertiesWindowProc(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (message) {

        case WM_CREATE:
        {
            CREATESTRUCTA *create_info =
                (CREATESTRUCTA *)lParam;

            ProcessInfo *process =
                (ProcessInfo *)create_info->lpCreateParams;

            char text[256];

            /*
             * Name
             */
            wsprintfA(
                text,
                "Name: %s",
                process->name
            );

            CreateWindowA(
                "STATIC",
                text,
                WS_CHILD | WS_VISIBLE,
                20,
                20,
                500,
                25,
                hwnd,
                NULL,
                GetModuleHandle(NULL),
                NULL
            );

            /*
             * PID
             */
            wsprintfA(
                text,
                "PID: %lu",
                process->pid
            );

            CreateWindowA(
                "STATIC",
                text,
                WS_CHILD | WS_VISIBLE,
                20,
                50,
                500,
                25,
                hwnd,
                NULL,
                GetModuleHandle(NULL),
                NULL
            );

            /*
             * Memory
             */
            double memory_mb =
                (double)process->memory_usage /
                (1024.0 * 1024.0);

            snprintf(
                text,
                sizeof(text),
                "Memory: %.1f MB",
                memory_mb
            );

            CreateWindowA(
                "STATIC",
                text,
                WS_CHILD | WS_VISIBLE,
                20,
                80,
                500,
                25,
                hwnd,
                NULL,
                GetModuleHandle(NULL),
                NULL
            );

            /*
             * CPU
             */
            snprintf(
                text,
                sizeof(text),
                "CPU: %.1f%%",
                process->cpu_usage
            );

            CreateWindowA(
                "STATIC",
                text,
                WS_CHILD | WS_VISIBLE,
                20,
                110,
                500,
                25,
                hwnd,
                NULL,
                GetModuleHandle(NULL),
                NULL
            );

            /*
             * Executable path.
             *
             * Always create the Path controls,
             * even if Windows cannot give us
             * the executable path.
             */
            char path[MAX_PATH];

            lstrcpyA(
                path,
                "Path unavailable"
            );

            HANDLE process_handle = OpenProcess(
                PROCESS_QUERY_INFORMATION |
                PROCESS_VM_READ,
                FALSE,
                process->pid
            );

            if (process_handle != NULL) {

                DWORD path_length =
                    GetModuleFileNameExA(
                        process_handle,
                        NULL,
                        path,
                        sizeof(path)
                    );

                if (path_length == 0) {

                    lstrcpyA(
                        path,
                        "Path unavailable"
                    );
                }

                CloseHandle(process_handle);
            }

            /*
             * Path label.
             */
            CreateWindowA(
                "STATIC",
                "Path:",
                WS_CHILD | WS_VISIBLE,
                20,
                145,
                50,
                25,
                hwnd,
                NULL,
                GetModuleHandle(NULL),
                NULL
            );

            /*
             * Read-only path box.
             *
             * The user can select and copy
             * the executable path.
             */
            CreateWindowA(
                "EDIT",
                path,
                WS_CHILD |
                WS_VISIBLE |
                WS_BORDER |
                ES_READONLY |
                ES_AUTOHSCROLL,
                70,
                145,
                400,
                25,
                hwnd,
                NULL,
                GetModuleHandle(NULL),
                NULL
            );

            /*
             * Close button.
             */
            CreateWindowA(
                "BUTTON",
                "Close",
                WS_CHILD |
                WS_VISIBLE |
                BS_DEFPUSHBUTTON,
                185,
                190,
                100,
                30,
                hwnd,
                (HMENU)IDC_PROPERTIES_CLOSE,
                GetModuleHandle(NULL),
                NULL
            );

            return 0;
        }

        case WM_COMMAND:

            if (LOWORD(wParam) ==
                IDC_PROPERTIES_CLOSE) {

                DestroyWindow(hwnd);

                return 0;
            }

            break;

        case WM_CLOSE:

            DestroyWindow(hwnd);

            return 0;

        case WM_DESTROY:

            return 0;
    }

    return DefWindowProcA(
        hwnd,
        message,
        wParam,
        lParam
    );
}


/*
 * Create the Properties window.
 */
static void show_process_properties(void)
{
    if (selected_pid == 0) {
        return;
    }

    ProcessInfo processes[MAX_PROCESSES];

    int count = get_processes(
        processes,
        MAX_PROCESSES
    );

    ProcessInfo *selected_process = NULL;

    for (int i = 0; i < count; i++) {

        if (processes[i].pid == selected_pid) {

            selected_process =
                &processes[i];

            break;
        }
    }

    if (selected_process == NULL) {

        MessageBoxA(
            NULL,
            "The selected process no longer exists.",
            "Process Properties",
            MB_OK | MB_ICONWARNING
        );

        return;
    }

    WNDCLASSA wc = {0};

    wc.lpfnWndProc =
        PropertiesWindowProc;

    wc.hInstance =
        GetModuleHandle(NULL);

    wc.lpszClassName =
        PROCESS_PROPERTIES_CLASS;

    wc.hbrBackground =
        (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassA(&wc);

    CreateWindowExA(
        WS_EX_DLGMODALFRAME,
        PROCESS_PROPERTIES_CLASS,
        "Process Properties",
        WS_OVERLAPPED |
        WS_CAPTION |
        WS_SYSMENU |
        WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        520,
        270,
        GetParent(process_list),
        NULL,
        GetModuleHandle(NULL),
        selected_process
    );
}


/*
 * Main window procedure.
 */
LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (uMsg) {

        /*
         * Window creation.
         */
        case WM_CREATE:
        {
            process_list = CreateWindowExA(
                0,
                WC_LISTVIEWA,
                "",
                WS_CHILD |
                WS_VISIBLE |
                LVS_REPORT |
                LVS_SINGLESEL |
                LVS_SHOWSELALWAYS,
                0,
                0,
                0,
                0,
                hwnd,
                (HMENU)IDC_PROCESS_LIST,
                GetModuleHandle(NULL),
                NULL
            );

            if (process_list == NULL) {

                MessageBoxA(
                    hwnd,
                    "Failed to create process list.",
                    "CTaskManager",
                    MB_ICONERROR
                );

                return -1;
            }

            /*
             * Full row selection and grid lines.
             */
            ListView_SetExtendedListViewStyle(
                process_list,
                LVS_EX_FULLROWSELECT |
                LVS_EX_GRIDLINES
            );

            /*
             * Name column.
             */
            LVCOLUMNA column = {0};

            column.mask =
                LVCF_TEXT |
                LVCF_WIDTH;

            column.cx = 300;
            column.pszText = "Name";

            ListView_InsertColumn(
                process_list,
                0,
                &column
            );

            /*
             * PID column.
             */
            column.cx = 120;
            column.pszText = "PID";

            ListView_InsertColumn(
                process_list,
                1,
                &column
            );

            /*
             * Memory column.
             */
            column.cx = 140;
            column.pszText = "Memory";

            ListView_InsertColumn(
                process_list,
                2,
                &column
            );

            /*
             * CPU column.
             */
            column.cx = 100;
            column.pszText = "CPU";

            ListView_InsertColumn(
                process_list,
                3,
                &column
            );

            /*
             * Initial process list.
             */
            populate_process_list();

            /*
             * Refresh every second.
             */
            SetTimer(
                hwnd,
                TIMER_PROCESS_REFRESH,
                PROCESS_REFRESH_INTERVAL,
                NULL
            );

            return 0;
        }

        /*
         * Notifications from the ListView.
         */
        case WM_NOTIFY:
        {
            NMHDR *header =
                (NMHDR *)lParam;

            if (header->hwndFrom ==
                process_list) {

                /*
                 * Selection changed.
                 */
                if (header->code ==
                    LVN_ITEMCHANGED) {

                    NMLISTVIEW *notification =
                        (NMLISTVIEW *)lParam;

                    if (
                        (notification->uNewState &
                         LVIS_SELECTED) &&

                        !(notification->uOldState &
                          LVIS_SELECTED)
                    ) {

                        int row =
                            notification->iItem;

                        LVITEMA item = {0};

                        item.mask =
                            LVIF_PARAM;

                        item.iItem =
                            row;

                        if (ListView_GetItem(
                            process_list,
                            &item
                        )) {

                            selected_pid =
                                (DWORD)item.lParam;

                            char message[64];

                            wsprintfA(
                                message,
                                "Selected PID: %lu",
                                selected_pid
                            );

                            SetWindowTextA(
                                GetParent(process_list),
                                message
                            );
                        }
                    }

                    /*
                     * Nothing selected.
                     */
                    if (
                        (notification->uOldState &
                         LVIS_SELECTED) &&

                        !(notification->uNewState &
                          LVIS_SELECTED)
                    ) {

                        selected_pid = 0;

                        SetWindowTextA(
                            GetParent(process_list),
                            "CTaskManager"
                        );
                    }
                }

                /*
                 * Clicked empty area.
                 */
                if (header->code == NM_CLICK) {

                    NMITEMACTIVATE *click =
                        (NMITEMACTIVATE *)lParam;

                    if (click->iItem == -1) {

                        ListView_SetItemState(
                            process_list,
                            -1,
                            0,
                            LVIS_SELECTED
                        );

                        selected_pid = 0;

                        SetWindowTextA(
                            GetParent(process_list),
                            "CTaskManager"
                        );
                    }
                }

                /*
                 * Delete key.
                 */
                if (header->code ==
                    LVN_KEYDOWN) {

                    NMLVKEYDOWN *key =
                        (NMLVKEYDOWN *)lParam;

                    if (key->wVKey ==
                        VK_DELETE) {

                        end_selected_process();
                    }
                }

                /*
                 * Right-click.
                 */
                if (header->code ==
                    NM_RCLICK) {

                    NMLISTVIEW *click =
                        (NMLISTVIEW *)lParam;

                    if (click->iItem >= 0) {

                        LVITEMA item = {0};

                        item.mask =
                            LVIF_PARAM;

                        item.iItem =
                            click->iItem;

                        if (ListView_GetItem(
                            process_list,
                            &item
                        )) {

                            selected_pid =
                                (DWORD)item.lParam;

                            POINT point;

                            GetCursorPos(&point);

                            show_process_context_menu(
                                GetParent(process_list),
                                point.x,
                                point.y
                            );
                        }
                    }
                }
            }

            return 0;
        }

        /*
         * Resize.
         */
        case WM_SIZE:
        {
            if (process_list != NULL) {

                int width =
                    LOWORD(lParam);

                int height =
                    HIWORD(lParam);

                MoveWindow(
                    process_list,
                    0,
                    0,
                    width,
                    height,
                    TRUE
                );
            }

            return 0;
        }

        /*
         * Refresh timer.
         */
        case WM_TIMER:
        {
            if (wParam ==
                TIMER_PROCESS_REFRESH) {

                populate_process_list();
            }

            return 0;
        }

        /*
         * Destroy main window.
         */
        case WM_DESTROY:
        {
            KillTimer(
                hwnd,
                TIMER_PROCESS_REFRESH
            );

            PostQuitMessage(0);

            return 0;
        }

        default:

            return DefWindowProcA(
                hwnd,
                uMsg,
                wParam,
                lParam
            );
    }
}


/*
 * Program entry point.
 */
int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow
)
{
    (void)hPrevInstance;
    (void)lpCmdLine;

    /*
     * Initialize common controls.
     */
    INITCOMMONCONTROLSEX common_controls = {0};

    common_controls.dwSize =
        sizeof(INITCOMMONCONTROLSEX);

    common_controls.dwICC =
        ICC_LISTVIEW_CLASSES;

    InitCommonControlsEx(
        &common_controls
    );

    /*
     * Main window class.
     */
    const char CLASS_NAME[] =
        "CTaskManagerWindow";

    WNDCLASSA wc = {0};

    wc.lpfnWndProc =
        WindowProc;

    wc.hInstance =
        hInstance;

    wc.lpszClassName =
        CLASS_NAME;

    wc.hCursor =
        LoadCursor(
            NULL,
            IDC_ARROW
        );

    wc.hIcon = LoadIconA(
        hInstance,
        "IDI_APP_ICON"
    );
    
    wc.hbrBackground =
        (HBRUSH)(COLOR_WINDOW + 1);

    /*
     * Register main window class.
     */
    if (!RegisterClassA(&wc)) {

        MessageBoxA(
            NULL,
            "Failed to register window class.",
            "CTaskManager",
            MB_ICONERROR
        );

        return 1;
    }

    /*
     * Create main window.
     */
    HWND hwnd = CreateWindowExA(
        0,
        CLASS_NAME,
        "CTaskManager",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1000,
        650,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hwnd == NULL) {

        MessageBoxA(
            NULL,
            "Failed to create window.",
            "CTaskManager",
            MB_ICONERROR
        );

        return 1;
    }

    /*
     * Show main window.
     */
    ShowWindow(
        hwnd,
        nCmdShow
    );

    UpdateWindow(hwnd);

    /*
     * Message loop.
     */
    MSG msg;

    while (
        GetMessageA(
            &msg,
            NULL,
            0,
            0
        ) > 0
    ) {

        TranslateMessage(&msg);

        DispatchMessageA(&msg);
    }

    return (int)msg.wParam;
}