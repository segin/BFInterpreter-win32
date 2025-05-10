#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h> // For GET_WM_COMMAND_ID
#include <stdio.h>    // For sprintf_s (optional, mainly for debugging or error messages)
#include <stdlib.h>   // For malloc/free
#include <string.h>   // For memset, strlen, strcpy, etc.
#include <commdlg.h>  // For OpenFileName
#include <stdarg.h>   // For va_list, va_start, va_end

// Define Control IDs
#define IDC_STATIC_CODE     101
#define IDC_EDIT_CODE       102
#define IDC_STATIC_INPUT    103
#define IDC_EDIT_INPUT      104
#define IDC_STATIC_OUTPUT   105
#define IDC_EDIT_OUTPUT     106

// Define Menu IDs
#define IDM_FILE_RUN        201
#define IDM_FILE_COPYOUTPUT 202
#define IDM_FILE_EXIT       203
#define IDM_FILE_OPEN       204 // New menu ID for Open
#define IDM_FILE_SETTINGS   205 // New menu ID for Settings
#define IDM_FILE_CLEAROUTPUT 206 // New menu ID for Clear Output

// Define Dialog Control IDs for Settings Dialog
#define IDC_CHECK_DEBUG_INTERPRETER 301
#define IDC_CHECK_DEBUG_OUTPUT      302
#define IDC_CHECK_DEBUG_BASIC       303
// IDOK and IDCANCEL are predefined as 1 and 2

// --- Custom Messages for Thread Communication (ANSI versions) ---
// Message to append a character to output. wParam = character, lParam = 0. (No longer used with buffering)
// #define WM_APP_INTERPRETER_OUTPUT_CHAR (WM_APP + 1)
// Message to append a string to output. wParam = 0, lParam = pointer to string (must be valid when message is processed).
#define WM_APP_INTERPRETER_OUTPUT_STRING (WM_APP + 2)
// Message when interpreter finishes (success or error). wParam = status (0=success, 1=error).
#define WM_APP_INTERPRETER_DONE   (WM_APP + 3)

// --- Constants (ANSI versions) ---
#define WINDOW_TITLE_ANSI        "Win32 BF Interpreter"
#define STRING_CODE_HELP_ANSI    "Code:"
#define STRING_INPUT_HELP_ANSI   "Standard input:"
#define STRING_OUTPUT_HELP_ANSI  "Standard output:"
#define STRING_ACTION_RUN_ANSI   "&Run\tCtrl+R"
#define STRING_ACTION_COPY_ANSI  "&Copy Output\tCtrl+C"
#define STRING_ACTION_EXIT_ANSI  "E&xit\tCtrl+X" // Added Ctrl+X to menu text
#define STRING_ACTION_OPEN_ANSI  "&Open...\tCtrl+O" // Added Open menu text
#define STRING_ACTION_SETTINGS_ANSI "&Settings..." // Added Settings menu text
#define STRING_ACTION_CLEAROUTPUT_ANSI "C&lear Output" // Added Clear Output menu text
#define STRING_FILE_MENU_ANSI    "&File"
#define STRING_CODE_TEXT_ANSI    ",[>,]<[.<]" // Default code
#define STRING_INPUT_TEXT_ANSI   "This is the default stdin-text" // Default input
#define STRING_COPIED_ANSI       "Output copied to clipboard!"
#define STRING_CRASH_PREFIX_ANSI "\r\n\r\n*** Program crashed with Exception:\r\n" // \r\n for Windows newlines
#define STRING_MISMATCHED_BRACKETS_ANSI "Mismatched brackets.\r\n"
#define STRING_THREAD_ERROR_ANSI "Error: Could not create interpreter thread.\r\n"
#define STRING_MEM_ERROR_CODE_ANSI "Error: Memory allocation failed for code.\r\n"
#define STRING_MEM_ERROR_INPUT_ANSI "Error: Memory allocation failed for input.\r\n"
#define STRING_MEM_ERROR_PARAMS_ANSI "Error: Memory allocation failed for thread parameters.\r\n"
#define STRING_MEM_ERROR_OPTIMIZE_ANSI "Memory allocation failed for optimized code.\r\n"
#define STRING_CLIPBOARD_OPEN_ERROR_ANSI "Failed to open clipboard."
#define STRING_CLIPBOARD_MEM_ALLOC_ERROR_ANSI "Failed to allocate memory for clipboard."
#define STRING_CLIPBOARD_MEM_LOCK_ERROR_ANSI "Failed to lock memory for clipboard."
#define STRING_FONT_ERROR_ANSI "Failed to create monospaced font."
#define STRING_WINDOW_REG_ERROR_ANSI "Window Registration Failed!"
#define STRING_WINDOW_CREATION_ERROR_ANSI "Window Creation Failed!"
#define STRING_CONFIRM_EXIT_ANSI "Confirm Exit"
#define STRING_REALLY_QUIT_ANSI "Really quit?"
#define STRING_OPEN_FILE_TITLE_ANSI "Open Brainfuck Source File"
#define STRING_SETTINGS_NOT_IMPLEMENTED_ANSI "Settings option is not yet implemented."
#define STRING_SETTINGS_TITLE_ANSI "Interpreter Settings"
#define STRING_DEBUG_INTERPRETER_ANSI "Enable interpreter instruction debug messages"
#define STRING_DEBUG_OUTPUT_ANSI "Enable interpreter output message debug messages"
#define STRING_DEBUG_BASIC_ANSI "Enable basic debug messages"
#define STRING_OK_ANSI "OK"
#define STRING_CANCEL_ANSI "Cancel"


#define TAPE_SIZE           65536 // Equivalent to 0x10000 in Java Tape.java
#define OUTPUT_BUFFER_SIZE  1024  // Size of the output buffer

// Global variables
HINSTANCE hInst;
HFONT hMonoFont = NULL;
HWND hwndCodeEdit = NULL;
HWND hwndInputEdit = NULL;
HWND hwndOutputEdit = NULL;
HANDLE g_hInterpreterThread = NULL; // Handle for the worker thread
volatile BOOL g_bInterpreterRunning = FALSE; // Flag to indicate if interpreter is running (volatile for thread safety)
HACCEL hAccelTable; // Handle to the accelerator table

// Global debug settings flags (volatile for thread safety)
volatile BOOL g_bDebugInterpreter = TRUE; // Default to TRUE
volatile BOOL g_bDebugOutput = TRUE;      // Default to TRUE
volatile BOOL g_bDebugBasic = TRUE;       // Default to TRUE


// Forward declaration of the window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
// Forward declaration of the settings dialog procedure
INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);


