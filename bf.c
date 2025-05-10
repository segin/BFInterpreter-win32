#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h> // For GET_WM_COMMAND_ID
#include <stdio.h>    // For sprintf_s (optional, mainly for debugging or error messages)
#include <stdlib.h>   // For malloc/free
#include <string.h>   // For memset, strlen, strcpy, etc.
#include <commdlg.h>  // For OpenFileName
#include <stdarg.h>   // For va_list, va_start, va_end
#include <dlgs.h>     // Include this for dialog styles like DS_RESIZE (though using WS_SIZEBOX/WS_THICKFRAME below)

// Define Control IDs
#define IDC_STATIC_CODE     101
#define IDC_EDIT_CODE       102
#define IDC_STATIC_INPUT    103
#define IDC_EDIT_INPUT      104
#define IDC_STATIC_OUTPUT   105
#define IDC_EDIT_OUTPUT     106 // Reverted back to EDIT control ID
#define IDC_BUTTON_NEW_WINDOW 108 // New ID for the "New Window" button

// Define Menu IDs
#define IDM_FILE_RUN        201
#define IDM_FILE_COPYOUTPUT 202
#define IDM_FILE_EXIT       203
#define IDM_FILE_OPEN       204 // New menu ID for Open
#define IDM_FILE_SETTINGS   205 // New menu ID for Settings
#define IDM_FILE_CLEAROUTPUT 206 // New menu ID for Clear Output

// Accelerator IDs for Ctrl+A
#define IDM_SELECTALL_CODE   401
#define IDM_SELECTALL_INPUT  402
#define IDM_SELECTALL_OUTPUT 403


// Define Dialog Control IDs for Settings Dialog
#define IDC_CHECK_DEBUG_INTERPRETER 301
#define IDC_CHECK_DEBUG_OUTPUT      302
#define IDC_CHECK_DEBUG_BASIC       303
// IDOK and IDCANCEL are predefined as 1 and 2

// New Control ID for the dummy checkbox in the new window/dialog
#define IDC_DUMMY_CHECKBOX 501


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
#define STRING_NEW_WINDOW_BUTTON_ANSI "New Window" // Text for the new button
#define STRING_BLANK_WINDOW_TITLE_ANSI "Blank Window" // Title for the new window/dialog
#define NEW_WINDOW_CLASS_NAME_ANSI "BlankWindowClass" // This constant is no longer used for modal dialog


#define TAPE_SIZE           65536 // Equivalent to 0x10000 in Java Tape.java
#define OUTPUT_BUFFER_SIZE  1024  // Size of the output buffer

// Global variables
HINSTANCE hInst;
HFONT hMonoFont = NULL;
HWND hwndCodeEdit = NULL;
HWND hwndInputEdit = NULL;
HWND hwndOutputEdit = NULL; // Reverted back to original name for edit control
HANDLE g_hInterpreterThread = NULL; // Handle for the worker thread
volatile BOOL g_bInterpreterRunning = FALSE; // Flag to indicate if interpreter is running (volatile for thread safety)
HACCEL hAccelTable; // Handle to the accelerator table

// Global debug settings flags (volatile for thread safety)
volatile BOOL g_bDebugInterpreter = TRUE; // Default to TRUE
volatile BOOL g_bDebugOutput = TRUE;      // Default to TRUE
volatile BOOL g_bDebugBasic = TRUE;       // Default to TRUE

// Forward declaration of the blank dialog procedure
INT_PTR CALLBACK BlankDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Forward declaration of the main window procedure
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

// --- Dialog Item Template Structure (Wide Character Version) ---
// This structure defines a control within a dialog box template in memory.
#pragma pack(push, 1) // Ensure byte alignment
typedef struct {
    DWORD  helpID;
    DWORD  exStyle;
    DWORD  style;
    short  x;
    short  y;
    short  cx;
    short  cy;
    DWORD  id;
    // Followed by:
    // - Class (WORD ordinal or null-terminated WCHAR string)
    // - Title (WORD ordinal or null-terminated WCHAR string)
    // - Creation Data (WORD size, followed by data)
} MY_DLGITEMTEMPLATEEX_WIDE;
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
            // Get handles to the dynamically created controls
            // These will be NULL for a blank dialog, which is expected.
            hCheckInterpreter = GetDlgItem(hDlg, IDC_CHECK_DEBUG_INTERPRETER);
            hCheckOutput = GetDlgItem(hDlg, IDC_CHECK_DEBUG_OUTPUT);
            hCheckBasic = GetDlgItem(hDlg, IDC_CHECK_DEBUG_BASIC);
            hBtnOK = GetDlgItem(hDlg, IDOK);
            hBtnCancel = GetDlgItem(hDlg, IDCANCEL);

            // Initialize checkboxes based on current global settings
            // These checks will safely do nothing if the controls don't exist.
            if (hCheckInterpreter) CheckDlgButton(hDlg, IDC_CHECK_DEBUG_INTERPRETER, g_bDebugInterpreter ? BST_CHECKED : BST_UNCHECKED);
            if (hCheckOutput) CheckDlgButton(hDlg, IDC_CHECK_DEBUG_OUTPUT, g_bDebugOutput ? BST_CHECKED : BST_UNCHECKED);
            if (hCheckBasic) CheckDlgButton(hDlg, IDC_CHECK_DEBUG_BASIC, g_bDebugBasic ? BST_CHECKED : BST_UNCHECKED);

            // Apply the rule: if basic is off, all are off
            if (!g_bDebugBasic) {
                g_bDebugInterpreter = FALSE;
                g_bDebugOutput = FALSE;
            }
            // Corrected typo here: g_bInterpreter -> g_bDebugInterpreter
            DebugPrint("SettingsDialogProc: Debug settings saved. Basic: %d, Interpreter: %d, Output: %d\n", g_bDebugBasic, g_bDebugInterpreter, g_bDebugOutput);


            // For a blank dialog, we don't have OK/Cancel buttons to wait for.
            // The dialog will just appear and can be closed via the system menu or Alt+F4.
            // We should NOT call EndDialog here, as the user needs to see the blank dialog.

            return (INT_PTR)TRUE; // Return TRUE to set the keyboard focus
        case WM_COMMAND:
        { // Added braces to create a new scope
            DebugPrint("SettingsDialogProc: WM_COMMAND received. LOWORD(wParam): %u, HIWORD(wParam): %u, lParam: %p\n", LOWORD(wParam), HIWORD(wParam), (void*)lParam);
            switch (LOWORD(wParam)) {
                case IDOK:
                    DebugPrint("SettingsDialogProc: IDOK received.\n");
                    // Save settings from checkboxes (will do nothing for blank dialog)
                    g_bDebugInterpreter = IsDlgButtonChecked(hDlg, IDC_CHECK_DEBUG_INTERPRETER) == BST_CHECKED;
                    g_bDebugOutput = IsDlgButtonChecked(hDlg, IDC_CHECK_DEBUG_OUTPUT) == BST_CHECKED;
                    g_bDebugBasic = IsDlgButtonChecked(hDlg, IDC_CHECK_DEBUG_BASIC) == BST_CHECKED;

                    // Apply the rule: if basic is off, all are off
                    if (!g_bDebugBasic) {
                        g_bDebugInterpreter = FALSE;
                        g_bDebugOutput = FALSE;
                    }
                    DebugPrint("SettingsDialogProc: Debug settings saved. Basic: %d, Interpreter: %d, Output: %d\n", g_bDebugBasic, g_bDebugInterpreter, g_bDebugOutput);

                    EndDialog(hDlg, LOWORD(wParam)); // Close the dialog
                    return (INT_PTR)TRUE;
                case IDCANCEL:
                    DebugPrint("SettingsDialogProc: IDCANCEL received.\n");
                    EndDialog(hDlg, LOWORD(wParam)); // Close the dialog
                    return (INT_PTR)TRUE;
            }
            break;
        } // Added braces to create a new scope
         case WM_CLOSE:
            DebugPrint("SettingsDialogProc: WM_CLOSE received.\n");
            EndDialog(hDlg, IDCANCEL); // Treat closing via system menu as Cancel
            return (INT_PTR)TRUE;
    }
    return (INT_PTR)FALSE; // Let the system handle other messages
}

