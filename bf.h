#ifndef BF_H
#define BF_H

// Target Windows 95 / NT 4.0 API level
#define WINVER 0x0400
#define _WIN32_WINNT 0x0400

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h> // For GET_WM_COMMAND_ID
#include <stdio.h>    // For vsprintf, sprintf (use with caution)
#include <stdlib.h>   // For malloc/free
#include <string.h>   // For memset, strlen, strcpy, strncpy, strdup
#include <wchar.h>    // For WEOF (though strsafe.h is removed, good to have for general wide char if ever needed)
#include <commdlg.h>  // For OpenFileName
#include <stdarg.h>   // For va_list, va_start, va_end
#include <dlgs.h>     // Include this for dialog styles
#include <commctrl.h> // Include for Common Control
#include <winreg.h>   // Include for Registry functions
// #include <strsafe.h> // Removed for Windows 95 compatibility

// Resource IDs - These will correspond to IDs in bf.rc
// Menu IDs
#define IDM_MAINMENU        1000 // Main menu resource ID
#define IDM_FILE_NEW        1001
#define IDM_FILE_OPEN       1002
#define IDM_FILE_RUN        1003
#define IDM_FILE_COPYOUTPUT 1004
#define IDM_FILE_CLEAROUTPUT 1005
#define IDM_FILE_SETTINGS   1006
#define IDM_FILE_EXIT       1007
#define IDM_EDIT_CUT        1008
#define IDM_EDIT_COPY       1009
#define IDM_EDIT_PASTE      1010
#define IDM_EDIT_SELECTALL  1011
#define IDM_HELP_ABOUT      1012

// Control IDs for Main Window
#define IDC_STATIC_CODE     2001
#define IDC_EDIT_CODE       2002
#define IDC_STATIC_INPUT    2003
#define IDC_EDIT_INPUT      2004
#define IDC_STATIC_OUTPUT   2005
#define IDC_EDIT_OUTPUT     2006

// Dialog IDs
#define IDD_SETTINGS        3000
#define IDD_ABOUT           4000

// Control IDs for Settings Dialog
#define IDC_CHECK_DEBUG_BASIC       3001
#define IDC_CHECK_DEBUG_INTERPRETER 3002
#define IDC_CHECK_DEBUG_OUTPUT      3003

// Control IDs for About Dialog
#define IDC_STATIC_ABOUT_TEXT 4001

// Accelerator Table ID
#define IDA_ACCELERATORS    5000

// String Table IDs
#define IDS_APP_TITLE                   1
#define IDS_CODE_LABEL                  2
#define IDS_INPUT_LABEL                 3
#define IDS_OUTPUT_LABEL                4
#define IDS_DEFAULT_CODE                5
#define IDS_DEFAULT_INPUT               6
#define IDS_COPIED_TO_CLIPBOARD         7
#define IDS_CRASH_PREFIX                8
#define IDS_MISMATCHED_BRACKETS         9
#define IDS_THREAD_ERROR                10
#define IDS_MEM_ERROR_CODE              11
#define IDS_MEM_ERROR_INPUT             12
#define IDS_MEM_ERROR_PARAMS            13
#define IDS_MEM_ERROR_OPTIMIZE          14
#define IDS_CLIPBOARD_OPEN_ERROR        15
#define IDS_CLIPBOARD_MEM_ALLOC_ERROR   16
#define IDS_CLIPBOARD_MEM_LOCK_ERROR    17
#define IDS_FONT_ERROR                  18
#define IDS_WINDOW_REG_ERROR            19
#define IDS_WINDOW_CREATION_ERROR       20
#define IDS_CONFIRM_EXIT_TITLE          21
#define IDS_REALLY_QUIT_MSG             22
#define IDS_OPEN_FILE_TITLE             23
#define IDS_SETTINGS_TITLE              24
#define IDS_DEBUG_INTERPRETER_CHK       25
#define IDS_DEBUG_OUTPUT_CHK            26
#define IDS_DEBUG_BASIC_CHK             27
#define IDS_OK                          28
#define IDS_CANCEL                      29
#define IDS_ABOUT_TITLE                 30
#define IDS_ABOUT_TEXT                  31
#define IDS_FILE_MENU                   32
#define IDS_EDIT_MENU                   33
#define IDS_HELP_MENU                   34
#define IDS_FILE_NEW_MENU               35
#define IDS_FILE_OPEN_MENU              36
#define IDS_FILE_RUN_MENU               37
#define IDS_FILE_COPYOUTPUT_MENU        38
#define IDS_FILE_CLEAROUTPUT_MENU       39
#define IDS_FILE_SETTINGS_MENU          40
#define IDS_FILE_EXIT_MENU              41
#define IDS_EDIT_CUT_MENU               42
#define IDS_EDIT_COPY_MENU              43
#define IDS_EDIT_PASTE_MENU             44
#define IDS_EDIT_SELECTALL_MENU         45
#define IDS_HELP_ABOUT_MENU             46