// Helper function to append text to an EDIT control (Defined before use)
// This function is less relevant now with the text replacement approach for output
void AppendText(HWND hwndEdit, const char* newText) {
    // Get the current length of text in the edit control
    int len = GetWindowTextLengthA(hwndEdit);
    // Set the selection to the end of the text
    SendMessageA(hwndEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    // Replace the selection (which is empty and at the end) with the new text
    SendMessageA(hwndEdit, EM_REPLACESEL, 0, (LPARAM)newText);
}

// Helper function for conditional debug output
void DebugPrint(const char* format, ...) {
    if (!g_bDebugBasic) return; // If basic debugging is off, print nothing

    char buffer[512]; // Adjust size as needed
    va_list args;
    va_start(args, format);
    vsprintf_s(buffer, sizeof(buffer), format, args);
    va_end(args);
    OutputDebugStringA(buffer);
}

void DebugPrintInterpreter(const char* format, ...) {
    if (!g_bDebugBasic || !g_bDebugInterpreter) return; // If basic or interpreter debugging is off, print nothing

    char buffer[512]; // Adjust size as needed
    va_list args;
    va_start(args, format);
    vsprintf_s(buffer, sizeof(buffer), format, args);
    va_end(args);
    OutputDebugStringA(buffer);
}

void DebugPrintOutput(const char* format, ...) {
    if (!g_bDebugBasic || !g_bDebugOutput) return; // If basic or output debugging is off, print nothing

    char buffer[512]; // Adjust size as needed
    va_list args;
    va_start(args, format);
    vsprintf_s(buffer, sizeof(buffer), format, args);
    va_end(args);
    OutputDebugStringA(buffer);
}


// --- Brainfuck Tape Structure and Functions (Translation of Tape.java) ---
typedef struct {
    unsigned char tape[TAPE_SIZE];
    int position;
} Tape;

void Tape_init(Tape* tape) {
    memset(tape->tape, 0, TAPE_SIZE);
    tape->position = 0;
}

unsigned char Tape_get(Tape* tape) {
    return tape->tape[tape->position];
}

void Tape_set(Tape* tape, unsigned char value) {
    tape->tape[tape->position] = value;
}

void Tape_inc(Tape* tape) {
    tape->tape[tape->position]++; // Unsigned char wraps around automatically 0-255
}

void Tape_dec(Tape* tape) {
    tape->tape[tape->position]--; // Unsigned char wraps around automatically 0-255
}

void Tape_forward(Tape* tape) {
    tape->position++;
    if (tape->position >= TAPE_SIZE) {
        tape->position = 0; // Wrap around
    }
}

void Tape_reverse(Tape* tape) {
    tape->position--;
    if (tape->position < 0) {
        tape->position = TAPE_SIZE - 1; // Wrap around
    }
}

// --- Interpreter Logic Structure (Translation of Interpreter.java run method) ---

// Structure to pass data to the interpreter thread (using char* for ANSI)
typedef struct {
    HWND hwndMainWindow; // Handle to the main window for posting messages
    char* code;
    char* input;
    int input_len;
    int input_pos;
    char* output_buffer; // Buffer for output characters
    int output_buffer_pos; // Current position in the output buffer
    // Add a copy of the output text from the UI thread to rebuild upon receiving a chunk
    char* current_output_text;
    int current_output_len;
} InterpreterParams;

// Helper function to send buffered output to the main thread
void SendBufferedOutput(InterpreterParams* params) {
    if (params->output_buffer_pos > 0) {
        params->output_buffer[params->output_buffer_pos] = '\0'; // Null-terminate the buffer
        // Allocate memory for the string to be sent to the main thread
        char* output_string = _strdup(params->output_buffer); // Use _strdup for ANSI
        if (output_string) {
            PostMessage(params->hwndMainWindow, WM_APP_INTERPRETER_OUTPUT_STRING, 0, (LPARAM)output_string);
        } else {
             DebugPrint("SendBufferedOutput: Failed to duplicate output string.\n");
             // Optionally, post an error message to the UI
             PostMessage(params->hwndMainWindow, WM_APP_INTERPRETER_OUTPUT_STRING, 0, (LPARAM)"Error: Failed to buffer output.\r\n");
        }
        params->output_buffer_pos = 0; // Reset buffer position
    }
}


// Function to filter Brainfuck code (Translation of Interpreter.java optimize method)
// Allocates a new string, caller must free (using char* for ANSI)
char* optimize_code(const char* code) {
    int len = strlen(code);
    char* ocode = (char*)malloc((len + 1) * sizeof(char));
    if (!ocode) return NULL;

    int ocode_len = 0;
    for (int i = 0; i < len; i++) {
        switch (code[i]) {
            case '>':
            case '<':
            case '+':
            case '-':
            case ',':
            case '.':
            case '[':
            case ']':
                ocode[ocode_len++] = code[i];
                break;
            default:
                // Ignore other characters
                break;
        }
    }
    ocode[ocode_len] = '\0';
    return ocode;
}


// The Brainfuck interpreter function running in a separate thread (ANSI version)
DWORD WINAPI InterpretThreadProc(LPVOID lpParam) {
    DebugPrintInterpreter("Interpreter thread started.\n");

    InterpreterParams* params = (InterpreterParams*)lpParam;
    Tape tape;
    Tape_init(&tape);

    char* ocode = optimize_code(params->code);
    if (!ocode) {
        DebugPrintInterpreter("InterpretThreadProc: Failed to optimize code (memory allocation).\n");
        PostMessage(params->hwndMainWindow, WM_APP_INTERPRETER_OUTPUT_STRING, 0, (LPARAM)STRING_MEM_ERROR_OPTIMIZE_ANSI);
        PostMessage(params->hwndMainWindow, WM_APP_INTERPRETER_DONE, 1, 0); // Indicate error
        // Free parameters passed to the thread
        free(params->code);
        free(params->input);
        free(params->output_buffer); // Free output buffer
        if (params->current_output_text) free(params->current_output_text); // Free current output text
        free(params);
        // g_hInterpreterThread = NULL; // This should be handled by the main thread or a more robust mechanism
        g_bInterpreterRunning = FALSE; // Use simple assignment for volatile bool
        return 1; // Indicate failure
    }

    int pc = 0;
    int ocode_len = strlen(ocode);
    int error_status = 0; // 0 for success, 1 for error

    DebugPrintInterpreter("InterpretThreadProc: Starting main loop.\n");

    // Standard C error handling for bracket mismatches
    while (pc < ocode_len && g_bInterpreterRunning && error_status == 0) {
        // Optional: Add a small sleep here for very fast loops to ensure UI responsiveness
        // Sleep(1); // Sleep for 1 millisecond

        char current_instruction = ocode[pc];
        DebugPrintInterpreter("PC: %d, Instruction: %c\n", pc, current_instruction);


        switch (current_instruction) {
            case '>':
                Tape_forward(&tape);
                pc++; // Increment PC after instruction
                break;
            case '<':
                Tape_reverse(&tape);
                pc++; // Increment PC after instruction
                break;
            case '+':
                Tape_inc(&tape);
                pc++; // Increment PC after instruction
                break;
            case '-':
                Tape_dec(&tape);
                pc++; // Increment PC after instruction
                break;
            case ',':
                if (params->input_pos < params->input_len) {
                    Tape_set(&tape, (unsigned char)params->input[params->input_pos]);
                    params->input_pos++;
                } else {
                    // Panu Kalliokoski behavior: input past end of string is 0
                    Tape_set(&tape, 0);
                }
                pc++; // Increment PC after instruction
                break;
            case '.':
                // Append character to buffer instead of posting message immediately
                if (params->output_buffer_pos < OUTPUT_BUFFER_SIZE - 1) {
                    params->output_buffer[params->output_buffer_pos++] = Tape_get(&tape);
                } else {
                    // Buffer is full, send it to the main thread
                    SendBufferedOutput(params);
                     // Add the current character to the now-empty buffer
                    params->output_buffer[params->output_buffer_pos++] = Tape_get(&tape);
                }
                // Yield to other threads/processes periodically to keep UI responsive
                // Sleep(0); // Yields execution time slice - uncomment if needed for very long loops without output
                pc++; // Increment PC after instruction
                break;
            case '[':
                if (Tape_get(&tape) == 0) {
                    // If cell is zero, find the matching ']'
                    int bracket_count = 1;
                    int current_pc = pc;
                    while (bracket_count > 0 && ++current_pc < ocode_len) {
                        if (ocode[current_pc] == '[') bracket_count++;
                        else if (ocode[current_pc] == ']') bracket_count--;
                    }
                    if (current_pc >= ocode_len) { // Reached end without finding matching ']'
                         error_status = 1;
                         DebugPrintInterpreter("InterpretThreadProc: Mismatched opening bracket found (no matching closing bracket).\n");
                    } else {
                        pc = current_pc + 1; // Jump *past* the matching ']'
                    }
                } else {
                    // If cell is non-zero, just move to the next instruction
                    pc++;
                }
                break;
            case ']':
                if (Tape_get(&tape) != 0) {
                    // If cell is non-zero, find the matching '['
                    int bracket_count = 1;
                    int current_pc = pc;
                    while (bracket_count > 0 && --current_pc >= 0) {
                        if (ocode[current_pc] == ']') bracket_count++;
                        else if (ocode[current_pc] == '[') bracket_count--;
                    }
                     if (current_pc < 0) { // Reached beginning without finding matching '['
                         error_status = 1;
                         DebugPrintInterpreter("InterpretThreadProc: Mismatched closing bracket found (no matching opening bracket).\n");
                     } else {
                        pc = current_pc; // Jump *to* the matching '['
                     }
                } else {
                    // If cell is zero, just move to the next instruction
                    pc++;
                }
                break;
        }
        // Check for external stop signal periodically
        if (!g_bInterpreterRunning) { // Accessing volatile bool
             DebugPrintInterpreter("InterpretThreadProc: Stop signal received.\n");
             break; // Exit the loop if stop is requested
        }
    }

    // Send any remaining buffered output before finishing
    SendBufferedOutput(params);

    // Check for mismatched brackets after loop finishes normally (if no error yet)
    // This check is redundant if the loop logic correctly sets error_status on mismatch
    // but keeping it doesn't hurt and might catch edge cases.
    if (error_status == 0 && pc < ocode_len && (ocode[pc] == '[' || ocode[pc] == ']')) {
       error_status = 1; // Mismatched brackets detected at the end
       DebugPrintInterpreter("InterpretThreadProc: Mismatched brackets detected at end of code.\n");
    }

    // Post specific error message if it was a bracket mismatch
    if (error_status == 1) {
        PostMessage(params->hwndMainWindow, WM_APP_INTERPRETER_OUTPUT_STRING, 0, (LPARAM)STRING_MISMATCHED_BRACKETS_ANSI);
    } else if (g_bInterpreterRunning) { // Only signal success if not stopped externally
        DebugPrintInterpreter("InterpretThreadProc: Interpretation finished successfully.\n");
    }


    // Signal completion to the main thread
    PostMessage(params->hwndMainWindow, WM_APP_INTERPRETER_DONE, error_status, 0);

    // Free allocated memory
    free(ocode);
    free(params->code);
    free(params->input);
    free(params->output_buffer); // Free output buffer
    if (params->current_output_text) free(params->current_output_text); // Free current output text
    free(params);
    // g_hInterpreterThread = NULL; // This should be handled by the main thread or a more robust mechanism
    g_bInterpreterRunning = FALSE; // Use simple assignment for volatile bool

    DebugPrintInterpreter("Interpreter thread exiting.\n");
    return error_status; // Return status
}

// --- Dialog Template Structure (Wide Character Version) ---
// This structure must match the DLGTEMPLATEEX format precisely.
// It will be followed by wide character strings for menu, class, and title,
// and potentially font information if DS_SETFONT is used.
#pragma pack(push, 1) // Ensure byte alignment
typedef struct {
    WORD      dlgVer;
    WORD      signature;
    DWORD     helpID;
    DWORD     exStyle;
    DWORD     style;
    WORD      cDlgItems;
    short     x;
    short     y;
    short     cx;
    short     cy;
    // Followed by:
    // - Menu (WORD ordinal or null-terminated WCHAR string)
    // - Class (WORD ordinal or null-terminated WCHAR string)
    // - Title (null-terminated WCHAR string)
    // - Font (if DS_SETFONT): WORD pointsize, BYTE weight, BYTE italic, null-terminated WCHAR typeface
} MY_DLGTEMPLATEEX_WIDE;
#pragma pack(pop)


// --- Settings Dialog Procedure ---
INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    // Declare HWND variables before the switch
    HWND hCheckInterpreter;
    HWND hCheckOutput;
    HWND hCheckBasic;
    HWND hBtnOK;
    HWND hBtnCancel;

    switch (message) {
        case WM_INITDIALOG:
            DebugPrint("SettingsDialogProc: WM_INITDIALOG received.\n");
            // Create controls dynamically and initialize the declared HWND variables
            hCheckInterpreter = CreateWindowA("BUTTON", STRING_DEBUG_INTERPRETER_ANSI,
                          WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                          10, 10, 300, 20, hDlg, (HMENU)IDC_CHECK_DEBUG_INTERPRETER, hInst, NULL);
            if (hCheckInterpreter == NULL) DebugPrint("SettingsDialogProc: Failed to create IDC_CHECK_DEBUG_INTERPRETER. Error: %lu\n", GetLastError());

            hCheckOutput = CreateWindowA("BUTTON", STRING_DEBUG_OUTPUT_ANSI,
                          WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                          10, 35, 300, 20, hDlg, (HMENU)IDC_CHECK_DEBUG_OUTPUT, hInst, NULL);
             if (hCheckOutput == NULL) DebugPrint("SettingsDialogProc: Failed to create IDC_CHECK_DEBUG_OUTPUT. Error: %lu\n", GetLastError());

            hCheckBasic = CreateWindowA("BUTTON", STRING_DEBUG_BASIC_ANSI,
                          WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                          10, 60, 300, 20, hDlg, (HMENU)IDC_CHECK_DEBUG_BASIC, hInst, NULL);
             if (hCheckBasic == NULL) DebugPrint("SettingsDialogProc: Failed to create IDC_CHECK_DEBUG_BASIC. Error: %lu\n", GetLastError());

            hBtnOK = CreateWindowA("BUTTON", STRING_OK_ANSI,
                          WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                          80, 100, 75, 25, hDlg, (HMENU)IDOK, hInst, NULL);
             if (hBtnOK == NULL) DebugPrint("SettingsDialogProc: Failed to create IDOK button. Error: %lu\n", GetLastError());

            hBtnCancel = CreateWindowA("BUTTON", STRING_CANCEL_ANSI,
                          WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                          160, 100, 75, 25, hDlg, (HMENU)IDCANCEL, hInst, NULL);
             if (hBtnCancel == NULL) DebugPrint("SettingsDialogProc: Failed to create IDCANCEL button. Error: %lu\n", GetLastError());


            // Initialize checkboxes based on current global settings
            CheckDlgButton(hDlg, IDC_CHECK_DEBUG_INTERPRETER, g_bDebugInterpreter ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_CHECK_DEBUG_OUTPUT, g_bDebugOutput ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_CHECK_DEBUG_BASIC, g_bDebugBasic ? BST_CHECKED : BST_UNCHECKED);
            return (INT_PTR)TRUE; // Return TRUE to set the keyboard focus
        case WM_COMMAND:
        { // Added braces to create a new scope
            DebugPrint("SettingsDialogProc: WM_COMMAND received. LOWORD(wParam): %u, HIWORD(wParam): %u, lParam: %p\n", LOWORD(wParam), HIWORD(wParam), (void*)lParam);
            switch (LOWORD(wParam)) {
                case IDOK:
                    DebugPrint("SettingsDialogProc: IDOK received.\n");
                    // Save settings from checkboxes
                    g_bDebugInterpreter = IsDlgButtonChecked(hDlg, IDC_CHECK_DEBUG_INTERPRETER) == BST_CHECKED;
                    g_bDebugOutput = IsDlgButtonChecked(hDlg, IDC_CHECK_DEBUG_OUTPUT) == BST_CHECKED;
                    g_bDebugBasic = IsDlgButtonChecked(hDlg, IDC_CHECK_DEBUG_BASIC) == BST_CHECKED;

                    // Apply the rule: if basic is off, all are off
                    if (!g_bDebugBasic) {
                        g_bDebugInterpreter = FALSE;
                        g_bDebugOutput = FALSE;
                    }
                    DebugPrint("SettingsDialogProc: Debug settings saved. Basic: %d, Interpreter: %d, Output: %d\n", g_bDebugBasic, g_bInterpreter, g_bDebugOutput);


                    EndDialog(hDlg, LOWORD(wParam)); // Close the dialog
                    return (INT_PTR)TRUE;
                case IDCANCEL:
                    DebugPrint("SettingsDialogProc: IDCANCEL received.\n");
                    EndDialog(hDlg, LOWORD(wParam)); // Close the dialog
                    return (INT_PTR)TRUE;
            }
            break;
        } // Added braces to create a new scope
    }
    return (INT_PTR)FALSE; // Let the system handle other messages
}


