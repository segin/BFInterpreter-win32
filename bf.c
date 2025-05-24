#include "bf.h"

// Global variables
HINSTANCE hInst;
HFONT hMonoFont = NULL;
HFONT hLabelFont = NULL;
HWND hwndCodeEdit = NULL;
HWND hwndInputEdit = NULL;
HWND hwndOutputEdit = NULL;
HANDLE g_hInterpreterThread = NULL;
volatile BOOL g_bInterpreterRunning = FALSE;
HACCEL hAccelTable = NULL;

// Global debug settings flags
volatile BOOL g_bDebugInterpreter = FALSE;
volatile BOOL g_bDebugOutput = FALSE;
volatile BOOL g_bDebugBasic = TRUE;

// Helper to load strings from resource, ensures null termination
char* LoadStringFromResource(UINT uID, char* buffer, int bufferSize) {
    if (LoadStringA(hInst, uID, buffer, bufferSize) > 0)
        return buffer;
    // Fallback or error string if LoadString fails
    // Using strncpy for safety, ensuring null termination.
    strncpy(buffer, "ErrorLoadingString", bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
    return buffer;
}


// Helper function to append text to an EDIT control
void AppendTextToEditControl(HWND hwndEdit, const char* newText) {
    int len = GetWindowTextLengthA(hwndEdit);
    SendMessageA(hwndEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageA(hwndEdit, EM_REPLACESEL, 0, (LPARAM)newText);
}

// Helper function for conditional debug output
// WARNING: vsprintf is not bounds-checked. Use with extreme caution.
// Ensure buffer is large enough for the expected output.
void DebugPrint(const char* format, ...) {
    if (!g_bDebugBasic)
        return;
    char buffer[512]; // Ensure this is large enough
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args); // Replaced StringCchVPrintfA
    va_end(args);
    OutputDebugStringA(buffer);
}

void DebugPrintInterpreter(const char* format, ...) {
    if (!g_bDebugBasic || !g_bDebugInterpreter)
        return;
    char buffer[512]; // Ensure this is large enough
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args); // Replaced StringCchVPrintfA
    va_end(args);
    OutputDebugStringA(buffer);
}

void DebugPrintOutput(const char* format, ...) {
    if (!g_bDebugBasic || !g_bDebugOutput)
        return;
    char buffer[512]; // Ensure this is large enough
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args); // Replaced StringCchVPrintfA
    va_end(args);
    OutputDebugStringA(buffer);
}

// --- Brainfuck Tape Structure and Functions ---
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
    tape->tape[tape->position]++;
}

void Tape_dec(Tape* tape) {
    tape->tape[tape->position]--;
}

void Tape_forward(Tape* tape) {
    tape->position++;
    if (tape->position >= TAPE_SIZE)
        tape->position = 0;
}

void Tape_reverse(Tape* tape) {
    tape->position--;
    if (tape->position < 0)
        tape->position = TAPE_SIZE - 1;
}

// --- Interpreter Logic ---
void SendBufferedOutput(InterpreterParams* params) {
    if (params->output_buffer_pos > 0) {
        params->output_buffer[params->output_buffer_pos] = '\0';
        char* output_string = strdup(params->output_buffer); // Changed from _strdup
        if (output_string)
            PostMessage(params->hwndMainWindow, WM_APP_INTERPRETER_OUTPUT_STRING, 0, (LPARAM)output_string);
        else {
            DebugPrint("SendBufferedOutput: Failed to duplicate output string.\n");
            char errorBuffer[MAX_STRING_LENGTH];
            LoadStringFromResource(IDS_MEM_ERROR_PARAMS, errorBuffer, MAX_STRING_LENGTH); 
            PostMessage(params->hwndMainWindow, WM_APP_INTERPRETER_OUTPUT_STRING, 0, (LPARAM)strdup(errorBuffer)); // Changed from _strdup
        }
        params->output_buffer_pos = 0;
    }
}

char* optimize_code(const char* code) {
    size_t len = strlen(code);
    char* ocode = (char*)malloc(len + 1);
    if (!ocode)
        return NULL;
    size_t ocode_len = 0;
    for (size_t i = 0; i < len; i++) { 
        switch (code[i]) {
            case '>': case '<': case '+': case '-':
            case ',': case '.': case '[': case ']':
                ocode[ocode_len++] = code[i];
                break;
        }
    }
    ocode[ocode_len] = '\0';
    return ocode;
}