// Manifest ID
#define IDR_MANIFEST 1

// --- Custom Messages for Thread Communication ---
#define WM_APP_INTERPRETER_OUTPUT_STRING (WM_APP + 2)
#define WM_APP_INTERPRETER_DONE          (WM_APP + 3)

// --- Constants ---
#define TAPE_SIZE           65536
#define OUTPUT_BUFFER_SIZE  1024
#define MAX_STRING_LENGTH   512

// Registry Constants
#define REG_COMPANY_KEY_ANSI "Software\\Talamar Developments"
#define REG_APP_KEY_ANSI     "Software\\Talamar Developments\\BF Interpreter"
#define REG_VALUE_DEBUG_BASIC_ANSI "DebugBasic"
#define REG_VALUE_DEBUG_INTERPRETER_ANSI "DebugInterpreter"
#define REG_VALUE_DEBUG_OUTPUT_ANSI "DebugOutput"

// Global variables
extern HINSTANCE hInst;
extern HFONT hMonoFont;
extern HFONT hLabelFont;
extern HWND hwndCodeEdit;
extern HWND hwndInputEdit;
extern HWND hwndOutputEdit;
extern HANDLE g_hInterpreterThread;
extern volatile BOOL g_bInterpreterRunning;
extern HACCEL hAccelTable;

// Global debug settings flags
extern volatile BOOL g_bDebugInterpreter;
extern volatile BOOL g_bDebugOutput;
extern volatile BOOL g_bDebugBasic;

// --- Brainfuck Tape Structure ---
typedef struct {
    unsigned char tape[TAPE_SIZE];
    int position;
} Tape;

// --- Interpreter Parameters Structure ---
typedef struct {
    HWND hwndMainWindow;
    char* code;
    char* input;
    int input_len;
    int input_pos;
    char* output_buffer;
    int output_buffer_pos;
} InterpreterParams;

// Function Prototypes
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SettingsDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK AboutDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void DebugPrint(const char* format, ...);
void DebugPrintInterpreter(const char* format, ...);
void DebugPrintOutput(const char* format, ...);
void AppendTextToEditControl(HWND hwndEdit, const char* newText); 
char* LoadStringFromResource(UINT uID, char* buffer, int bufferSize); 

void Tape_init(Tape* tape);
unsigned char Tape_get(Tape* tape);
void Tape_set(Tape* tape, unsigned char value);
void Tape_inc(Tape* tape);
void Tape_dec(Tape* tape);
void Tape_forward(Tape* tape);
void Tape_reverse(Tape* tape);

char* optimize_code(const char* code);
DWORD WINAPI InterpretThreadProc(LPVOID lpParam);
void SendBufferedOutput(InterpreterParams* params);

void SaveDebugSettingsToRegistry(void);
void LoadDebugSettingsFromRegistry(void);

#endif // BF_H