// --- Blank Dialog Procedure ---
INT_PTR CALLBACK BlankDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
            DebugPrint("BlankDialogProc: WM_INITDIALOG received.\n");
            // No need to create controls here if they are defined in the template
            return (INT_PTR)TRUE; // Return TRUE to set the keyboard focus

        case WM_COMMAND:
        { // Added braces for scope
            DebugPrint("BlankDialogProc: WM_COMMAND received.\n");
            int wmId = LOWORD(wParam);
            switch (wmId) {
                case IDC_DUMMY_CHECKBOX:
                    DebugPrint("BlankDialogProc: Dummy checkbox clicked.\n");
                    // Dummy handler for the checkbox
                    break;
                case IDOK: // Standard OK button
                case IDCANCEL: // Standard Cancel button
                    DebugPrint("BlankDialogProc: IDOK or IDCANCEL received. Ending dialog.\n");
                    EndDialog(hDlg, LOWORD(wParam)); // Close the dialog
                    return (INT_PTR)TRUE;
            }
            break; // End of WM_COMMAND
        }


        case WM_CLOSE:
            DebugPrint("BlankDialogProc: WM_CLOSE received. Ending dialog.\n");
            EndDialog(hDlg, IDCANCEL); // Close the dialog
            return (INT_PTR)TRUE; // Indicate we handled the message

        default:
            // Let Windows handle any messages we don't process
            return (INT_PTR)FALSE; // Return FALSE for messages not handled
    }
    return (INT_PTR)FALSE; // Default return for handled messages that don't return earlier
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

            // Standard Output (Edit control without ES_READONLY, input blocked manually)
            hwndOutputEdit = CreateWindowExA(
                WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL, // Removed ES_READONLY
                10, 325, 560, 150, hwnd, (HMENU)IDC_EDIT_OUTPUT, hInst, NULL); // Reverted to IDC_EDIT_OUTPUT

            // New Window Button (positioned in the top-right)
            // Get client rectangle to position relative to window size
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            int buttonWidth = 100;
            int buttonHeight = 25;
            int buttonMargin = 10;

            CreateWindowA("BUTTON", STRING_NEW_WINDOW_BUTTON_ANSI, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          rcClient.right - buttonWidth - buttonMargin, buttonMargin, buttonWidth, buttonHeight,
                          hwnd, (HMENU)IDC_BUTTON_NEW_WINDOW, hInst, NULL);


            // --- Debug Print: Check the class name of the created output control ---
            char class_name[256];
            GetClassNameA(hwndOutputEdit, class_name, sizeof(class_name));
            DebugPrint("WM_CREATE: Created output control with handle %p and class name '%s'.\n", (void*)hwndOutputEdit, class_name);


            // Set initial text (ANSI)
            SetWindowTextA(hwndCodeEdit, STRING_CODE_TEXT_ANSI);
            SetWindowTextA(hwndInputEdit, STRING_INPUT_TEXT_ANSI);
            SetWindowTextA(hwndOutputEdit, ""); // Set text for the edit control

            // Apply the monospaced font to all EDIT controls
            if (hMonoFont) {
                SendMessageA(hwndCodeEdit, WM_SETFONT, (WPARAM)hMonoFont, MAKELPARAM(TRUE, 0));
                SendMessageA(hwndInputEdit, WM_SETFONT, (WPARAM)hMonoFont, MAKELPARAM(TRUE, 0));
                SendMessageA(hwndOutputEdit, WM_SETFONT, (WPARAM)hMonoFont, MAKELPARAM(TRUE, 0)); // Apply to edit control
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
            int buttonWidth = 100; // Keep button size fixed
            int buttonHeight = 25;
            int buttonMargin = 10;


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
            MoveWindow(hwndOutputEdit, margin, currentY, width - 2 * margin, outputEditHeight, TRUE); // Move edit control

            // Position the "New Window" button in the top-right
            MoveWindow(GetDlgItem(hwnd, IDC_BUTTON_NEW_WINDOW),
                       width - buttonWidth - buttonMargin, buttonMargin,
                       buttonWidth, buttonHeight, TRUE);


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
                    // Filter for Brainfuck files (*.bf and *.b) and all files
                    ofn.lpstrFilter = "Brainfuck Source Code (*.bf, *.b)\0*.bf;*.b\0All Files (*.*)\0*.*\0";
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
                    DebugPrint("WM_COMMAND: IDM_FILE_SETTINGS received. Attempting to show message box.\n");

                    /*
                    // --- Prepare data for the dialog template in memory ---

                    // Dialog Title (Wide Character)
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
                    size_t title_string_size_wide = (title_len_wide + 1) * sizeof(WCHAR);

                    // Control Class Names (Wide Character)
                    // These are allocated but not used in the blank dialog case, will be freed.
                    const char* btn_class_ansi = "BUTTON";
                    int btn_class_len_wide = MultiByteToWideChar(CP_ACP, 0, btn_class_ansi, -1, NULL, 0); // -1 for null terminator
                    WCHAR* btn_class_wide = (WCHAR*)malloc(btn_class_len_wide * sizeof(WCHAR));
                    if (!btn_class_wide) {
                         DebugPrint("IDM_FILE_SETTINGS: Failed to allocate memory for wide button class string.\n");
                         free(settings_dialog_title_wide);
                         break;
                    }
                    MultiByteToWideChar(CP_ACP, 0, btn_class_ansi, -1, btn_class_wide, btn_class_len_wide);
                    // size_t btn_class_size_wide = btn_class_len_wide * sizeof(WCHAR); // Not needed for blank dialog

                    // Control Text (Wide Character)
                    // These are allocated but not used in the blank dialog case, will be freed.
                    const char* debug_interp_text_ansi = STRING_DEBUG_INTERPRETER_ANSI;
                    int debug_interp_text_len_wide = MultiByteToWideChar(CP_ACP, 0, debug_interp_text_ansi, -1, NULL, 0);
                    WCHAR* debug_interp_text_wide = (WCHAR*)malloc(debug_interp_text_len_wide * sizeof(WCHAR));
                     if (!debug_interp_text_wide) {
                         DebugPrint("IDM_FILE_SETTINGS: Failed to allocate memory for wide debug interp text.\n");
                         free(settings_dialog_title_wide);
                         free(btn_class_wide);
                         break;
                    }
                    MultiByteToWideChar(CP_ACP, 0, debug_interp_text_ansi, -1, debug_interp_text_wide, debug_interp_text_len_wide);
                    // size_t debug_interp_text_size_wide = debug_interp_text_len_wide * sizeof(WCHAR); // Not needed

                    const char* debug_output_text_ansi = STRING_DEBUG_OUTPUT_ANSI;
                    int debug_output_text_len_wide = MultiByteToWideChar(CP_ACP, 0, debug_output_text_ansi, -1, NULL, 0);
                    WCHAR* debug_output_text_wide = (WCHAR*)malloc(debug_output_text_len_wide * sizeof(WCHAR));
                     if (!debug_output_text_wide) {
                         DebugPrint("IDM_FILE_SETTINGS: Failed to allocate memory for wide debug output text.\n");
                         free(settings_dialog_title_wide);
                         free(btn_class_wide);
                         free(debug_interp_text_wide);
                         break;
                    }
                    MultiByteToWideChar(CP_ACP, 0, debug_output_text_ansi, -1, debug_output_text_wide, debug_output_text_len_wide);
                    // size_t debug_output_text_size_wide = debug_output_text_len_wide * sizeof(WCHAR); // Not needed

                    const char* debug_basic_text_ansi = STRING_DEBUG_BASIC_ANSI;
                    int debug_basic_text_len_wide = MultiByteToWideChar(CP_ACP, 0, debug_basic_text_ansi, -1, NULL, 0);
                    WCHAR* debug_basic_text_wide = (WCHAR*)malloc(debug_basic_text_len_wide * sizeof(WCHAR));
                     if (!debug_basic_text_wide) {
                         DebugPrint("IDM_FILE_SETTINGS: Failed to allocate memory for wide debug basic text.\n");
                         free(settings_dialog_title_wide);
                         free(btn_class_wide);
                         free(debug_interp_text_wide);
                         free(debug_output_text_wide);
                         break;
                    }
                    MultiByteToWideChar(CP_ACP, 0, debug_basic_text_ansi, -1, debug_basic_text_wide, debug_basic_text_len_wide);
                    // size_t debug_basic_text_size_wide = debug_basic_text_len_wide * sizeof(WCHAR); // Not needed

                    const char* ok_text_ansi = STRING_OK_ANSI;
                    int ok_text_len_wide = MultiByteToWideChar(CP_ACP, 0, ok_text_ansi, -1, NULL, 0);
                    WCHAR* ok_text_wide = (WCHAR*)malloc(ok_text_len_wide * sizeof(WCHAR));
                     if (!ok_text_wide) {
                         DebugPrint("IDM_FILE_SETTINGS: Failed to allocate memory for wide OK text.\n");
                         free(settings_dialog_title_wide);
                         free(btn_class_wide);
                         free(debug_interp_text_wide);
                         free(debug_output_text_wide);
                         free(debug_basic_text_wide);
                         break;
                    }
                    MultiByteToWideChar(CP_ACP, 0, ok_text_ansi, -1, ok_text_wide, ok_text_len_wide);
                    // size_t ok_text_size_wide = ok_text_len_wide * sizeof(WCHAR); // Not needed

                    const char* cancel_text_ansi = STRING_CANCEL_ANSI;
                    int cancel_text_len_wide = MultiByteToWideChar(CP_ACP, 0, cancel_text_ansi, -1, NULL, 0);
                    WCHAR* cancel_text_wide = (WCHAR*)malloc(cancel_text_len_wide * sizeof(WCHAR));
                     if (!cancel_text_wide) {
                         DebugPrint("IDM_FILE_SETTINGS: Failed to allocate memory for wide Cancel text.\n");
                         free(settings_dialog_title_wide);
                         free(btn_class_wide);
                         free(debug_interp_text_wide);
                         free(debug_output_text_wide);
                         free(debug_basic_text_wide);
                         free(ok_text_wide);
                         break;
                    }
                    MultiByteToWideChar(CP_ACP, 0, cancel_text_ansi, -1, cancel_text_wide, cancel_text_len_wide);
                    // size_t cancel_text_size_wide = cancel_text_len_wide * sizeof(WCHAR); // Not needed


                    // --- Calculate total required memory size for a blank dialog ---

                    // Size of the base DLGTEMPLATEEX_WIDE structure
                    size_t template_base_size = sizeof(MY_DLGTEMPLATEEX_WIDE);

                    // Size of the menu name (WORD ordinal 0xFFFF followed by WORD 0 for no menu)
                    size_t menu_size = sizeof(WORD) + sizeof(WORD); // 0xFFFF, 0

                    // Size of the class name (WORD ordinal 0xFFFF followed by WORD 0 for default dialog class)
                    size_t class_size = sizeof(WORD) + sizeof(WORD); // 0xFFFF, 0

                    // Total size = base structure size + menu + class + title
                    size_t total_template_size = template_base_size;
                    total_template_size = (total_template_size + sizeof(WORD) - 1) & ~(sizeof(WORD) - 1); // Align for menu
                    total_template_size += menu_size;
                    total_template_size = (total_template_size + sizeof(WORD) - 1) & ~(sizeof(WORD) - 1); // Align for class
                    total_template_size += class_size;
                    total_template_size = (total_template_size + sizeof(WORD) - 1) & ~(sizeof(WORD) - 1); // Align for title
                    total_template_size += title_string_size_wide;

                    // Ensure the final size is DWORD aligned (although for just the header, WORD is sufficient)
                    total_template_size = (total_template_size + sizeof(DWORD) - 1) & ~(sizeof(DWORD) - 1);


                    DebugPrint("IDM_FILE_SETTINGS: Calculated total template size for blank dialog: %zu\n", total_template_size);


                    // Allocate memory for the combined template and data
                    HGLOBAL hGlobalTemplate = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, total_template_size);
                    DebugPrint("IDM_FILE_SETTINGS: GlobalAlloc returned %p\n", hGlobalTemplate);

                    if (hGlobalTemplate) {
                        LPBYTE pGlobalTemplate = (LPBYTE)GlobalLock(hGlobalTemplate);
                        DebugPrint("IDM_FILE_SETTINGS: GlobalLock returned %p\n", pGlobalTemplate);

                        if (pGlobalTemplate) {
                            LPBYTE pCurrent = pGlobalTemplate;

                            // Copy the fixed template structure
                            MY_DLGTEMPLATEEX_WIDE template_fixed_part = {
                                0x0001, // dlgVer
                                0xFFFF, // signature (indicates DLGTEMPLATEEX)
                                0,      // helpID
                                0,      // exStyle
                                WS_POPUP | WS_BORDER | WS_SYSMENU | DS_MODALFRAME | WS_CAPTION, // style (removed DS_SETFONT)
                                0,      // cDlgItems (Number of controls - set to 0 for blank dialog)
                                100,    // x (arbitrary position)
                                100,    // y (arbitrary position)
                                350,    // cx (width)
                                150     // cy (height)
                            };
                            memcpy(pCurrent, &template_fixed_part, sizeof(MY_DLGTEMPLATEEX_WIDE));
                            pCurrent += sizeof(MY_DLGTEMPLATEEX_WIDE);
                            DebugPrint("IDM_FILE_SETTINGS: Copied fixed template structure. Current offset: %zu\n", pCurrent - pGlobalTemplate);

                            // Align for menu (WORD alignment)
                            pCurrent = (LPBYTE)(((ULONG_PTR)pCurrent + sizeof(WORD) - 1) & ~(sizeof(WORD) - 1));
                            DebugPrint("IDM_FILE_SETTINGS: Aligned for menu. Current offset: %zu\n", pCurrent - pGlobalTemplate);
                            // Copy menu name (ordinal for no menu)
                            LPWORD pMenu = (LPWORD)pCurrent;
                            *pMenu++ = 0xFFFF; // Indicates ordinal
                            *pMenu = 0;      // Ordinal 0 for no menu
                            pCurrent += sizeof(WORD) * 2; // Size of ordinal + 0
                            DebugPrint("IDM_FILE_SETTINGS: Copied menu ordinal. Current offset: %zu\n", pCurrent - pGlobalTemplate);


                            // Align for class (WORD alignment)
                            pCurrent = (LPBYTE)(((ULONG_PTR)pCurrent + sizeof(WORD) - 1) & ~(sizeof(WORD) - 1));
                            DebugPrint("IDM_FILE_SETTINGS: Aligned for class. Current offset: %zu\n", pCurrent - pGlobalTemplate);
                            // Copy class name (ordinal for default dialog class)
                             LPWORD pClass = (LPWORD)pCurrent;
                            *pClass++ = 0xFFFF; // Indicates ordinal
                            *pClass = 0;      // Ordinal 0 for default dialog class
                            pCurrent += sizeof(WORD) * 2; // Size of ordinal + 0
                            DebugPrint("IDM_FILE_SETTINGS: Copied class ordinal. Current offset: %zu\n", pCurrent - pGlobalTemplate);


                            // Align for title (WORD alignment)
                            pCurrent = (LPBYTE)(((ULONG_PTR)pCurrent + sizeof(WORD) - 1) & ~(sizeof(WORD) - 1));
                            DebugPrint("IDM_FILE_SETTINGS: Aligned for title. Current offset: %zu\n", pCurrent - pGlobalTemplate);
                            // Copy the wide title string
                            LPWSTR pTitle = (LPWSTR)pCurrent;
                            memcpy(pTitle, settings_dialog_title_wide, title_string_size_wide);
                            pCurrent += title_string_size_wide;
                            DebugPrint("IDM_FILE_SETTINGS: Copied wide title string. Current offset: %zu\n", pCurrent - pGlobalTemplate);


                            // --- No control items added here for a blank dialog ---
                            // Commented out the ADD_CONTROL_ITEM macro calls:
                            // #define ADD_CONTROL_ITEM(...) ...
                            // ADD_CONTROL_ITEM(IDC_CHECK_DEBUG_INTERPRETER, btn_class_wide, debug_interp_text_wide, 10, 10, 300, 20, BS_AUTOCHECKBOX, 0);
                            // ADD_CONTROL_ITEM(IDC_CHECK_DEBUG_OUTPUT, btn_class_wide, debug_output_text_wide, 10, 35, 300, 20, BS_AUTOCHECKBOX, 0);
                            // ADD_CONTROL_ITEM(IDC_CHECK_DEBUG_BASIC, btn_class_wide, debug_basic_text_wide, 10, 60, 300, 20, BS_AUTOCHECKBOX, 0);
                            // ADD_CONTROL_ITEM(IDOK, btn_class_wide, ok_text_wide, 80, 100, 75, 25, BS_DEFPUSHBUTTON, 0);
                            // ADD_CONTROL_ITEM(IDCANCEL, btn_class_wide, cancel_text_wide, 160, 100, 75, 25, BS_PUSHBUTTON, 0);


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

                    // Free the allocated wide strings (moved inside the case)
                    // free(settings_dialog_title_wide); // This variable was not allocated in this version
                    // These were allocated but not used in the blank dialog case, still good to free.
                    // free(btn_class_wide); // This variable is not used in the blank dialog case
                    // free(debug_interp_text_wide); // Not used
                    // free(debug_output_text_wide); // Not used
                    // free(debug_basic_text_wide); // Not used
                    // free(ok_text_wide); // Not used
                    // free(cancel_text_wide); // Not used
                    DebugPrint("IDM_FILE_SETTINGS: Freed wide string memory.\n");
                    */

                    // --- Replace with a simple MessageBoxA call ---
                    MessageBoxA(hwnd, "Settings menu item clicked!", "Settings Test", MB_OK | MB_ICONINFORMATION);
                    DebugPrint("WM_COMMAND: Settings dialog process finished.\n");
                    break;
                }

                case IDM_FILE_RUN:
                { // Added braces to scope variables
                    DebugPrint("WM_COMMAND: IDM_FILE_RUN received.\n");
                    if (!g_bInterpreterRunning) {
                        DebugPrint("IDM_FILE_RUN: Interpreter not running, attempting to start.\n");
                        // Clear previous output by setting edit control text to empty
                        DebugPrint("IDM_FILE_RUN: Clearing output edit (handle %p).\n", (void*)hwndOutputEdit);
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
                    // For an edit control, we can get the entire text
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
                    break; // Break for IDM_FILE_COPYOUTPUT case
                } // End brace for IDM_FILE_COPYOUTPUT scope

                 case IDM_FILE_CLEAROUTPUT:
                 {
                     DebugPrint("WM_COMMAND: IDM_FILE_CLEAROUTPUT received. Clearing output text.\n");
                     DebugPrint("IDM_FILE_CLEAROUTPUT: Clearing output edit (handle %p).\n", (void*)hwndOutputEdit);

                     // Temporarily disable redrawing
                     SendMessageA(hwndOutputEdit, WM_SETREDRAW, FALSE, 0);
                     DebugPrint("IDM_FILE_CLEAROUTPUT: Disabled redrawing.\n");

                     // Clear the text
                     SetWindowTextA(hwndOutputEdit, "");
                     DebugPrint("IDM_FILE_CLEAROUTPUT: Text cleared.\n");

                     // Re-enable redrawing and force a repaint
                     SendMessageA(hwndOutputEdit, WM_SETREDRAW, TRUE, 0);
                     DebugPrint("IDM_FILE_CLEAROUTPUT: Enabled redrawing.\n");

                     RECT rcClient;
                     GetClientRect(hwndOutputEdit, &rcClient);
                     InvalidateRect(hwndOutputEdit, &rcClient, TRUE); // Invalidate and erase background
                     UpdateWindow(hwndOutputEdit); // Force immediate paint

                     DebugPrint("IDM_FILE_CLEAROUTPUT: Repaint forced.\n");

                     break;
                 }

                case IDM_FILE_EXIT:
                    DebugPrint("WM_COMMAND: IDM_FILE_EXIT received.\n");
                    DestroyWindow(hwnd);
                    break;

                case IDM_SELECTALL_CODE:
                    DebugPrint("WM_COMMAND: IDM_SELECTALL_CODE received. Selecting all text in code edit.\n");
                    // Select all text in the code edit control
                    SendMessageA(hwndCodeEdit, EM_SETSEL, 0, -1);
                    break;

                case IDM_SELECTALL_INPUT:
                     DebugPrint("WM_COMMAND: IDM_SELECTALL_INPUT received. Selecting all text in input edit.\n");
                    // Select all text in the input edit control
                    SendMessageA(hwndInputEdit, EM_SETSEL, 0, -1);
                    break;

                case IDM_SELECTALL_OUTPUT:
                     DebugPrint("WM_COMMAND: IDM_SELECTALL_OUTPUT received. Selecting all text in output edit.\n");
                    // Select all text in the output edit control
                    SendMessageA(hwndOutputEdit, EM_SETSEL, 0, -1);
                    break;

                case IDC_BUTTON_NEW_WINDOW:
                {
                    DebugPrint("WM_COMMAND: IDC_BUTTON_NEW_WINDOW received. Creating new blank modal dialog.\n");

                    // --- Prepare data for the dialog template in memory ---

                    // Dialog Title (Wide Character)
                    int title_len_ansi = strlen(STRING_BLANK_WINDOW_TITLE_ANSI);
                    int title_len_wide = MultiByteToWideChar(CP_ACP, 0, STRING_BLANK_WINDOW_TITLE_ANSI, title_len_ansi, NULL, 0);
                    WCHAR* blank_dialog_title_wide = (WCHAR*)malloc((title_len_wide + 1) * sizeof(WCHAR));
                    if (!blank_dialog_title_wide) {
                        DebugPrint("IDC_BUTTON_NEW_WINDOW: Failed to allocate memory for wide title string.\n");
                        MessageBoxA(hwnd, "Memory allocation failed for dialog title.", "Error", MB_OK | MB_ICONERROR);
                        break;
                    }
                    MultiByteToWideChar(CP_ACP, 0, STRING_BLANK_WINDOW_TITLE_ANSI, title_len_ansi, blank_dialog_title_wide, title_len_wide);
                    blank_dialog_title_wide[title_len_wide] = L'\0'; // Null-terminate wide string
                    size_t title_string_size_wide = (title_len_wide + 1) * sizeof(WCHAR);

                    // Control Class Names (Wide Character)
                    const char* btn_class_ansi = "BUTTON";
                    int btn_class_len_wide = MultiByteToWideChar(CP_ACP, 0, btn_class_ansi, -1, NULL, 0); // -1 for null terminator
                    WCHAR* btn_class_wide = (WCHAR*)malloc(btn_class_len_wide * sizeof(WCHAR));
                    if (!btn_class_wide) {
                         DebugPrint("IDC_BUTTON_NEW_WINDOW: Failed to allocate memory for wide button class string.\n");
                         free(blank_dialog_title_wide);
                         MessageBoxA(hwnd, "Memory allocation failed for button class.", "Error", MB_OK | MB_ICONERROR);
                         break;
                    }
                    MultiByteToWideChar(CP_ACP, 0, btn_class_ansi, -1, btn_class_wide, btn_class_len_wide);
                    size_t btn_class_size_wide = btn_class_len_wide * sizeof(WCHAR);

                    // Control Text (Wide Character)
                    const char* dummy_checkbox_text_ansi = STRING_DUMMY_CHECKBOX_ANSI;
                    int dummy_checkbox_text_len_wide = MultiByteToWideChar(CP_ACP, 0, dummy_checkbox_text_ansi, -1, NULL, 0);
                    WCHAR* dummy_checkbox_text_wide = (WCHAR*)malloc(dummy_checkbox_text_len_wide * sizeof(WCHAR));
                     if (!dummy_checkbox_text_wide) {
                         DebugPrint("IDC_BUTTON_NEW_WINDOW: Failed to allocate memory for wide dummy checkbox text.\n");
                         free(blank_dialog_title_wide);
                         free(btn_class_wide);
                         MessageBoxA(hwnd, "Memory allocation failed for checkbox text.", "Error", MB_OK | MB_ICONERROR);
                         break;
                    }
                    MultiByteToWideChar(CP_ACP, 0, dummy_checkbox_text_ansi, -1, dummy_checkbox_text_wide, dummy_checkbox_text_len_wide);
                    size_t dummy_checkbox_text_size_wide = dummy_checkbox_text_len_wide * sizeof(WCHAR);


                    // --- Calculate total required memory size for the dialog template ---

                    // Size of the base DLGTEMPLATEEX_WIDE structure
                    size_t template_base_size = sizeof(MY_DLGTEMPLATEEX_WIDE);

                    // Size of the menu name (WORD ordinal 0xFFFF followed by WORD 0 for no menu)
                    size_t menu_size = sizeof(WORD) + sizeof(WORD); // 0xFFFF, 0

                    // Size of the class name (WORD ordinal 0xFFFF followed by WORD 0 for default dialog class)
                    size_t class_size = sizeof(WORD) + sizeof(WORD); // 0xFFFF, 0

                    // Size of the title string
                    size_t title_size = title_string_size_wide;

                    // Size of the dialog item (checkbox)
                    size_t item_base_size = sizeof(MY_DLGITEMTEMPLATEEX_WIDE);
                    // Item class name size (BUTTON)
                    size_t item_class_size = btn_class_size_wide;
                    // Item title size (Dummy Checkbox text)
                    size_t item_title_size = dummy_checkbox_text_size_wide;
                    // Creation data size (WORD 0 for no creation data)
                    size_t item_creation_data_size = sizeof(WORD);

                    // Total size for one item = base size + class + title + creation data
                    // Need to account for alignment before each string/data block
                    size_t total_item_size = item_base_size;
                    total_item_size = (total_item_size + sizeof(WORD) - 1) & ~(sizeof(WORD) - 1); // Align for class
                    total_item_size += item_class_size;
                    total_item_size = (total_item_size + sizeof(WORD) - 1) & ~(sizeof(WORD) - 1); // Align for title
                    total_item_size += item_title_size;
                    total_item_size = (total_item_size + sizeof(WORD) - 1) & ~(sizeof(WORD) - 1); // Align for creation data
                    total_item_size += item_creation_data_size;
                    // Ensure the final item size is DWORD aligned
                    total_item_size = (total_item_size + sizeof(DWORD) - 1) & ~(sizeof(DWORD) - 1);


                    // Total template size = base size + menu + class + title + item(s)
                    size_t total_template_size = template_base_size;
                    total_template_size = (total_template_size + sizeof(WORD) - 1) & ~(sizeof(WORD) - 1); // Align for menu
                    total_template_size += menu_size;
                    total_template_size = (total_template_size + sizeof(WORD) - 1) & ~(sizeof(WORD) - 1); // Align for class
                    total_template_size += class_size;
                    total_template_size = (total_template_size + sizeof(WORD) - 1) & ~(sizeof(WORD) - 1); // Align for title
                    total_template_size += title_size;
                    // Align for the first dialog item (DWORD alignment)
                    total_template_size = (total_template_size + sizeof(DWORD) - 1) & ~(sizeof(DWORD) - 1);
                    total_template_size += total_item_size; // Add size for the dummy checkbox item


                    DebugPrint("IDC_BUTTON_NEW_WINDOW: Calculated total template size for modal dialog: %zu\n", total_template_size);


                    // Allocate memory for the combined template and data
                    HGLOBAL hGlobalTemplate = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, total_template_size);
                    DebugPrint("IDC_BUTTON_NEW_WINDOW: GlobalAlloc returned %p\n", hGlobalTemplate);

                    if (hGlobalTemplate) {
                        LPBYTE pGlobalTemplate = (LPBYTE)GlobalLock(hGlobalTemplate);
                        DebugPrint("IDC_BUTTON_NEW_WINDOW: GlobalLock returned %p\n", pGlobalTemplate);

                        if (pGlobalTemplate) {
                            LPBYTE pCurrent = pGlobalTemplate;

                            // Copy the fixed template structure
                            MY_DLGTEMPLATEEX_WIDE template_fixed_part = {
                                0x0001, // dlgVer
                                0xFFFF, // signature (indicates DLGTEMPLATEEX)
                                0,      // helpID
                                0,      // exStyle
                                WS_POPUP | WS_BORDER | WS_SYSMENU | DS_MODALFRAME | WS_CAPTION | DS_SETFONT, // style (Added DS_SETFONT)
                                1,      // cDlgItems (Number of controls - 1 for the checkbox)
                                100,    // x (arbitrary position)
                                100,    // y (arbitrary position)
                                250,    // cx (width)
                                100     // cy (height)
                            };
                            memcpy(pCurrent, &template_fixed_part, sizeof(MY_DLGTEMPLATEEX_WIDE));
                            pCurrent += sizeof(MY_DLGTEMPLATEEX_WIDE);
                            DebugPrint("IDC_BUTTON_NEW_WINDOW: Copied fixed template structure. Current offset: %zu\n", pCurrent - pGlobalTemplate);

                            // Align for menu (WORD alignment)
                            pCurrent = (LPBYTE)(((ULONG_PTR)pCurrent + sizeof(WORD) - 1) & ~(sizeof(WORD) - 1));
                            DebugPrint("IDC_BUTTON_NEW_WINDOW: Aligned for menu. Current offset: %zu\n", pCurrent - pGlobalTemplate);
                            // Copy menu name (ordinal for no menu)
                            LPWORD pMenu = (LPWORD)pCurrent;
                            *pMenu++ = 0xFFFF; // Indicates ordinal
                            *pMenu = 0;      // Ordinal 0 for no menu
                            pCurrent += sizeof(WORD) * 2; // Size of ordinal + 0
                            DebugPrint("IDC_BUTTON_NEW_WINDOW: Copied menu ordinal. Current offset: %zu\n", pCurrent - pGlobalTemplate);


                            // Align for class (WORD alignment)
                            pCurrent = (LPBYTE)(((ULONG_PTR)pCurrent + sizeof(WORD) - 1) & ~(sizeof(WORD) - 1));
                            DebugPrint("IDC_BUTTON_NEW_WINDOW: Aligned for class. Current offset: %zu\n", pCurrent - pGlobalTemplate);
                            // Copy class name (ordinal for default dialog class)
                             LPWORD pClass = (LPWORD)pCurrent;
                            *pClass++ = 0xFFFF; // Indicates ordinal
                            *pClass = 0;      // Ordinal 0 for default dialog class
                            pCurrent += sizeof(WORD) * 2; // Size of ordinal + 0
                            DebugPrint("IDC_BUTTON_NEW_WINDOW: Copied class ordinal. Current offset: %zu\n", pCurrent - pGlobalTemplate);


                            // Align for title (WORD alignment)
                            pCurrent = (LPBYTE)(((ULONG_PTR)pCurrent + sizeof(WORD) - 1) & ~(sizeof(WORD) - 1));
                            DebugPrint("IDC_BUTTON_NEW_WINDOW: Aligned for title. Current offset: %zu\n", pCurrent - pGlobalTemplate);
                            // Copy the wide title string
                            LPWSTR pTitle = (LPWSTR)pCurrent;
                            memcpy(pTitle, blank_dialog_title_wide, title_string_size_wide);
                            pCurrent += title_string_size_wide;
                            DebugPrint("IDC_BUTTON_NEW_WINDOW: Copied wide title string. Current offset: %zu\n", pCurrent - pGlobalTemplate);

                            // Align for font (WORD alignment)
                            pCurrent = (LPBYTE)(((ULONG_PTR)pCurrent + sizeof(WORD) - 1) & ~(sizeof(WORD) - 1));
                            DebugPrint("IDC_BUTTON_NEW_WINDOW: Aligned for font. Current offset: %zu\n", pCurrent - pGlobalTemplate);
                            // Copy font information (size, weight, italic, typeface)
                            LPWORD pFont = (LPWORD)pCurrent;
                            *pFont++ = 8; // Point size (adjust as needed)
                            *pFont++ = FW_NORMAL; // Weight
                            *pFont++ = FALSE; // Italic
                            pCurrent += sizeof(WORD) * 3;
                            // Copy font typeface (Wide Character) - using a common dialog font
                            const char* dialog_font_ansi = "MS Shell Dlg"; // Common dialog font
                            int dialog_font_len_wide = MultiByteToWideChar(CP_ACP, 0, dialog_font_ansi, -1, NULL, 0);
                            WCHAR* dialog_font_wide = (WCHAR*)malloc(dialog_font_len_wide * sizeof(WCHAR));
                            if (dialog_font_wide) {
                                MultiByteToWideChar(CP_ACP, 0, dialog_font_ansi, -1, dialog_font_wide, dialog_font_len_wide);
                                memcpy(pCurrent, dialog_font_wide, dialog_font_len_wide * sizeof(WCHAR));
                                pCurrent += dialog_font_len_wide * sizeof(WCHAR);
                                free(dialog_font_wide); // Free the temporary wide font string
                            } else {
                                DebugPrint("IDC_BUTTON_NEW_WINDOW: Failed to allocate memory for wide dialog font string.\n");
                                // Handle error or use a default (which memcpy with NULL would do, but might crash)
                                // For robustness, you might want to handle this more gracefully.
                            }
                            DebugPrint("IDC_BUTTON_NEW_WINDOW: Copied font info. Current offset: %zu\n", pCurrent - pGlobalTemplate);


                            // Align for the first dialog item (DWORD alignment)
                            pCurrent = (LPBYTE)(((ULONG_PTR)pCurrent + sizeof(DWORD) - 1) & ~(sizeof(DWORD) - 1));
                            DebugPrint("IDC_BUTTON_NEW_WINDOW: Aligned for first item. Current offset: %zu\n", pCurrent - pGlobalTemplate);


                            // --- Add Dialog Item (Dummy Checkbox) ---
                            MY_DLGITEMTEMPLATEEX_WIDE item_checkbox = {
                                0,      // helpID
                                0,      // exStyle
                                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP, // style
                                10,     // x
                                10,     // y
                                200,    // cx (width)
                                20,     // cy (height)
                                IDC_DUMMY_CHECKBOX // id
                            };
                            memcpy(pCurrent, &item_checkbox, sizeof(MY_DLGITEMTEMPLATEEX_WIDE));
                            pCurrent += sizeof(MY_DLGITEMTEMPLATEEX_WIDE);
                             DebugPrint("IDC_BUTTON_NEW_WINDOW: Copied item structure. Current offset: %zu\n", pCurrent - pGlobalTemplate);


                            // Align for item class (WORD alignment)
                            pCurrent = (LPBYTE)(((ULONG_PTR)pCurrent + sizeof(WORD) - 1) & ~(sizeof(WORD) - 1));
                            DebugPrint("IDC_BUTTON_NEW_WINDOW: Aligned for item class. Current offset: %zu\n", pCurrent - pGlobalTemplate);
                            // Copy item class name (BUTTON)
                            LPWSTR pItemClass = (LPWSTR)pCurrent;
                            memcpy(pItemClass, btn_class_wide, btn_class_size_wide);
                            pCurrent += btn_class_size_wide;
                             DebugPrint("IDC_BUTTON_NEW_WINDOW: Copied item class. Current offset: %zu\n", pCurrent - pGlobalTemplate);


                            // Align for item title (WORD alignment)
                            pCurrent = (LPBYTE)(((ULONG_PTR)pCurrent + sizeof(WORD) - 1) & ~(sizeof(WORD) - 1));
                            DebugPrint("IDC_BUTTON_NEW_WINDOW: Aligned for item title. Current offset: %zu\n", pCurrent - pGlobalTemplate);
                            // Copy item title (Dummy Checkbox text)
                            LPWSTR pItemTitle = (LPWSTR)pCurrent;
                            memcpy(pItemTitle, dummy_checkbox_text_wide, dummy_checkbox_text_size_wide);
                            pCurrent += dummy_checkbox_text_size_wide;
                             DebugPrint("IDC_BUTTON_NEW_WINDOW: Copied item title. Current offset: %zu\n", pCurrent - pGlobalTemplate);


                            // Align for item creation data (WORD alignment)
                            pCurrent = (LPBYTE)(((ULONG_PTR)pCurrent + sizeof(WORD) - 1) & ~(sizeof(WORD) - 1));
                            DebugPrint("IDC_BUTTON_NEW_WINDOW: Aligned for item creation data. Current offset: %zu\n", pCurrent - pGlobalTemplate);
                            // Copy item creation data (WORD 0)
                            LPWORD pCreationDataSize = (LPWORD)pCurrent;
                            *pCreationDataSize = 0; // Size of creation data (0 bytes)
                            pCurrent += sizeof(WORD);
                             DebugPrint("IDC_BUTTON_NEW_WINDOW: Copied item creation data size. Current offset: %zu\n", pCurrent - pGlobalTemplate);


                            GlobalUnlock(hGlobalTemplate);
                            DebugPrint("IDC_BUTTON_NEW_WINDOW: GlobalUnlock called.\n");


                            // Call DialogBoxIndirectW with the memory handle
                            DebugPrint("IDC_BUTTON_NEW_WINDOW: Calling DialogBoxIndirectW...\n");
                            // Use BlankDialogProc for the new modal dialog
                            INT_PTR dialog_result = DialogBoxIndirectW(hInst, (LPCDLGTEMPLATE)hGlobalTemplate, hwnd, BlankDialogProc);
                            DebugPrint("IDC_BUTTON_NEW_WINDOW: DialogBoxIndirectW returned %Id.\n", dialog_result);


                            if (dialog_result == -1) {
                                DebugPrint("IDC_BUTTON_NEW_WINDOW: GetLastError after DialogBoxIndirectW: %lu\n", GetLastError());
                                MessageBoxA(hwnd, "Failed to show modal dialog!", "Error", MB_ICONEXCLAMATION | MB_OK);
                            }

                            DebugPrint("IDC_BUTTON_NEW_WINDOW: Freeing global memory.\n");
                            GlobalFree(hGlobalTemplate); // Free the allocated memory

                        } else {
                            DebugPrint("IDC_BUTTON_NEW_WINDOW: GlobalLock failed for dialog template. GetLastError: %lu\n", GetLastError());
                             MessageBoxA(hwnd, "Failed to lock memory for dialog template.", "Error", MB_OK | MB_ICONERROR);
                        }
                    } else {
                        DebugPrint("IDC_BUTTON_NEW_WINDOW: GlobalAlloc failed for dialog template. GetLastError: %lu\n", GetLastError());
                        MessageBoxA(hwnd, "Failed to allocate memory for dialog template.", "Error", MB_OK | MB_ICONERROR);
                    }

                    // Free the allocated wide strings
                    free(blank_dialog_title_wide);
                    free(btn_class_wide);
                    free(dummy_checkbox_text_wide);
                    DebugPrint("IDC_BUTTON_NEW_WINDOW: Freed wide string memory.\n");

                    break; // Break for IDC_BUTTON_NEW_WINDOW case
                }


                default:
                    // Let Windows handle any messages we don't process (ANSI version)
                    // DebugPrint("WindowProc: Unhandled message.\n"); // Too noisy
                    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
            }
            break; // End of WM_COMMAND (handled cases break internally)
        }

        // --- Input Blocking for Output Edit Control ---
        // Intercept messages that could modify the text in the output edit control
        case WM_KEYDOWN:
        case WM_CHAR:
        case WM_UNICHAR: // For Unicode characters (though we are using ANSI)
        case WM_INPUT:   // Raw input messages
        case WM_IME_CHAR: // Input Method Editor messages
        {
            // Check if the message is for the output edit control
            if ((HWND)lParam == hwndOutputEdit) {
                // If it is, block it by returning 0
                DebugPrint("WindowProc: Blocking input message %u for output edit control.\n", uMsg);
                return 0; // Indicate that we handled the message and it should not be processed further
            }
            // Otherwise, let the default window procedure handle it
            return DefWindowProcA(hwnd, uMsg, wParam, lParam);
        }

        // Intercept mouse clicks that could lead to text modification
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        {
             // Check if the message is for the output edit control
             if ((HWND)lParam == hwndOutputEdit) {
                // If it is, allow it. This is needed for selection to work.
                DebugPrint("WindowProc: Allowing mouse down message %u for output edit control (for selection).\n");
                return DefWindowProcA(hwnd, uMsg, wParam, lParam);
            }
             // Otherwise, let the default window procedure handle it
            return DefWindowProcA(hwnd, uMsg, wParam, lParam);
        }

        // Allow mouse move for selection, but block if it's not part of a drag selection
        case WM_MOUSEMOVE:
        {
             // Check if the message is for the output edit control
             if ((HWND)lParam == hwndOutputEdit) {
                 // Check if a mouse button is down (indicating a drag selection)
                 if (wParam & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON)) {
                     // If a button is down, it's likely a selection drag, so allow the message
                     DebugPrint("WindowProc: Allowing mouse move message for output edit control (during selection).\n");
                     return DefWindowProcA(hwnd, uMsg, wParam, lParam);
                 } else {
                     // If no button is down, it's just a mouse hover, which we can ignore
                     // to prevent potential issues, although it's usually harmless.
                     // Returning 0 here might prevent the caret from changing shape on hover,
                     // which is acceptable for a read-only like behavior.
                     DebugPrint("WindowProc: Blocking mouse move message for output edit control (not during selection).\n");
                     return 0; // Block the message
                 }
            }
             // Otherwise, let the default window procedure handle it
            return DefWindowProcA(hwnd, uMsg, wParam, lParam);
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
            HWND hEdit = hwndOutputEdit; // Use the handle for the edit control
            LPCSTR szString = (LPCSTR)lParam; // Assuming lParam is a valid string pointer

            if (szString) {
                DebugPrintOutput("WM_APP_INTERPRETER_OUTPUT_STRING: Appending to edit control with handle %p.\n", (void*)hEdit);

                // Get current text length
                int current_len = GetWindowTextLengthA(hEdit);

                // Set selection to the end to append
                SendMessageA(hEdit, EM_SETSEL, (WPARAM)current_len, (LPARAM)current_len);

                // Replace the empty selection at the end with the new string
                SendMessageA(hEdit, EM_REPLACESEL, 0, (LPARAM)szString);
                DebugPrintOutput("WM_APP_INTERPRETER_OUTPUT_STRING: Appended text.\n");

                // Auto-scroll to the bottom
                SendMessageA(hEdit, EM_SCROLLCARET, 0, 0);
                DebugPrintOutput("WM_APP_INTERPRETER_OUTPUT_STRING: Scrolled to caret.\n");

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
    const char MAIN_WINDOW_CLASS_NAME[] = "BFInterpreterWindowClass";

    // --- Register the main window class ---
    WNDCLASSA wc = { 0 }; // Use WNDCLASSA for ANSI

    wc.lpfnWndProc     = WindowProc;
    wc.hInstance       = hInstance;
    wc.lpszClassName = MAIN_WINDOW_CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor         = LoadCursor(NULL, IDC_ARROW);
    wc.lpszMenuName  = NULL; // We will create menu programmatically

    DebugPrint("WinMain: Registering main window class.\n");
    if (!RegisterClassA(&wc)) // Use RegisterClassA for ANSI
    {
        DebugPrint("WinMain: Main window registration failed.\n");
        MessageBoxA(NULL, STRING_WINDOW_REG_ERROR_ANSI, "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    DebugPrint("WinMain: Main window class registered successfully.\n");

    // --- Note: Removed registration for NEW_WINDOW_CLASS_NAME_ANSI as we are using a modal dialog ---
    // WNDCLASSA wcBlank = { 0 }; // Use WNDCLASSA for ANSI
    // wcBlank.lpfnWndProc     = BlankWindowProc; // Use the new window procedure
    // wcBlank.hInstance       = hInstance;
    // wcBlank.lpszClassName = NEW_WINDOW_CLASS_NAME_ANSI; // Use the new class name
    // wcBlank.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    // wcBlank.hCursor         = LoadCursor(NULL, IDC_ARROW);
    // wcBlank.lpszMenuName  = NULL; // No menu for the blank window
    // DebugPrint("WinMain: Registering blank window class.\n");
    // if (!RegisterClassA(&wcBlank)) // Use RegisterClassA for ANSI
    // {
    //     DebugPrint("WinMain: Blank window registration failed.\n");
    //     MessageBoxA(NULL, "Blank Window Registration Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
    // }
    // DebugPrint("WinMain: Blank window class registered successfully.\n");


    // --- Load Accelerator Table ---
    // Define the accelerator table structure
    ACCEL AccelTable[] = {
        {FVIRTKEY | FCONTROL, 'R', IDM_FILE_RUN},
        {FVIRTKEY | FCONTROL, 'C', IDM_FILE_COPYOUTPUT},
        {FVIRTKEY | FCONTROL, 'X', IDM_FILE_EXIT}, // Accelerator for Exit
        {FVIRTKEY | FCONTROL, 'O', IDM_FILE_OPEN},  // Accelerator for Open
        {FVIRTKEY | FCONTROL, 'A', IDM_SELECTALL_CODE}, // Ctrl+A for Code Edit
        {FVIRTKEY | FCONTROL, 'A', IDM_SELECTALL_INPUT}, // Ctrl+A for Input Edit
        {FVIRTKEY | FCONTROL, 'A', IDM_SELECTALL_OUTPUT} // Ctrl+A for Output Edit (now that it's an edit control)
    };

    // Create the accelerator table from the structure
    hAccelTable = CreateAcceleratorTableA(AccelTable, sizeof(AccelTable) / sizeof(ACCEL));

    if (hAccelTable == NULL) {
        DebugPrint("WinMain: Failed to create accelerator table.\n");
        // You might want to show a message box here in a real app
    } else {
        DebugPrint("WinMain: Accelerator table created.\n");
    }


    // --- Create the main window ---
    DebugPrint("WinMain: Creating main window.\n");
    HWND hwnd = CreateWindowExA(
        0,                                  // Optional window styles.
        MAIN_WINDOW_CLASS_NAME,             // Window class (ANSI)
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
        DebugPrint("WinMain: Main window creation failed.\n");
        MessageBoxA(NULL, STRING_WINDOW_CREATION_ERROR_ANSI, "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }
    DebugPrint("WinMain: Main window created successfully.\n");

    // --- Show and update the main window ---
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    DebugPrint("WinMain: Main window shown and updated.\n");

    // --- Message Loop ---
    DebugPrint("WinMain: Entering message loop.\n");
    MSG msg = { 0 };
    while (GetMessageA(&msg, NULL, 0, 0) > 0) // Use GetMessageA
    {
        // Check if the message is for a dialog box. If so, let the dialog handle it.
        // This is crucial for modal dialogs created with DialogBoxIndirect.
        if (!IsDialogMessage(GetActiveWindow(), &msg)) {
             // Translate accelerator keys before dispatching the message
            if (!TranslateAcceleratorA(msg.hwnd, hAccelTable, &msg)) {
                TranslateMessage(&msg);
                DispatchMessageA(&msg); // Use DispatchMessageA
            }
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