DWORD WINAPI InterpretThreadProc(LPVOID lpParam) {
    DebugPrintInterpreter("Interpreter thread started.\n");
    InterpreterParams* params = (InterpreterParams*)lpParam;
    Tape tape;
    Tape_init(&tape);
    char strBuffer[MAX_STRING_LENGTH];

    char* ocode = optimize_code(params->code);
    if (!ocode) {
        DebugPrintInterpreter("InterpretThreadProc: Failed to optimize code (memory allocation).\n");
        LoadStringFromResource(IDS_MEM_ERROR_OPTIMIZE, strBuffer, MAX_STRING_LENGTH);
        PostMessage(params->hwndMainWindow, WM_APP_INTERPRETER_OUTPUT_STRING, 0, (LPARAM)strdup(strBuffer)); // Changed from _strdup
        PostMessage(params->hwndMainWindow, WM_APP_INTERPRETER_DONE, 1, 0);
        free(params->code);
        free(params->input);
        free(params->output_buffer);
        free(params);
        g_bInterpreterRunning = FALSE;
        return 1;
    }

    size_t pc = 0;
    size_t ocode_len = strlen(ocode);
    int error_status = 0;

    DebugPrintInterpreter("InterpretThreadProc: Starting main loop.\n");
    while (pc < ocode_len && g_bInterpreterRunning && error_status == 0) { 
        char current_instruction = ocode[pc];
        DebugPrintInterpreter("PC: %zu, Instruction: %c\n", pc, current_instruction);

        switch (current_instruction) {
            case '>': Tape_forward(&tape); pc++; break;
            case '<': Tape_reverse(&tape); pc++; break;
            case '+': Tape_inc(&tape); pc++; break;
            case '-': Tape_dec(&tape); pc++; break;
            case ',':
                if (params->input_pos < params->input_len)
                    Tape_set(&tape, (unsigned char)params->input[params->input_pos++]);
                else
                    Tape_set(&tape, 0);
                pc++;
                break;
            case '.':
                if (params->output_buffer_pos < OUTPUT_BUFFER_SIZE - 1)
                    params->output_buffer[params->output_buffer_pos++] = Tape_get(&tape);
                else {
                    SendBufferedOutput(params);
                    params->output_buffer[params->output_buffer_pos++] = Tape_get(&tape);
                }
                pc++;
                break;
            case '[':
                if (Tape_get(&tape) == 0) {
                    int bracket_count = 1;
                    size_t current_pc_loop = pc; 
                    while (bracket_count > 0 && ++current_pc_loop < ocode_len) {
                        if (ocode[current_pc_loop] == '[')
                            bracket_count++;
                        else if (ocode[current_pc_loop] == ']')
                            bracket_count--;
                    }
                    if (current_pc_loop >= ocode_len) {
                        error_status = 1;
                        DebugPrintInterpreter("InterpretThreadProc: Mismatched opening bracket.\n");
                    } else {
                        pc = current_pc_loop + 1;
                    }
                } else
                    pc++;
                break;
            case ']':
                if (Tape_get(&tape) != 0) {
                    int bracket_count = 1;
                    size_t current_pc_loop = pc; 
                    while (bracket_count > 0) {
                        if (current_pc_loop == 0) { 
                            error_status = 1;
                            DebugPrintInterpreter("InterpretThreadProc: Mismatched closing bracket (reached beginning).\n");
                            break;
                        }
                        current_pc_loop--; 
                        if (ocode[current_pc_loop] == ']')
                            bracket_count++;
                        else if (ocode[current_pc_loop] == '[')
                            bracket_count--;
                    }
                     if (error_status == 0 && bracket_count != 0) { 
                         error_status = 1;
                         DebugPrintInterpreter("InterpretThreadProc: Mismatched closing bracket (loop ended unexpectedly).\n");
                     } else if (error_status == 0) {
                        pc = current_pc_loop; 
                     }
                } else
                    pc++;
                break;
        }
        if (!g_bInterpreterRunning) {
            DebugPrintInterpreter("InterpretThreadProc: Stop signal received.\n");
            break;
        }
    }

    SendBufferedOutput(params);

    if (error_status == 1)
        PostMessage(params->hwndMainWindow, WM_APP_INTERPRETER_OUTPUT_STRING, 0, (LPARAM)strdup(LoadStringFromResource(IDS_MISMATCHED_BRACKETS, strBuffer, MAX_STRING_LENGTH))); // Changed from _strdup
    else if (g_bInterpreterRunning)
        DebugPrintInterpreter("InterpretThreadProc: Interpretation finished successfully.\n");

    PostMessage(params->hwndMainWindow, WM_APP_INTERPRETER_DONE, error_status, 0);
    free(ocode);
    free(params->code);
    free(params->input);
    free(params->output_buffer);
    free(params);
    g_bInterpreterRunning = FALSE;
    DebugPrintInterpreter("Interpreter thread exiting.\n");
    return error_status;
}

