#include <windows.h>
#include <stdlib.h> // For malloc, free
#include <string.h> // For string functions if needed (though not directly used here)

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

// Global variables
HINSTANCE hInst;
HFONT hMonoFont = NULL;
HWND hwndCodeEdit = NULL;
HWND hwndInputEdit = NULL;
HWND hwndOutputEdit = NULL;

// Forward declaration of the window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Helper function to append text to an EDIT control (Defined before use)
void AppendText(HWND hwndEdit, const char* newText) {
    // Get the current length of text in the edit control
    int len = GetWindowTextLengthA(hwndEdit);
    // Set the selection to the end of the text
    SendMessageA(hwndEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    // Replace the selection (which is empty and at the end) with the new text
    SendMessageA(hwndEdit, EM_REPLACESEL, 0, (LPARAM)newText);
}


// --- WinMain Entry Point (ANSI version) ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Store instance handle in our global variable
    hInst = hInstance;

    // Define window class name (ANSI string)
    const char CLASS_NAME[] = "BFInterpreterWindowClass";

    // --- Register the window class ---
    WNDCLASSA wc = { 0 }; // Use WNDCLASSA for ANSI

    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.lpszMenuName  = NULL; // We will create menu programmatically

    if (!RegisterClassA(&wc)) // Use RegisterClassA for ANSI
    {
        MessageBoxA(NULL, "Window Registration Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // --- Create the window ---
    HWND hwnd = CreateWindowExA(
        0,                              // Optional window styles.
        CLASS_NAME,                     // Window class (ANSI)
        "Win32 BF Interpreter UI (ANSI)", // Window title (ANSI)
        WS_OVERLAPPEDWINDOW,            // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 550,

        NULL,       // Parent window
        NULL,       // Menu (we create it in WM_CREATE)
        hInstance,  // Instance handle
        NULL        // Additional application data
    );

    if (hwnd == NULL)
    {
        MessageBoxA(NULL, "Window Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // --- Show and update the window ---
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // --- Message Loop ---
    MSG msg = { 0 };
    while (GetMessageA(&msg, NULL, 0, 0) > 0) // Use GetMessageA
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg); // Use DispatchMessageA
    }

    return (int)msg.wParam;
}

// --- Window Procedure ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_CREATE:
        {
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
                 MessageBoxA(hwnd, "Failed to create monospaced font.", "Font Error", MB_OK | MB_ICONWARNING);
            }

            // --- Create Menu ---
            HMENU hMenubar = CreateMenu();
            HMENU hMenuFile = CreateMenu();

            AppendMenuA(hMenuFile, MF_STRING, IDM_FILE_RUN, "&Run\tCtrl+R");
            AppendMenuA(hMenuFile, MF_STRING, IDM_FILE_COPYOUTPUT, "&Copy Output\tCtrl+C");
            AppendMenuA(hMenuFile, MF_SEPARATOR, 0, NULL);
            AppendMenuA(hMenuFile, MF_STRING, IDM_FILE_EXIT, "E&xit");

            AppendMenuA(hMenubar, MF_POPUP, (UINT_PTR)hMenuFile, "&File");

            SetMenu(hwnd, hMenubar);

            // --- Create Controls ---
            // Labels (STATIC)
            CreateWindowA("STATIC", "Code:", WS_CHILD | WS_VISIBLE,
                          10, 10, 40, 20, hwnd, (HMENU)IDC_STATIC_CODE, hInst, NULL);
            CreateWindowA("STATIC", "Standard input:", WS_CHILD | WS_VISIBLE,
                          10, 170, 100, 20, hwnd, (HMENU)IDC_STATIC_INPUT, hInst, NULL);
            CreateWindowA("STATIC", "Standard output:", WS_CHILD | WS_VISIBLE,
                          10, 300, 100, 20, hwnd, (HMENU)IDC_STATIC_OUTPUT, hInst, NULL);

            // Edit Controls (EDIT)
            // Code Input
            hwndCodeEdit = CreateWindowExA(
                WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                10, 35, 560, 125, hwnd, (HMENU)IDC_EDIT_CODE, hInst, NULL);

            // Standard Input
            hwndInputEdit = CreateWindowExA(
                WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                10, 195, 560, 95, hwnd, (HMENU)IDC_EDIT_INPUT, hInst, NULL);

            // Standard Output (Read-only)
            hwndOutputEdit = CreateWindowExA(
                WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                10, 325, 560, 150, hwnd, (HMENU)IDC_EDIT_OUTPUT, hInst, NULL);

            // Set initial text (ANSI)
            SetWindowTextA(hwndCodeEdit, ",[&>,]<[.<]");
            SetWindowTextA(hwndInputEdit, "This is the default stdin-text");
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
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
                case IDM_FILE_RUN:
                { // Added braces to scope variables
                    // Placeholder for running the interpreter logic
                    SetWindowTextA(hwndOutputEdit, ""); // Clear previous output (ANSI)
                    AppendText(hwndOutputEdit, "--- Running (Not Implemented) ---\r\n");

                    // Example: Get code text (ANSI)
                    int codeLen = GetWindowTextLengthA(hwndCodeEdit);
                    if(codeLen > 0) {
                        // Need +1 for null terminator
                        char* codeBuffer = (char*)malloc(codeLen + 1);
                        if(codeBuffer) {
                            GetWindowTextA(hwndCodeEdit, codeBuffer, codeLen + 1);
                            AppendText(hwndOutputEdit, "Code:\r\n");
                            AppendText(hwndOutputEdit, codeBuffer);
                            AppendText(hwndOutputEdit, "\r\n");
                            free(codeBuffer); // Free allocated memory
                        } else {
                            AppendText(hwndOutputEdit, "[Error allocating memory for code]\r\n");
                        }
                    } else {
                         AppendText(hwndOutputEdit, "[No code entered]\r\n");
                    }

                     // Example: Get input text (ANSI)
                    int inputLen = GetWindowTextLengthA(hwndInputEdit);
                    if(inputLen > 0) {
                        // Need +1 for null terminator
                        char* inputBuffer = (char*)malloc(inputLen + 1);
                        if(inputBuffer) {
                            GetWindowTextA(hwndInputEdit, inputBuffer, inputLen + 1);
                            AppendText(hwndOutputEdit, "Input:\r\n");
                            AppendText(hwndOutputEdit, inputBuffer);
                            AppendText(hwndOutputEdit, "\r\n");
                            free(inputBuffer); // Free allocated memory
                        } else {
                             AppendText(hwndOutputEdit, "[Error allocating memory for input]\r\n");
                        }
                    } else {
                         AppendText(hwndOutputEdit, "[No input entered]\r\n");
                    }
                    AppendText(hwndOutputEdit, "--- Execution Placeholder End ---");
                    break; // Break for IDM_FILE_RUN case
                } // End brace for IDM_FILE_RUN scope

                case IDM_FILE_COPYOUTPUT:
                { // Added braces to scope variables
                    int textLen = GetWindowTextLengthA(hwndOutputEdit);
                    if (textLen > 0) {
                        // Need +1 for null terminator
                        HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, textLen + 1);
                        if (hGlobal) {
                            char* pText = (char*)GlobalLock(hGlobal);
                            if(pText) {
                                GetWindowTextA(hwndOutputEdit, pText, textLen + 1);
                                GlobalUnlock(hGlobal);

                                if (OpenClipboard(hwnd)) {
                                    EmptyClipboard();
                                    // Use CF_TEXT for ANSI text
                                    SetClipboardData(CF_TEXT, hGlobal);
                                    CloseClipboard();
                                    // Set hGlobal to NULL so we don't free it below,
                                    // the system owns it now.
                                    hGlobal = NULL;
                                } else {
                                    MessageBoxA(hwnd, "Failed to open clipboard.", "Error", MB_OK | MB_ICONERROR);
                                }
                            } else {
                                 MessageBoxA(hwnd, "Failed to lock memory for clipboard.", "Error", MB_OK | MB_ICONERROR);
                            }
                            // If hGlobal is not NULL here, it means SetClipboardData failed or
                            // was not called, so we should free the memory we allocated.
                            if (hGlobal) {
                                GlobalFree(hGlobal);
                            }
                         } else {
                              MessageBoxA(hwnd, "Failed to allocate memory for clipboard.", "Error", MB_OK | MB_ICONERROR);
                         }
                    } else {
                         // Optional: Notify user if there's nothing to copy
                         // MessageBoxA(hwnd, "Output area is empty.", "Copy", MB_OK | MB_ICONINFORMATION);
                    }
                    break; // Break for IDM_FILE_COPYOUTPUT case
                } // End brace for IDM_FILE_COPYOUTPUT scope

                case IDM_FILE_EXIT:
                    DestroyWindow(hwnd);
                    break;

                default:
                    // Let Windows handle any messages we don't process
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

        case WM_CLOSE:
            // Use MessageBoxA for ANSI
            if (MessageBoxA(hwnd, "Really quit?", "Confirm Exit", MB_OKCANCEL | MB_ICONQUESTION) == IDOK) {
                DestroyWindow(hwnd);
            }
            return 0; // Indicate we handled the message (prevents DefWindowProc from closing)

        case WM_DESTROY:
            if (hMonoFont) {
                DeleteObject(hMonoFont); // Clean up the font object
                hMonoFont = NULL;
            }
            PostQuitMessage(0); // End the message loop
            break;

        default:
            // Let Windows handle any messages we don't process (ANSI version)
            return DefWindowProcA(hwnd, uMsg, wParam, lParam);
    }
    return 0; // Default return for handled messages that don't return earlier
}