// --- Window Procedure ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_CREATE:
        {
            DebugPrint("WM_CREATE received.\n");
            // --- Create Monospaced Font ---
            hMonoFont = CreateFontA(
                16,                     // Height
                0,                      // Width (auto)
                0,                      // Escapement
                0,                      // Orientation
                FW_NORMAL,              // Weight
                FALSE,                  // Italic
                FALSE,                  // Underline
                FALSE,                  // Strikeout
                ANSI_CHARSET,           // Charset
                OUT_DEFAULT_PRECIS,     // Output Precision
                CLIP_DEFAULT_PRECIS,    // Clipping Precision
                DEFAULT_QUALITY,        // Quality
                FIXED_PITCH | FF_MODERN,// Pitch and Family (FIXED_PITCH is key for mono)
                "Courier New");         // Font name (ANSI)

            if (hMonoFont == NULL) {
                 MessageBoxA(hwnd, STRING_FONT_ERROR_ANSI, "Font Error", MB_OK | MB_ICONWARNING);
            }

            // --- Create Menu ---
            HMENU hMenubar = CreateMenu();
            HMENU hMenuFile = CreateMenu();

            AppendMenuA(hMenuFile, MF_STRING, IDM_FILE_OPEN, STRING_ACTION_OPEN_ANSI); // Added Open menu item
            AppendMenuA(hMenuFile, MF_STRING, IDM_FILE_RUN, STRING_ACTION_RUN_ANSI);
            AppendMenuA(hMenuFile, MF_STRING, IDM_FILE_COPYOUTPUT, STRING_ACTION_COPY_ANSI);
            AppendMenuA(hMenuFile, MF_STRING, IDM_FILE_CLEAROUTPUT, STRING_ACTION_CLEAROUTPUT_ANSI); // Added Clear Output menu item
            AppendMenuA(hMenuFile, MF_SEPARATOR, 0, NULL);
            AppendMenuA(hMenuFile, MF_STRING, IDM_FILE_SETTINGS, STRING_ACTION_SETTINGS_ANSI); // Added Settings menu item
            AppendMenuA(hMenuFile, MF_SEPARATOR, 0, NULL);
            AppendMenuA(hMenuFile, MF_STRING, IDM_FILE_EXIT, STRING_ACTION_EXIT_ANSI); // Updated menu text

            AppendMenuA(hMenubar, MF_POPUP, (UINT_PTR)hMenuFile, STRING_FILE_MENU_ANSI);

            SetMenu(hwnd, hMenubar);

            // --- Create Controls ---
            // Labels (STATIC)
            CreateWindowA("STATIC", STRING_CODE_HELP_ANSI, WS_CHILD | WS_VISIBLE,
                          10, 10, 100, 20, hwnd, (HMENU)IDC_STATIC_CODE, hInst, NULL);
            CreateWindowA("STATIC", STRING_INPUT_HELP_ANSI, WS_CHILD | WS_VISIBLE,
                          10, 170, 150, 20, hwnd, (HMENU)IDC_STATIC_INPUT, hInst, NULL);
            CreateWindowA("STATIC", STRING_OUTPUT_HELP_ANSI, WS_CHILD | WS_VISIBLE,
                          10, 300, 150, 20, hwnd, (HMENU)IDC_STATIC_OUTPUT, hInst, NULL);

            // Edit Controls (EDIT)
            // Code Input
            hwndCodeEdit = CreateWindowExA(
                WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_TABSTOP,
                10, 35, 560, 125, hwnd, (HMENU)IDC_EDIT_CODE, hInst, NULL);

            // Standard Input
            hwndInputEdit = CreateWindowExA(
                WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_TABSTOP,
                10, 195, 560, 95, hwnd, (HMENU)IDC_EDIT_INPUT, hInst, NULL);

            // Standard Output (Read-only)
            hwndOutputEdit = CreateWindowExA(
                WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                10, 325, 560, 150, hwnd, (HMENU)IDC_EDIT_OUTPUT, hInst, NULL);

            // Set initial text (ANSI)
            SetWindowTextA(hwndCodeEdit, STRING_CODE_TEXT_ANSI);
            SetWindowTextA(hwndInputEdit, STRING_INPUT_TEXT_ANSI);
            SetWindowTextA(hwndOutputEdit, "");

            // Apply the monospaced font to all EDIT controls
            if (hMonoFont) {
                SendMessageA(hwndCodeEdit, WM_SETFONT, (WPARAM)hMonoFont, MAKELPARAM(TRUE, 0));
                SendMessageA(hwndInputEdit, WM_SETFONT, (WPARAM)hMonoFont, MAKELPARAM(TRUE, 0));
                SendMessageA(hwndOutputEdit, WM_SETFONT, (WPARAM)hMonoFont, MAKELPARAM(TRUE, 0));
            }

            SetFocus(hwndCodeEdit);
            break; // End of WM_CREATE
        }

        case WM_SIZE:
        {
            // Handle window resizing - adjust control positions and sizes
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);

            int margin = 10;
            int labelHeight = 20;
            int editTopMargin = 5;
            int spacing = 10;
            int minimumEditHeight = 30; // Minimum height for any edit box

            int currentY = margin;

            // Code Label & Edit
            MoveWindow(GetDlgItem(hwnd, IDC_STATIC_CODE), margin, currentY, width - 2 * margin, labelHeight, TRUE);
            currentY += labelHeight + editTopMargin;
            int codeEditHeight = height / 4; // Approximate height
            if (codeEditHeight < minimumEditHeight) codeEditHeight = minimumEditHeight; // Ensure minimum
            MoveWindow(hwndCodeEdit, margin, currentY, width - 2 * margin, codeEditHeight, TRUE);
            currentY += codeEditHeight + spacing;

            // Input Label & Edit
            MoveWindow(GetDlgItem(hwnd, IDC_STATIC_INPUT), margin, currentY, width - 2 * margin, labelHeight, TRUE);
            currentY += labelHeight + editTopMargin;
            int inputEditHeight = height / 6; // Approximate height
            if (inputEditHeight < minimumEditHeight) inputEditHeight = minimumEditHeight; // Ensure minimum
            MoveWindow(hwndInputEdit, margin, currentY, width - 2 * margin, inputEditHeight, TRUE);
            currentY += inputEditHeight + spacing;

            // Output Label & Edit
            MoveWindow(GetDlgItem(hwnd, IDC_STATIC_OUTPUT), margin, currentY, width - 2 * margin, labelHeight, TRUE);
            currentY += labelHeight + editTopMargin;
            int outputEditHeight = height - currentY - margin; // Remaining space
            if (outputEditHeight < minimumEditHeight) outputEditHeight = minimumEditHeight; // Ensure minimum
            MoveWindow(hwndOutputEdit, margin, currentY, width - 2 * margin, outputEditHeight, TRUE);

            break; // End of WM_SIZE
        }


        case WM_COMMAND:
        {
            DebugPrint("WM_COMMAND received.\n");
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
                case IDM_FILE_OPEN:
                {
                    DebugPrint("WM_COMMAND: IDM_FILE_OPEN received.\n");
                    OPENFILENAMEA ofn;       // Structure for the file dialog
                    char szFile[260] = { 0 }; // Buffer for file name

                    // Initialize OPENFILENAME
                    ZeroMemory(&ofn, sizeof(ofn));
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hwnd;
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = sizeof(szFile);
                    // Filter for Brainfuck files and all files
                    ofn.lpstrFilter = "Brainfuck Source Code (*.bf)\0*.bf\0All Files (*.*)\0*.*\0";
                    ofn.nFilterIndex = 1; // Default to the first filter (Brainfuck files)
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
                    ofn.lpstrTitle = STRING_OPEN_FILE_TITLE_ANSI;


                    // Display the Open dialog box.
                    if (GetOpenFileNameA(&ofn) == TRUE)
                    {
                        DebugPrint("IDM_FILE_OPEN: File selected.\n");
                        // User selected a file, now read its content
                        HANDLE hFile = CreateFileA(ofn.lpstrFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

                        if (hFile != INVALID_HANDLE_VALUE)
                        {
                            DebugPrint("IDM_FILE_OPEN: File opened successfully.\n");
                            DWORD fileSize = GetFileSize(hFile, NULL);
                            if (fileSize != INVALID_FILE_SIZE)
                            {
                                char* fileContent = (char*)malloc(fileSize + 1);
                                if (fileContent)
                                {
                                    DWORD bytesRead;
                                    if (ReadFile(hFile, fileContent, fileSize, &bytesRead, NULL))
                                    {
                                        fileContent[bytesRead] = '\0'; // Null-terminate the content
                                        SetWindowTextA(hwndCodeEdit, fileContent); // Set code edit text
                                        DebugPrint("IDM_FILE_OPEN: File content loaded into code edit.\n");
                                    }
                                    else
                                    {
                                        DebugPrint("IDM_FILE_OPEN: Error reading file.\n");
                                        MessageBoxA(hwnd, "Error reading file.", "File Error", MB_OK | MB_ICONERROR);
                                    }
                                    free(fileContent);
                                }
                                else
                                {
                                    DebugPrint("IDM_FILE_OPEN: Memory allocation failed for file content.\n");
                                    MessageBoxA(hwnd, STRING_MEM_ERROR_CODE_ANSI, "Memory Error", MB_OK | MB_ICONERROR);
                                }
                            }
                            else
                            {
                                DebugPrint("IDM_FILE_OPEN: Error getting file size.\n");
                                MessageBoxA(hwnd, "Error getting file size.", "File Error", MB_OK | MB_ICONERROR);
                            }
                            CloseHandle(hFile);
                        }
                        else
                        {
                             char error_msg[256];
                             sprintf_s(error_msg, sizeof(error_msg), "IDM_FILE_OPEN: Error creating file handle: %lu\n", GetLastError());
                             DebugPrint(error_msg);
                             MessageBoxA(hwnd, "Error opening file.", "File Error", MB_OK | MB_ICONERROR);
                        }
                    } else {
                         DebugPrint("IDM_FILE_OPEN: File selection cancelled or failed.\n");
                         // User cancelled or an error occurred (can check CommDlgExtendedError())
                    }
                    break;
                }

                case IDM_FILE_SETTINGS:
                {
                    DebugPrint("WM_COMMAND: IDM_FILE_SETTINGS received. Attempting to show dialog.\n");

                    // Convert ANSI title to Wide Character title
                    int title_len_ansi = strlen(STRING_SETTINGS_TITLE_ANSI);
                    int title_len_wide = MultiByteToWideChar(CP_ACP, 0, STRING_SETTINGS_TITLE_ANSI, title_len_ansi, NULL, 0);
                    WCHAR* settings_dialog_title_wide = (WCHAR*)malloc((title_len_wide + 1) * sizeof(WCHAR));
                    if (!settings_dialog_title_wide) {
                        DebugPrint("IDM_FILE_SETTINGS: Failed to allocate memory for wide title string.\n");
                        // Optionally, show an error message box
                        break;
                    }
                    MultiByteToWideChar(CP_ACP, 0, STRING_SETTINGS_TITLE_ANSI, title_len_ansi, settings_dialog_title_wide, title_len_wide);
                    settings_dialog_title_wide[title_len_wide] = L'\0'; // Null-terminate wide string

                    // Calculate the required size for the template in memory
                    // Size of the base structure
                    size_t template_base_size = sizeof(MY_DLGTEMPLATEEX_WIDE);

                    // Size of the menu name (WORD ordinal 0xFFFF followed by WORD 0 for no menu)
                    size_t menu_size = sizeof(WORD) + sizeof(WORD); // 0xFFFF, 0

                    // Size of the class name (WORD ordinal 0xFFFF followed by WORD 0 for default dialog class)
                    size_t class_size = sizeof(WORD) + sizeof(WORD); // 0xFFFF, 0

                     // Size of the wide title string including null terminator
                    size_t title_string_size_wide = (title_len_wide + 1) * sizeof(WCHAR);

                    // Calculate total required size, ensuring proper alignment between sections.
                    // The structure is DWORD aligned. Menu follows, WORD aligned. Class follows, WORD aligned.
                    // Title follows, WORD aligned.
                    size_t current_offset = template_base_size;

                    // Align for menu
                    current_offset = (current_offset + sizeof(WORD) - 1) & ~(sizeof(WORD) - 1);
                    size_t offset_menu = current_offset;
                    current_offset += menu_size;

                    // Align for class
                    current_offset = (current_offset + sizeof(WORD) - 1) & ~(sizeof(WORD) - 1);
                    size_t offset_class = current_offset;
                    current_offset += class_size;

                    // Align for title
                    current_offset = (current_offset + sizeof(WORD) - 1) & ~(sizeof(WORD) - 1);
                    size_t offset_title = current_offset;
                    current_offset += title_string_size_wide;

                    // Final size must be DWORD aligned
                    size_t total_template_size = (current_offset + sizeof(DWORD) - 1) & ~(sizeof(DWORD) - 1);


                    DebugPrint("IDM_FILE_SETTINGS: Calculated total template size: %zu\n", total_template_size);
                    DebugPrint("IDM_FILE_SETTINGS: Offsets - Base: 0, Menu: %zu, Class: %zu, Title: %zu\n", 0, offset_menu, offset_class, offset_title);


                    // Allocate memory for the combined template and data
                    HGLOBAL hGlobalTemplate = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, total_template_size);
                    DebugPrint("IDM_FILE_SETTINGS: GlobalAlloc returned %p\n", hGlobalTemplate);

                    if (hGlobalTemplate) {
                        LPBYTE pGlobalTemplate = (LPBYTE)GlobalLock(hGlobalTemplate);
                        DebugPrint("IDM_FILE_SETTINGS: GlobalLock returned %p\n", pGlobalTemplate);

                        if (pGlobalTemplate) {
                            // Copy the fixed template structure
                            MY_DLGTEMPLATEEX_WIDE template_fixed_part = {
                                0x0001, // dlgVer
                                0xFFFF, // signature (indicates DLGTEMPLATEEX)
                                0,      // helpID
                                0,      // exStyle
                                WS_POPUP | WS_BORDER | WS_SYSMENU | DS_MODALFRAME | WS_CAPTION, // style (removed DS_SETFONT)
                                0,      // cDlgItems (we add controls dynamically)
                                100,    // x (arbitrary position)
                                100,    // y (arbitrary position)
                                350,    // cx (width)
                                150     // cy (height)
                            };
                            memcpy(pGlobalTemplate, &template_fixed_part, sizeof(MY_DLGTEMPLATEEX_WIDE));
                            DebugPrint("IDM_FILE_SETTINGS: Copied fixed template structure.\n");

                            // Copy menu name (ordinal for no menu)
                            LPWORD pMenu = (LPWORD)(pGlobalTemplate + offset_menu);
                            *pMenu++ = 0xFFFF; // Indicates ordinal
                            *pMenu = 0;      // Ordinal 0 for no menu
                            DebugPrint("IDM_FILE_SETTINGS: Copied menu ordinal.\n");


                            // Copy class name (ordinal for default dialog class)
                            LPWORD pClass = (LPWORD)(pGlobalTemplate + offset_class);
                            *pClass++ = 0xFFFF; // Indicates ordinal
                            *pClass = 0;      // Ordinal 0 for default dialog class
                            DebugPrint("IDM_FILE_SETTINGS: Copied class ordinal.\n");


                            // Copy the wide title string
                            LPWSTR pTitle = (LPWSTR)(pGlobalTemplate + offset_title);
                            memcpy(pTitle, settings_dialog_title_wide, title_string_size_wide);
                            DebugPrint("IDM_FILE_SETTINGS: Copied wide title string.\n");


                            GlobalUnlock(hGlobalTemplate);
                            DebugPrint("IDM_FILE_SETTINGS: GlobalUnlock called.\n");


                            // Call DialogBoxIndirectW with the memory handle
                            DebugPrint("IDM_FILE_SETTINGS: Calling DialogBoxIndirectW...\n");
                            INT_PTR dialog_result = DialogBoxIndirectW(hInst, (LPCDLGTEMPLATE)hGlobalTemplate, hwnd, SettingsDialogProc);
                            DebugPrint("IDM_FILE_SETTINGS: DialogBoxIndirectW returned %Id.\n", dialog_result);


                            if (dialog_result == -1) {
                                DebugPrint("IDM_FILE_SETTINGS: GetLastError after DialogBoxIndirectW: %lu\n", GetLastError());
                            }

                            DebugPrint("IDM_FILE_SETTINGS: Freeing global memory.\n");
                            GlobalFree(hGlobalTemplate); // Free the allocated memory

                        } else {
                            DebugPrint("IDM_FILE_SETTINGS: GlobalLock failed for dialog template. GetLastError: %lu\n", GetLastError());
                        }
                    } else {
                        DebugPrint("IDM_FILE_SETTINGS: GlobalAlloc failed for dialog template. GetLastError: %lu\n", GetLastError());
                    }

                    // Free the allocated wide title string
                    free(settings_dialog_title_wide);
                    DebugPrint("IDM_FILE_SETTINGS: Freed wide title string memory.\n");


                    DebugPrint("WM_COMMAND: Settings dialog process finished.\n");
                    break;
                }

                case IDM_FILE_RUN:
                { // Added braces to scope variables
                    DebugPrint("WM_COMMAND: IDM_FILE_RUN received.\n");
                    if (!g_bInterpreterRunning) {
                        DebugPrint("IDM_FILE_RUN: Interpreter not running, attempting to start.\n");
                        // Clear previous output
                        SetWindowTextA(hwndOutputEdit, "");

                        // Get code and input text (ANSI)
                        int code_len = GetWindowTextLengthA(hwndCodeEdit);
                        char* code = (char*)malloc((code_len + 1) * sizeof(char));
                        if (!code) {
                            DebugPrint("IDM_FILE_RUN: Memory allocation failed for code.\n");
                            SetWindowTextA(hwndOutputEdit, STRING_MEM_ERROR_CODE_ANSI);
                            break;
                        }
                        GetWindowTextA(hwndCodeEdit, code, code_len + 1);
                        DebugPrint("IDM_FILE_RUN: Code text obtained.\n");

                        int input_len = GetWindowTextLengthA(hwndInputEdit);
                        char* input = (char*)malloc((input_len + 1) * sizeof(char));
                         if (!input) {
                            DebugPrint("IDM_FILE_RUN: Memory allocation failed for input.\n");
                            SetWindowTextA(hwndOutputEdit, STRING_MEM_ERROR_INPUT_ANSI);
                            free(code); // Free previously allocated code
                            break;
                        }
                        GetWindowTextA(hwndInputEdit, input, input_len + 1);
                        DebugPrint("IDM_FILE_RUN: Input text obtained.\n");

                        // Prepare parameters for the thread
                        InterpreterParams* params = (InterpreterParams*)malloc(sizeof(InterpreterParams));
                        if (!params) {
                            DebugPrint("IDM_FILE_RUN: Memory allocation failed for thread parameters.\n");
                            SetWindowTextA(hwndOutputEdit, STRING_MEM_ERROR_PARAMS_ANSI);
                            free(code);
                            free(input);
                            break;
                        }
                        params->hwndMainWindow = hwnd;
                        params->code = code;
                        params->input = input;
                        params->input_len = input_len;
                        params->input_pos = 0;
                        // Allocate output buffer
                        params->output_buffer = (char*)malloc(OUTPUT_BUFFER_SIZE);
                        if (!params->output_buffer) {
                             DebugPrint("IDM_FILE_RUN: Memory allocation failed for output buffer.\n");
                             SetWindowTextA(hwndOutputEdit, "Error: Memory allocation failed for output buffer.\r\n");
                             free(code);
                             free(input);
                             free(params);
                             break;
                        }
                        params->output_buffer_pos = 0;
                        // Initialize current_output_text
                        params->current_output_text = _strdup("");
                        params->current_output_len = 0;

                        DebugPrint("IDM_FILE_RUN: InterpreterParams prepared.\n");

                        // Create and start the interpreter thread
                        g_bInterpreterRunning = TRUE; // Use simple assignment for volatile bool
                        g_hInterpreterThread = CreateThread(NULL, 0, InterpretThreadProc, params, 0, NULL);
                        DebugPrint("IDM_FILE_RUN: CreateThread called.\n");

                        if (g_hInterpreterThread == NULL) {
                            // Handle thread creation failure
                            g_bInterpreterRunning = FALSE; // Use simple assignment for volatile bool
                            char error_msg[256];
                            sprintf_s(error_msg, sizeof(error_msg), "IDM_FILE_RUN: CreateThread failed with error %lu\n", GetLastError());
                            DebugPrint(error_msg);
                            SetWindowTextA(hwndOutputEdit, STRING_THREAD_ERROR_ANSI);
                            free(code);
                            free(input);
                            free(params->output_buffer); // Free output buffer
                            if (params->current_output_text) free(params->current_output_text);
                            free(params);
                        } else {
                            DebugPrint("IDM_FILE_RUN: CreateThread succeeded.\n");
                            // Close the handle immediately since we don't need it after creating the thread
                            CloseHandle(g_hInterpreterThread);
                            g_hInterpreterThread = NULL; // Set to NULL as the handle is closed
                            // The thread will set g_bInterpreterRunning to FALSE when it finishes.
                        }
                    } else {
                        DebugPrint("IDM_FILE_RUN: Interpreter already running.\n");
                        // Optionally, display a message that interpreter is already running
                        // AppendText(hwndOutputEdit, "--- Interpreter is already running ---\r\n");
                    }
                    break; // Break for IDM_FILE_RUN case
                } // End brace for IDM_FILE_RUN scope

                case IDM_FILE_COPYOUTPUT:
                { // Added braces to scope variables
                    DebugPrint("WM_COMMAND: IDM_FILE_COPYOUTPUT received.\n");
                    // Check if any text is selected in the output edit control
                    DWORD char_start, char_end;
                    SendMessageA(hwndOutputEdit, EM_GETSEL, (WPARAM)&char_start, (LPARAM)&char_end);

                    if (char_start == char_end) { // No text is selected
                         DebugPrint("IDM_FILE_COPYOUTPUT: No text selected, copying entire output.\n");
                        int textLen = GetWindowTextLengthA(hwndOutputEdit);
                        if (textLen > 0) {
                             DebugPrint("IDM_FILE_COPYOUTPUT: Output text length > 0.\n");
                            // Need +1 for null terminator
                            HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, textLen + 1);
                            if (hGlobal) {
                                DebugPrint("IDM_FILE_COPYOUTPUT: GlobalAlloc succeeded.\n");
                                char* pText = (char*)GlobalLock(hGlobal);
                                if(pText) {
                                    DebugPrint("IDM_FILE_COPYOUTPUT: GlobalLock succeeded.\n");
                                    GetWindowTextA(hwndOutputEdit, pText, textLen + 1);
                                    GlobalUnlock(hGlobal);
                                    DebugPrint("IDM_FILE_COPYOUTPUT: Text copied to global memory.\n");

                                    if (OpenClipboard(hwnd)) {
                                        DebugPrint("IDM_FILE_COPYOUTPUT: OpenClipboard succeeded.\n");
                                        EmptyClipboard();
                                        // Use CF_TEXT for ANSI text
                                        SetClipboardData(CF_TEXT, hGlobal);
                                        CloseClipboard();
                                        DebugPrint("IDM_FILE_COPYOUTPUT: Clipboard data set.\n");
                                        // Set hGlobal to NULL so we don't free it below,
                                        // the system owns it now.
                                        hGlobal = NULL;
                                        MessageBoxA(hwnd, STRING_COPIED_ANSI, WINDOW_TITLE_ANSI, MB_OK | MB_ICONINFORMATION);
                                    } else {
                                        DebugPrint("IDM_FILE_COPYOUTPUT: Failed to open clipboard.\n");
                                        MessageBoxA(hwnd, STRING_CLIPBOARD_OPEN_ERROR_ANSI, "Error", MB_OK | MB_ICONERROR);
                                    }
                                } else {
                                    DebugPrint("IDM_FILE_COPYOUTPUT: Failed to lock memory.\n");
                                     MessageBoxA(hwnd, STRING_CLIPBOARD_MEM_LOCK_ERROR_ANSI, "Error", MB_OK | MB_ICONERROR);
                                }
                                // If hGlobal is not NULL here, it means SetClipboardData failed or
                                // was not called, so we should free the memory we allocated.
                                if (hGlobal) {
                                    DebugPrint("IDM_FILE_COPYOUTPUT: Freeing global memory.\n");
                                    GlobalFree(hGlobal);
                                }
                             } else {
                                 DebugPrint("IDM_FILE_COPYOUTPUT: Failed to allocate global memory.\n");
                                 MessageBoxA(hwnd, STRING_CLIPBOARD_MEM_ALLOC_ERROR_ANSI, "Error", MB_OK | MB_ICONERROR);
                             }
                        } else {
                            DebugPrint("IDM_FILE_COPYOUTPUT: Output text length is 0.\n");
                            // Optional: Notify user if there's nothing to copy
                            // MessageBoxA(hwnd, "Output area is empty.", "Copy", MB_OK | MB_ICONINFORMATION);
                        }
                    } else {
                        DebugPrint("IDM_FILE_COPYOUTPUT: Text is selected, allowing default copy behavior.\n");
                        // Text is selected, do nothing here. The default edit control handling
                        // will copy the selected text.
                    }
                    break; // Break for IDM_FILE_COPYOUTPUT case
                } // End brace for IDM_FILE_COPYOUTPUT scope

                 case IDM_FILE_CLEAROUTPUT:
                 {
                     DebugPrint("WM_COMMAND: IDM_FILE_CLEAROUTPUT received. Clearing output text.\n");
                     SetWindowTextA(hwndOutputEdit, ""); // Clear the text in the output edit control
                     DebugPrint("IDM_FILE_CLEAROUTPUT: Text cleared. Forcing repaint.\n");

                     // Attempt to force a clean repaint
                     InvalidateRect(hwndOutputEdit, NULL, TRUE); // Invalidate and erase background
                     UpdateWindow(hwndOutputEdit); // Force immediate paint
                     DebugPrint("IDM_FILE_CLEAROUTPUT: Repaint forced.\n");

                     break;
                 }

                case IDM_FILE_EXIT:
                    DebugPrint("WM_COMMAND: IDM_FILE_EXIT received.\n");
                    DestroyWindow(hwnd);
                    break;

                default:
                    // Let Windows handle any messages we don't process
                    // DebugPrint("WM_COMMAND: Unhandled message.\n"); // Too noisy
                    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
            }
            break; // End of WM_COMMAND (handled cases break internally)
        }

        case WM_CTLCOLORSTATIC:
        {
             HDC hdcStatic = (HDC)wParam;
             // Make label background transparent to match window background
             SetBkMode(hdcStatic, TRANSPARENT);
             // Return a NULL_BRUSH handle to prevent background painting
             return (LRESULT)GetStockObject(NULL_BRUSH);
        }
        // Note: No 'break' needed after return

         case WM_APP_INTERPRETER_OUTPUT_STRING: {
            // Append a string to output edit control (used for error messages and buffered output) (ANSI)
            DebugPrintOutput("WM_APP_INTERPRETER_OUTPUT_STRING received.\n");
            HWND hEdit = hwndOutputEdit;
            LPCSTR szString = (LPCSTR)lParam; // Assuming lParam is a valid string pointer

            if (szString) {
                // Disable redrawing
                SendMessageA(hEdit, WM_SETREDRAW, FALSE, 0);
                DebugPrintOutput("WM_APP_INTERPRETER_OUTPUT_STRING: Redraw disabled.\n");

                // Get current text, concatenate, and set
                int current_len = GetWindowTextLengthA(hEdit);
                int new_chunk_len = strlen(szString);
                int total_len = current_len + new_chunk_len;

                // Allocate buffer for combined text
                char* combined_text = (char*)malloc((total_len + 1) * sizeof(char));
                if (combined_text) {
                    GetWindowTextA(hEdit, combined_text, current_len + 1); // Get current text
                    strcat_s(combined_text, total_len + 1, szString); // Concatenate new chunk

                    SetWindowTextA(hEdit, combined_text); // Set the combined text
                    DebugPrintOutput("WM_APP_INTERPRETER_OUTPUT_STRING: Text replaced with combined output.\n");

                    free(combined_text); // Free the temporary combined buffer
                } else {
                     DebugPrintOutput("WM_APP_INTERPRETER_OUTPUT_STRING: Failed to allocate memory for combined text.\n");
                     // Fallback to appending if memory allocation fails
                     AppendText(hEdit, szString);
                }


                // Auto-scroll to the bottom
                int len = GetWindowTextLengthA(hEdit); // Get new length
                SendMessageA(hEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
                SendMessageA(hEdit, EM_SCROLLCARET, 0, 0); // Scroll caret into view
                DebugPrintOutput("WM_APP_INTERPRETER_OUTPUT_STRING: Scrolled to caret.\n");

                // Re-enable redrawing and force repaint
                SendMessageA(hEdit, WM_SETREDRAW, TRUE, 0);
                DebugPrintOutput("WM_APP_INTERPRETER_OUTPUT_STRING: Redraw re-enabled.\n");
                // Invalidate only the area that needs repainting if possible, or the whole control
                // Using NULL invalidates the entire client area. TRUE means erase background.
                InvalidateRect(hEdit, NULL, TRUE); // Changed to TRUE to force background erase
                UpdateWindow(hEdit); // Force an immediate repaint
                DebugPrintOutput("WM_APP_INTERPRETER_OUTPUT_STRING: Repaint forced.\n");


                // Free the memory allocated for the string in the worker thread
                free((void*)lParam);
                DebugPrintOutput("WM_APP_INTERPRETER_OUTPUT_STRING: Freed string memory.\n");
            }
            return 0;
         }


         case WM_APP_INTERPRETER_DONE: {
             DebugPrint("WM_APP_INTERPRETER_DONE received.\n");
             // Interpreter thread finished
             g_bInterpreterRunning = FALSE; // Use simple assignment for volatile bool
             // wParam indicates status: 0 for success, 1 for error

             if (wParam == 0) {
                 DebugPrint("WM_APP_INTERPRETER_DONE: Status = Success.\n");
                 // Success (output is already updated via WM_APP_INTERPRETER_OUTPUT_STRING)
                 // Optionally, display a "Done" message
                 // MessageBoxA(hwnd, "Interpretation finished successfully.", WINDOW_TITLE_ANSI, MB_OK | MB_ICONINFORMATION);
             } else {
                 DebugPrint("WM_APP_INTERPRETER_DONE: Status = Error.\n");
                 // Error message is already posted via WM_APP_INTERPRETER_OUTPUT_STRING
                 // Optionally, display an "Error" message box
                 // MessageBoxA(hwnd, "Interpretation finished with errors.", WINDOW_TITLE_ANSI, MB_OK | MB_ICONERROR);
             }

             return 0;
         }


        case WM_CLOSE:
            DebugPrint("WM_CLOSE received.\n");
            // Use MessageBoxA for ANSI
            if (MessageBoxA(hwnd, STRING_REALLY_QUIT_ANSI, STRING_CONFIRM_EXIT_ANSI, MB_OKCANCEL | MB_ICONQUESTION) == IDOK) {
                DebugPrint("WM_CLOSE: User confirmed exit.\n");
                DestroyWindow(hwnd);
            } else {
                 DebugPrint("WM_CLOSE: User cancelled exit.\n");
            }
            return 0; // Indicate we handled the message (prevents DefWindowProc from closing)

        case WM_DESTROY:
            DebugPrint("WM_DESTROY received.\n");
             // Signal the interpreter thread to stop if it's running
            g_bInterpreterRunning = FALSE; // Use simple assignment for volatile bool
            // It's generally not recommended to block the UI thread waiting for a worker thread
            // in WM_DESTROY in a real application, as it can make the application
            // appear to hang during shutdown. For simplicity here, we rely on the thread
            // checking g_bInterpreterRunning frequently. A robust solution would involve
            // more sophisticated thread synchronization or a dedicated "Stop" mechanism.

            if (hMonoFont) {
                DebugPrint("WM_DESTROY: Deleting font object.\n");
                DeleteObject(hMonoFont); // Clean up the font object
                hMonoFont = NULL;
            }
            PostQuitMessage(0); // End the message loop
            DebugPrint("WM_DESTROY: Posted WM_QUIT.\n");
            break;

        default:
            // Let Windows handle any messages we don't process (ANSI version)
            // DebugPrint("WindowProc: Unhandled message.\n"); // Too noisy
            return DefWindowProcA(hwnd, uMsg, wParam, lParam);
    }
    return 0; // Default return for handled messages that don't return earlier
}