// --- Settings Dialog Procedure ---
LRESULT CALLBACK SettingsDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam); 
    char strBuffer[MAX_STRING_LENGTH];
    switch (uMsg) {
        case WM_INITDIALOG:
        { 
            DebugPrint("SettingsDlgProc: WM_INITDIALOG received.\n");

            HWND hCheckBasic = GetDlgItem(hwnd, IDC_CHECK_DEBUG_BASIC);
            HWND hCheckInterpreter = GetDlgItem(hwnd, IDC_CHECK_DEBUG_INTERPRETER);
            HWND hCheckOutput = GetDlgItem(hwnd, IDC_CHECK_DEBUG_OUTPUT);
            HWND hOkButton = GetDlgItem(hwnd, IDOK);

            SetWindowTextA(hCheckBasic, LoadStringFromResource(IDS_DEBUG_BASIC_CHK, strBuffer, MAX_STRING_LENGTH));
            SetWindowTextA(hCheckInterpreter, LoadStringFromResource(IDS_DEBUG_INTERPRETER_CHK, strBuffer, MAX_STRING_LENGTH));
            SetWindowTextA(hCheckOutput, LoadStringFromResource(IDS_DEBUG_OUTPUT_CHK, strBuffer, MAX_STRING_LENGTH));
            SetWindowTextA(hOkButton, LoadStringFromResource(IDS_OK, strBuffer, MAX_STRING_LENGTH));
            SetWindowTextA(hwnd, LoadStringFromResource(IDS_SETTINGS_TITLE, strBuffer, MAX_STRING_LENGTH));

            CheckDlgButton(hwnd, IDC_CHECK_DEBUG_BASIC, g_bDebugBasic ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_DEBUG_INTERPRETER, g_bDebugInterpreter ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_DEBUG_OUTPUT, g_bDebugOutput ? BST_CHECKED : BST_UNCHECKED);

            EnableWindow(hCheckInterpreter, g_bDebugBasic);
            EnableWindow(hCheckOutput, g_bDebugBasic);

            HDC hdc = GetDC(hwnd);
            HFONT hFont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
            HFONT hOldFont = NULL;
            if (hFont)
                hOldFont = (HFONT)SelectObject(hdc, hFont);

            TEXTMETRIC tm;
            GetTextMetrics(hdc, &tm);
            int checkboxHeight = tm.tmHeight + tm.tmExternalLeading + 8; 
            int buttonHeight = tm.tmHeight + tm.tmExternalLeading + 14; 
            SIZE size;

            int maxCheckboxTextWidth = 0;
            GetWindowTextA(hCheckBasic, strBuffer, MAX_STRING_LENGTH);
            GetTextExtentPoint32A(hdc, strBuffer, (int)strlen(strBuffer), &size);
            if (size.cx > maxCheckboxTextWidth) maxCheckboxTextWidth = size.cx;

            GetWindowTextA(hCheckInterpreter, strBuffer, MAX_STRING_LENGTH);
            GetTextExtentPoint32A(hdc, strBuffer, (int)strlen(strBuffer), &size);
            if (size.cx > maxCheckboxTextWidth) maxCheckboxTextWidth = size.cx;
            
            GetWindowTextA(hCheckOutput, strBuffer, MAX_STRING_LENGTH);
            GetTextExtentPoint32A(hdc, strBuffer, (int)strlen(strBuffer), &size);
            if (size.cx > maxCheckboxTextWidth) maxCheckboxTextWidth = size.cx;

            int checkboxControlWidth = maxCheckboxTextWidth + GetSystemMetrics(SM_CXMENUCHECK) + 25; 

            GetWindowTextA(hOkButton, strBuffer, MAX_STRING_LENGTH);
            GetTextExtentPoint32A(hdc, strBuffer, (int)strlen(strBuffer), &size);
            int okButtonWidth = size.cx + 50; 

            const int DLG_MARGIN = 15;              
            const int CHECKBOX_V_SPACING = 10;      
            const int CONTROLS_BUTTON_GAP = 20;   
            const int BUTTON_BOTTOM_MARGIN = 15;    

            int currentY = DLG_MARGIN;

            SetWindowPos(hCheckBasic, NULL, DLG_MARGIN, currentY, checkboxControlWidth, checkboxHeight, SWP_NOZORDER);
            currentY += checkboxHeight + CHECKBOX_V_SPACING;
            SetWindowPos(hCheckInterpreter, NULL, DLG_MARGIN, currentY, checkboxControlWidth, checkboxHeight, SWP_NOZORDER);
            currentY += checkboxHeight + CHECKBOX_V_SPACING;
            SetWindowPos(hCheckOutput, NULL, DLG_MARGIN, currentY, checkboxControlWidth, checkboxHeight, SWP_NOZORDER);
            
            currentY += checkboxHeight; 
            currentY += CONTROLS_BUTTON_GAP; 

            int buttonStartX = (checkboxControlWidth + 2 * DLG_MARGIN - okButtonWidth) / 2;
            if (buttonStartX < DLG_MARGIN) buttonStartX = DLG_MARGIN;

            SetWindowPos(hOkButton, NULL, buttonStartX, currentY, okButtonWidth, buttonHeight, SWP_NOZORDER);
            currentY += buttonHeight; 
            currentY += BUTTON_BOTTOM_MARGIN; 

            int dialogWidth = checkboxControlWidth + 2 * DLG_MARGIN;
            int dialogHeight = currentY;

            RECT rcOwner, rcDlg;
            GetWindowRect(GetParent(hwnd), &rcOwner);
            GetWindowRect(hwnd, &rcDlg); 
            
            int newX = rcOwner.left + (rcOwner.right - rcOwner.left - dialogWidth) / 2;
            int newY = rcOwner.top + (rcOwner.bottom - rcOwner.top - dialogHeight) / 2;

            SetWindowPos(hwnd, HWND_TOP, newX, newY, dialogWidth, dialogHeight, SWP_SHOWWINDOW);

            if (hOldFont)
                SelectObject(hdc, hOldFont);
            ReleaseDC(hwnd, hdc);
            return (LRESULT)TRUE;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK:
                    DebugPrint("SettingsDlgProc: IDOK received.\n");
                    g_bDebugBasic = IsDlgButtonChecked(hwnd, IDC_CHECK_DEBUG_BASIC) == BST_CHECKED;
                    g_bDebugInterpreter = IsDlgButtonChecked(hwnd, IDC_CHECK_DEBUG_INTERPRETER) == BST_CHECKED;
                    g_bDebugOutput = IsDlgButtonChecked(hwnd, IDC_CHECK_DEBUG_OUTPUT) == BST_CHECKED;
                    if (!g_bDebugBasic) {
                        g_bDebugInterpreter = FALSE;
                        g_bDebugOutput = FALSE;
                    }
                    SaveDebugSettingsToRegistry();
                    EndDialog(hwnd, IDOK);
                    break;
                case IDCANCEL: 
                    DebugPrint("SettingsDlgProc: IDCANCEL (from ESC or close) received.\n");
                    EndDialog(hwnd, IDCANCEL);
                    break;
                case IDC_CHECK_DEBUG_BASIC:
                { 
                    BOOL bBasicChecked = IsDlgButtonChecked(hwnd, IDC_CHECK_DEBUG_BASIC) == BST_CHECKED;
                    EnableWindow(GetDlgItem(hwnd, IDC_CHECK_DEBUG_INTERPRETER), bBasicChecked);
                    EnableWindow(GetDlgItem(hwnd, IDC_CHECK_DEBUG_OUTPUT), bBasicChecked);
                    if (!bBasicChecked) {
                        CheckDlgButton(hwnd, IDC_CHECK_DEBUG_INTERPRETER, BST_UNCHECKED);
                        CheckDlgButton(hwnd, IDC_CHECK_DEBUG_OUTPUT, BST_UNCHECKED);
                    }
                    break;
                }
            }
            return (LRESULT)TRUE;
        case WM_CLOSE: 
            EndDialog(hwnd, IDCANCEL); 
            return (LRESULT)TRUE;
    }
    return (LRESULT)FALSE;
}

// --- About Dialog Procedure ---
LRESULT CALLBACK AboutDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam); 
    char aboutTextBuffer[MAX_STRING_LENGTH * 2]; 
    char titleBuffer[MAX_STRING_LENGTH];
    char okButtonBuffer[MAX_STRING_LENGTH];

    switch (uMsg) {
        case WM_INITDIALOG:
        { 
            DebugPrint("AboutDlgProc: WM_INITDIALOG received.\n");
            
            HWND hStaticText = GetDlgItem(hwnd, IDC_STATIC_ABOUT_TEXT);
            HWND hOkButton = GetDlgItem(hwnd, IDOK);

            LoadStringFromResource(IDS_ABOUT_TEXT, aboutTextBuffer, sizeof(aboutTextBuffer));
            LoadStringFromResource(IDS_OK, okButtonBuffer, sizeof(okButtonBuffer));
            LoadStringFromResource(IDS_ABOUT_TITLE, titleBuffer, sizeof(titleBuffer));

            SetWindowTextA(hStaticText, aboutTextBuffer);
            SetWindowTextA(hOkButton, okButtonBuffer);
            SetWindowTextA(hwnd, titleBuffer);

            HDC hdc = GetDC(hwnd);
            HFONT hFont = (HFONT)SendMessage(hStaticText, WM_GETFONT, 0, 0); 
            HFONT hOldFont = NULL;
            if (hFont)
                hOldFont = (HFONT)SelectObject(hdc, hFont);

            RECT textRect = {0, 0, 0, 0};
            int calcTextRectWidth = 300; 
            textRect.right = calcTextRectWidth; 
            DrawTextA(hdc, aboutTextBuffer, -1, &textRect, DT_CALCRECT | DT_WORDBREAK);
            
            int textHeight = textRect.bottom - textRect.top;
            int textWidth = textRect.right - textRect.left; 

            TEXTMETRIC tm;
            GetTextMetrics(hdc, &tm);
            int buttonHeight = tm.tmHeight + tm.tmExternalLeading + 14; 
            
            SIZE okButtonSize;
            GetTextExtentPoint32A(hdc, okButtonBuffer, (int)strlen(okButtonBuffer), &okButtonSize);
            int buttonWidth = okButtonSize.cx + 50; 

            const int DLG_MARGIN = 20;              
            const int TEXT_BUTTON_GAP = 15;       
            const int BUTTON_BOTTOM_MARGIN = 20;    

            int contentWidth = (textWidth > buttonWidth) ? textWidth : buttonWidth;
            int dialogWidth = contentWidth + 2 * DLG_MARGIN;
            int minDialogWidth = 250; 
            if (dialogWidth < minDialogWidth)
                dialogWidth = minDialogWidth;
            
            RECT finalTextRect = {0,0, dialogWidth - 2 * DLG_MARGIN, 0};
            DrawTextA(hdc, aboutTextBuffer, -1, &finalTextRect, DT_CALCRECT | DT_WORDBREAK);
            textHeight = finalTextRect.bottom - finalTextRect.top;

            int currentY = DLG_MARGIN;
            SetWindowPos(hStaticText, NULL, DLG_MARGIN, currentY, dialogWidth - 2 * DLG_MARGIN, textHeight, SWP_NOZORDER);
            currentY += textHeight; 
            currentY += TEXT_BUTTON_GAP;

            int buttonX = (dialogWidth - buttonWidth) / 2;
            SetWindowPos(hOkButton, NULL, buttonX, currentY, buttonWidth, buttonHeight, SWP_NOZORDER);
            currentY += buttonHeight;
            currentY += BUTTON_BOTTOM_MARGIN;

            int dialogHeight = currentY;

            RECT rcOwner, rcDlg;
            GetWindowRect(GetParent(hwnd), &rcOwner); 
            GetWindowRect(hwnd, &rcDlg); 
            
            int newX = rcOwner.left + (rcOwner.right - rcOwner.left - dialogWidth) / 2;
            int newY = rcOwner.top + (rcOwner.bottom - rcOwner.top - dialogHeight) / 2;

            SetWindowPos(hwnd, HWND_TOP, newX, newY, dialogWidth, dialogHeight, SWP_SHOWWINDOW);

            if (hOldFont)
                SelectObject(hdc, hOldFont);
            ReleaseDC(hwnd, hdc);
            return (LRESULT)TRUE;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
                EndDialog(hwnd, LOWORD(wParam));
            return (LRESULT)TRUE;
        case WM_CLOSE: 
            EndDialog(hwnd, IDCANCEL); 
            return (LRESULT)TRUE;
    }
    return (LRESULT)FALSE;
}