// --- WinMain Entry Point (ANSI version) ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    DebugPrint("WinMain started.\n");
    // Store instance handle in our global variable
    hInst = hInstance;

    // Define window class name (ANSI string)
    const char CLASS_NAME[] = "BFInterpreterWindowClass";

    // --- Register the window class ---
    WNDCLASSA wc = { 0 }; // Use WNDCLASSA for ANSI

    wc.lpfnWndProc     = WindowProc;
    wc.hInstance       = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor         = LoadCursor(NULL, IDC_ARROW);
    wc.lpszMenuName  = NULL; // We will create menu programmatically

    DebugPrint("WinMain: Registering window class.\n");
    if (!RegisterClassA(&wc)) // Use RegisterClassA for ANSI
    {
        DebugPrint("WinMain: Window registration failed.\n");
        MessageBoxA(NULL, STRING_WINDOW_REG_ERROR_ANSI, "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    DebugPrint("WinMain: Window class registered successfully.\n");

    // --- Load Accelerator Table ---
    // Define the accelerator table structure
    ACCEL AccelTable[] = {
        {FVIRTKEY | FCONTROL, 'R', IDM_FILE_RUN},
        {FVIRTKEY | FCONTROL, 'C', IDM_FILE_COPYOUTPUT},
        {FVIRTKEY | FCONTROL, 'X', IDM_FILE_EXIT}, // Accelerator for Exit
        {FVIRTKEY | FCONTROL, 'O', IDM_FILE_OPEN}  // Accelerator for Open
    };

    // Create the accelerator table from the structure
    hAccelTable = CreateAcceleratorTableA(AccelTable, sizeof(AccelTable) / sizeof(ACCEL));

    if (hAccelTable == NULL) {
        DebugPrint("WinMain: Failed to create accelerator table.\n");
        // You might want to show a message box here in a real app
    } else {
        DebugPrint("WinMain: Accelerator table created.\n");
    }


    // --- Create the window ---
    DebugPrint("WinMain: Creating window.\n");
    HWND hwnd = CreateWindowExA(
        0,                                  // Optional window styles.
        CLASS_NAME,                         // Window class (ANSI)
        WINDOW_TITLE_ANSI,                  // Window title (ANSI)
        WS_OVERLAPPEDWINDOW,                // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 550,

        NULL,       // Parent window
        NULL,       // Menu (we create it in WM_CREATE)
        hInstance,  // Instance handle
        NULL        // Additional application data
    );

    if (hwnd == NULL)
    {
        DebugPrint("WinMain: Window creation failed.\n");
        MessageBoxA(NULL, STRING_WINDOW_CREATION_ERROR_ANSI, "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    DebugPrint("WinMain: Window created successfully.\n");

    // --- Show and update the window ---
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    DebugPrint("WinMain: Window shown and updated.\n");

    // --- Message Loop ---
    DebugPrint("WinMain: Entering message loop.\n");
    MSG msg = { 0 };
    while (GetMessageA(&msg, NULL, 0, 0) > 0) // Use GetMessageA
    {
        // Translate accelerator keys before dispatching the message
        if (!TranslateAcceleratorA(msg.hwnd, hAccelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg); // Use DispatchMessageA
        }
    }
    DebugPrint("WinMain: Exited message loop.\n");

    // --- Clean up Accelerator Table ---
    if (hAccelTable) {
        DestroyAcceleratorTable(hAccelTable);
        DebugPrint("WinMain: Accelerator table destroyed.\n");
    }

    DebugPrint("WinMain finished.\n");
    return (int)msg.wParam;
}