// --- Registry Functions ---
void SaveDebugSettingsToRegistry() {
    HKEY hKey;
    LONG lResult;
    DebugPrint("SaveDebugSettingsToRegistry: Attempting to open/create registry key.\n");
    lResult = RegCreateKeyExA(HKEY_CURRENT_USER, REG_APP_KEY_ANSI, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (lResult != ERROR_SUCCESS) {
        DebugPrint("SaveDebugSettingsToRegistry: RegCreateKeyExA failed with error %lu.\n", lResult);
        return;
    }
    DWORD dwDebugBasic = g_bDebugBasic ? 1 : 0;
    DWORD dwDebugInterpreter = g_bDebugInterpreter ? 1 : 0;
    DWORD dwDebugOutput = g_bDebugOutput ? 1 : 0;

    RegSetValueExA(hKey, REG_VALUE_DEBUG_BASIC_ANSI, 0, REG_DWORD, (const BYTE*)&dwDebugBasic, sizeof(dwDebugBasic));
    RegSetValueExA(hKey, REG_VALUE_DEBUG_INTERPRETER_ANSI, 0, REG_DWORD, (const BYTE*)&dwDebugInterpreter, sizeof(dwDebugInterpreter));
    RegSetValueExA(hKey, REG_VALUE_DEBUG_OUTPUT_ANSI, 0, REG_DWORD, (const BYTE*)&dwDebugOutput, sizeof(dwDebugOutput));
    RegCloseKey(hKey);
    DebugPrint("SaveDebugSettingsToRegistry: Registry key closed.\n");
}

void LoadDebugSettingsFromRegistry() {
    HKEY hKey;
    LONG lResult;
    DWORD dwType, dwSize, dwValue;

    DebugPrint("LoadDebugSettingsFromRegistry: Attempting to open registry key.\n");
    lResult = RegOpenKeyExA(HKEY_CURRENT_USER, REG_APP_KEY_ANSI, 0, KEY_READ, &hKey);
    if (lResult != ERROR_SUCCESS) {
        DebugPrint("LoadDebugSettingsFromRegistry: RegOpenKeyExA failed with error %lu. Using default settings.\n", lResult);
        return;
    }

    dwSize = sizeof(dwValue);
    if (RegQueryValueExA(hKey, REG_VALUE_DEBUG_BASIC_ANSI, NULL, &dwType, (LPBYTE)&dwValue, &dwSize) == ERROR_SUCCESS && dwType == REG_DWORD)
        g_bDebugBasic = (dwValue != 0);
    dwSize = sizeof(dwValue);
    if (RegQueryValueExA(hKey, REG_VALUE_DEBUG_INTERPRETER_ANSI, NULL, &dwType, (LPBYTE)&dwValue, &dwSize) == ERROR_SUCCESS && dwType == REG_DWORD)
        g_bDebugInterpreter = (dwValue != 0);
    dwSize = sizeof(dwValue);
    if (RegQueryValueExA(hKey, REG_VALUE_DEBUG_OUTPUT_ANSI, NULL, &dwType, (LPBYTE)&dwValue, &dwSize) == ERROR_SUCCESS && dwType == REG_DWORD)
        g_bDebugOutput = (dwValue != 0);

    if (!g_bDebugBasic) {
        g_bDebugInterpreter = FALSE;
        g_bDebugOutput = FALSE;
    }
    RegCloseKey(hKey);
    DebugPrint("LoadDebugSettingsFromRegistry: Registry key closed.\n");
}


// --- Window Procedure ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    char strBuffer[MAX_STRING_LENGTH];
    char fileBuffer[MAX_PATH]; 

    switch (uMsg) {
        case WM_CREATE:
        { 
            DebugPrint("WM_CREATE received.\n");
            hMonoFont = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                   FIXED_PITCH | FF_MODERN, "Courier New");
            if (hMonoFont == NULL)
                MessageBoxA(hwnd, LoadStringFromResource(IDS_FONT_ERROR, strBuffer, MAX_STRING_LENGTH), "Font Error", MB_OK | MB_ICONWARNING);

            NONCLIENTMETRICSA ncm = {0}; 
            ncm.cbSize = sizeof(NONCLIENTMETRICSA); 
            if (SystemParametersInfoA(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)) {
                 ncm.lfMessageFont.lfItalic = TRUE;
                 hLabelFont = CreateFontIndirectA(&ncm.lfMessageFont);
            } else {
                hLabelFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT); 
                DebugPrint("WM_CREATE: SystemParametersInfoA for NONCLIENTMETRICS failed. Using default GUI font for labels.\n");
            }

            HWND hStaticCode = CreateWindowA(WC_STATICA, LoadStringFromResource(IDS_CODE_LABEL, strBuffer, MAX_STRING_LENGTH), WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
                          10, 10, 100, 20, hwnd, (HMENU)IDC_STATIC_CODE, hInst, NULL);
            if (hLabelFont)
                SendMessageA(hStaticCode, WM_SETFONT, (WPARAM)hLabelFont, TRUE);

            HWND hStaticInput = CreateWindowA(WC_STATICA, LoadStringFromResource(IDS_INPUT_LABEL, strBuffer, MAX_STRING_LENGTH), WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
                          10, 170, 150, 20, hwnd, (HMENU)IDC_STATIC_INPUT, hInst, NULL);
            if (hLabelFont)
                SendMessageA(hStaticInput, WM_SETFONT, (WPARAM)hLabelFont, TRUE);

            HWND hStaticOutput = CreateWindowA(WC_STATICA, LoadStringFromResource(IDS_OUTPUT_LABEL, strBuffer, MAX_STRING_LENGTH), WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
                          10, 300, 150, 20, hwnd, (HMENU)IDC_STATIC_OUTPUT, hInst, NULL);
            if (hLabelFont)
                SendMessageA(hStaticOutput, WM_SETFONT, (WPARAM)hLabelFont, TRUE);

            hwndCodeEdit = CreateWindowExA(WS_EX_CLIENTEDGE, WC_EDITA, "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_TABSTOP,
                10, 35, 560, 125, hwnd, (HMENU)IDC_EDIT_CODE, hInst, NULL);
            if (hMonoFont)
                SendMessageA(hwndCodeEdit, WM_SETFONT, (WPARAM)hMonoFont, TRUE);

            hwndInputEdit = CreateWindowExA(WS_EX_CLIENTEDGE, WC_EDITA, "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_TABSTOP,
                10, 195, 560, 95, hwnd, (HMENU)IDC_EDIT_INPUT, hInst, NULL);
            if (hMonoFont)
                SendMessageA(hwndInputEdit, WM_SETFONT, (WPARAM)hMonoFont, TRUE);

            hwndOutputEdit = CreateWindowExA(WS_EX_CLIENTEDGE, WC_EDITA, "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | WS_TABSTOP, 
                10, 325, 560, 150, hwnd, (HMENU)IDC_EDIT_OUTPUT, hInst, NULL);
            if (hMonoFont)
                SendMessageA(hwndOutputEdit, WM_SETFONT, (WPARAM)hMonoFont, TRUE);

            SetWindowTextA(hwndCodeEdit, LoadStringFromResource(IDS_DEFAULT_CODE, strBuffer, MAX_STRING_LENGTH));
            SetWindowTextA(hwndInputEdit, LoadStringFromResource(IDS_DEFAULT_INPUT, strBuffer, MAX_STRING_LENGTH));
            
            SetFocus(hwndCodeEdit);
            break;
        }
        case WM_SIZE:
        { 
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            int margin = 10, labelHeight = 20, editTopMargin = 5, spacing = 10, minEditHeight = 30;
            int currentY = margin;

            MoveWindow(GetDlgItem(hwnd, IDC_STATIC_CODE), margin, currentY, width - 2 * margin, labelHeight, TRUE);
            currentY += labelHeight + editTopMargin;
            int codeEditHeight = height / 4;
            if (codeEditHeight < minEditHeight)
                codeEditHeight = minEditHeight;
            MoveWindow(hwndCodeEdit, margin, currentY, width - 2 * margin, codeEditHeight, TRUE);
            currentY += codeEditHeight + spacing;

            MoveWindow(GetDlgItem(hwnd, IDC_STATIC_INPUT), margin, currentY, width - 2 * margin, labelHeight, TRUE);
            currentY += labelHeight + editTopMargin;
            int inputEditHeight = height / 6;
            if (inputEditHeight < minEditHeight)
                inputEditHeight = minEditHeight;
            MoveWindow(hwndInputEdit, margin, currentY, width - 2 * margin, inputEditHeight, TRUE);
            currentY += inputEditHeight + spacing;

            MoveWindow(GetDlgItem(hwnd, IDC_STATIC_OUTPUT), margin, currentY, width - 2 * margin, labelHeight, TRUE);
            currentY += labelHeight + editTopMargin;
            int outputEditHeight = height - currentY - margin;
            if (outputEditHeight < minEditHeight)
                outputEditHeight = minEditHeight;
            MoveWindow(hwndOutputEdit, margin, currentY, width - 2 * margin, outputEditHeight, TRUE); 
            break;
        }
        case WM_COMMAND:
        { 
            int wmId = LOWORD(wParam);
            switch (wmId) {
                case IDM_FILE_NEW:
                    SetWindowTextA(hwndCodeEdit, "");
                    SetWindowTextA(hwndInputEdit, "");
                    SetWindowTextA(hwndOutputEdit, "");
                    SetFocus(hwndCodeEdit);
                    break;
                case IDM_FILE_OPEN:
                { 
                    OPENFILENAMEA ofn = {0};
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hwnd;
                    ofn.lpstrFile = fileBuffer;
                    fileBuffer[0] = '\0';
                    ofn.nMaxFile = sizeof(fileBuffer);
                    ofn.lpstrFilter = "Brainfuck Source (*.bf;*.b)\0*.bf;*.b\0All Files (*.*)\0*.*\0";
                    ofn.nFilterIndex = 1;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
                    ofn.lpstrTitle = LoadStringFromResource(IDS_OPEN_FILE_TITLE, strBuffer, MAX_STRING_LENGTH);

                    if (GetOpenFileNameA(&ofn) == TRUE) {
                        HANDLE hFile = CreateFileA(ofn.lpstrFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                        if (hFile != INVALID_HANDLE_VALUE) {
                            DWORD fileSize = GetFileSize(hFile, NULL);
                            if (fileSize != INVALID_FILE_SIZE && fileSize < 1024 * 1024) { 
                                char* pFileContent = (char*)malloc(fileSize + 1);
                                if (pFileContent) {
                                    DWORD bytesRead;
                                    if (ReadFile(hFile, pFileContent, fileSize, &bytesRead, NULL)) {
                                        pFileContent[bytesRead] = '\0';
                                        SetWindowTextA(hwndCodeEdit, pFileContent);
                                    } else
                                         MessageBoxA(hwnd, "Error reading file.", "File Error", MB_OK | MB_ICONERROR);
                                    free(pFileContent);
                                } else
                                    MessageBoxA(hwnd, LoadStringFromResource(IDS_MEM_ERROR_CODE, strBuffer, MAX_STRING_LENGTH), "Memory Error", MB_OK | MB_ICONERROR);
                            } else
                                 MessageBoxA(hwnd, "File too large or error getting size.", "File Error", MB_OK | MB_ICONERROR);
                            CloseHandle(hFile);
                        } else
                            MessageBoxA(hwnd, "Error opening file.", "File Error", MB_OK | MB_ICONERROR);
                    }
                    break;
                }
                case IDM_FILE_SETTINGS:
                    DialogBox(hInst, MAKEINTRESOURCE(IDD_SETTINGS), hwnd, SettingsDlgProc);
                    break;
                case IDM_FILE_RUN:
                { 
                    if (!g_bInterpreterRunning) {
                        SetWindowTextA(hwndOutputEdit, ""); 
                        int code_len = GetWindowTextLengthA(hwndCodeEdit);
                        char* code_text = (char*)malloc(code_len + 1);
                        int input_len = GetWindowTextLengthA(hwndInputEdit);
                        char* input_text = (char*)malloc(input_len + 1);
                        InterpreterParams* params = (InterpreterParams*)malloc(sizeof(InterpreterParams));

                        if (!code_text || !input_text || !params) {
                            MessageBoxA(hwnd, LoadStringFromResource(IDS_MEM_ERROR_PARAMS, strBuffer, MAX_STRING_LENGTH), "Error", MB_OK);
                            free(code_text); free(input_text); free(params);
                            break;
                        }
                        GetWindowTextA(hwndCodeEdit, code_text, code_len + 1);
                        GetWindowTextA(hwndInputEdit, input_text, input_len + 1);

                        params->hwndMainWindow = hwnd;
                        params->code = code_text; 
                        params->input = input_text;
                        params->input_len = input_len;
                        params->input_pos = 0;
                        params->output_buffer = (char*)malloc(OUTPUT_BUFFER_SIZE);
                        if (!params->output_buffer) {
                             MessageBoxA(hwnd, LoadStringFromResource(IDS_MEM_ERROR_PARAMS, strBuffer, MAX_STRING_LENGTH), "Error", MB_OK); 
                             free(code_text); free(input_text); free(params);
                             break;
                        }
                        params->output_buffer_pos = 0;

                        g_bInterpreterRunning = TRUE;
                        g_hInterpreterThread = CreateThread(NULL, 0, InterpretThreadProc, params, 0, NULL);
                        if (g_hInterpreterThread == NULL) {
                            g_bInterpreterRunning = FALSE;
                            MessageBoxA(hwnd, LoadStringFromResource(IDS_THREAD_ERROR, strBuffer, MAX_STRING_LENGTH), "Error", MB_OK);
                            free(code_text); free(input_text); free(params->output_buffer); free(params);
                        } else {
                            CloseHandle(g_hInterpreterThread); 
                            g_hInterpreterThread = NULL;
                        }
                    }
                    break;
                }
                case IDM_FILE_COPYOUTPUT:
                { 
                    int textLen = GetWindowTextLengthA(hwndOutputEdit);
                    if (textLen > 0) {
                        HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, textLen + 1);
                        if (hGlobal) {
                            char* pText = (char*)GlobalLock(hGlobal);
                            if (pText) {
                                GetWindowTextA(hwndOutputEdit, pText, textLen + 1);
                                GlobalUnlock(hGlobal);
                                if (OpenClipboard(hwnd)) {
                                    EmptyClipboard();
                                    SetClipboardData(CF_TEXT, hGlobal);
                                    CloseClipboard();
                                    MessageBoxA(hwnd, LoadStringFromResource(IDS_COPIED_TO_CLIPBOARD, strBuffer, MAX_STRING_LENGTH), LoadStringFromResource(IDS_APP_TITLE, strBuffer, MAX_STRING_LENGTH), MB_OK | MB_ICONINFORMATION);
                                    hGlobal = NULL; 
                                } else
                                     MessageBoxA(hwnd, LoadStringFromResource(IDS_CLIPBOARD_OPEN_ERROR, strBuffer, MAX_STRING_LENGTH), "Error", MB_OK | MB_ICONERROR);
                            } else
                                MessageBoxA(hwnd, LoadStringFromResource(IDS_CLIPBOARD_MEM_LOCK_ERROR, strBuffer, MAX_STRING_LENGTH), "Error", MB_OK | MB_ICONERROR);
                            if (hGlobal)
                                GlobalFree(hGlobal);
                        } else
                            MessageBoxA(hwnd, LoadStringFromResource(IDS_CLIPBOARD_MEM_ALLOC_ERROR, strBuffer, MAX_STRING_LENGTH), "Error", MB_OK | MB_ICONERROR);
                    }
                    break;
                }
                case IDM_FILE_CLEAROUTPUT:
                    SetWindowTextA(hwndOutputEdit, "");
                    break;
                case IDM_FILE_EXIT:
                    DestroyWindow(hwnd);
                    break;
                case IDM_EDIT_CUT: SendMessage(GetFocus(), WM_CUT, 0, 0); break;
                case IDM_EDIT_COPY: SendMessage(GetFocus(), WM_COPY, 0, 0); break;
                case IDM_EDIT_PASTE: SendMessage(GetFocus(), WM_PASTE, 0, 0); break;
                case IDM_EDIT_SELECTALL:
                {
                    HWND hFocused = GetFocus();
                    if (hFocused == hwndCodeEdit || hFocused == hwndInputEdit || hFocused == hwndOutputEdit)
                        SendMessage(hFocused, EM_SETSEL, 0, -1);
                    break;
                }
                case IDM_HELP_ABOUT:
                    DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUT), hwnd, AboutDlgProc);
                    break;
                default:
                    return DefWindowProc(hwnd, uMsg, wParam, lParam);
            }
            break;
        }
        case WM_CTLCOLORSTATIC:
             return (LRESULT)GetStockObject(NULL_BRUSH);
        case WM_APP_INTERPRETER_OUTPUT_STRING:
        { 
            DebugPrintOutput("WM_APP_INTERPRETER_OUTPUT_STRING received.\n");
            LPCSTR szString = (LPCSTR)lParam;
            if (szString) {
                size_t original_len = strlen(szString);
                char* converted_string = (char*)malloc(original_len * 2 + 1); 
                if (converted_string) {
                    char* p_in = (char*)szString;
                    char* p_out = converted_string;
                    while (*p_in) {
                        if (*p_in == '\n' && (p_in == szString || *(p_in -1) != '\r')) {
                            *p_out++ = '\r';
                        }
                        *p_out++ = *p_in++;
                    }
                    *p_out = '\0';
                    AppendTextToEditControl(hwndOutputEdit, converted_string);
                    free(converted_string);
                } else
                     AppendTextToEditControl(hwndOutputEdit, "\r\nError: Mem alloc for output conversion failed.\r\n");
                free((void*)lParam); 
            }
            SendMessageA(hwndOutputEdit, EM_SCROLLCARET, 0, 0); 
            return 0;
        }
        case WM_APP_INTERPRETER_DONE:
            DebugPrint("WM_APP_INTERPRETER_DONE received.\n");
            g_bInterpreterRunning = FALSE;
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            DebugPrint("WM_DESTROY received.\n");
            g_bInterpreterRunning = FALSE; 
            if (hMonoFont)
                DeleteObject(hMonoFont);
            if (hLabelFont)
                DeleteObject(hLabelFont);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// --- WinMain ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance); 
    UNREFERENCED_PARAMETER(lpCmdLine);     

    DebugPrint("WinMain started.\n");
    hInst = hInstance;
    char strBuffer[MAX_STRING_LENGTH];

    INITCOMMONCONTROLSEX iccex;
    iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    iccex.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    if (!InitCommonControlsEx(&iccex)) {
        MessageBoxA(NULL, "Common Controls Init Failed!", "Error", MB_ICONERROR);
        return 1;
    }

    LoadDebugSettingsFromRegistry();

    const char MAIN_WINDOW_CLASS_NAME[] = "BFInterpreterWindowClassResource";
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = MAIN_WINDOW_CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszMenuName = MAKEINTRESOURCE(IDM_MAINMENU); 
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hIcon = LoadIcon(hInstance, IDI_APPLICATION); 

    if (!RegisterClassA(&wc)) {
        MessageBoxA(NULL, LoadStringFromResource(IDS_WINDOW_REG_ERROR, strBuffer, MAX_STRING_LENGTH), "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDA_ACCELERATORS));
    if (hAccelTable == NULL)
        DebugPrint("WinMain: Failed to load accelerator table.\n");

    HWND hwnd = CreateWindowExA(0, MAIN_WINDOW_CLASS_NAME, LoadStringFromResource(IDS_APP_TITLE, strBuffer, MAX_STRING_LENGTH),
                              WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 600, 550,
                              NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) {
        MessageBoxA(NULL, LoadStringFromResource(IDS_WINDOW_CREATION_ERROR, strBuffer, MAX_STRING_LENGTH), "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {0};
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        if (!TranslateAcceleratorA(hwnd, hAccelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    if (hAccelTable)
        DestroyAcceleratorTable(hAccelTable);
    DebugPrint("WinMain finished.\n");
    return (int)msg.wParam;
}

