# BFInterpreter (Win32 Port)

A Win32 port of the BFInterpreter application.

## Features

* Interprets Brainfuck code.
* Provides separate input and output text areas.
* Menu-driven operations for New, Open, Run, Copy Output, Clear Output, Settings, and Exit.
* Editable code, input, and output fields.
* Configurable debug message settings (saved to the registry).
* Dynamic resizing of About and Settings dialogs to fit content.

## Building

This project uses a Makefile and can be built with MinGW (GCC) or Clang on Windows.

**Prerequisites:**

* A C compiler (GCC or Clang for Windows, e.g., from MinGW-w64 or LLVM).
* A resource compiler (`windres` for GCC, `llvm-rc` for Clang, or `windres` can often be used with Clang too).
* `make` utility.

**Build Steps:**

1.  Ensure your compiler and resource compiler are in your system's PATH.
2.  Navigate to the project directory in a terminal.
3.  Run `make`.
    * By default, it uses `gcc`.
    * To specify a toolchain prefix (e.g., for cross-compilation or a specific MinGW installation):
        ```bash
        make PREFIX=x86_64-w64-mingw32-
        ```
    * The Makefile is currently set up to default to `gcc` and `windres`. If you intend to use Clang, you might need to adjust the `CC` and `RC` variables in the Makefile or provide them on the command line:
        ```bash
        make CC=clang RC=llvm-rc
        ```
        (Ensure `llvm-rc` is available or adjust `RC` to `windres` if using that with Clang).

This will produce `bfinterpreter.exe`.

## Usage

Run `bfinterpreter.exe`.

* **Code:** Enter or open a Brainfuck program.
* **Standard input:** Provide any input your Brainfuck program expects.
* **Standard output:** The program's output will appear here.
* Use the **File** menu to manage programs and execution.
* Use the **Edit** menu for standard text editing operations in the focused text field.
* Use **File > Settings** to configure debug message verbosity.
* Use **Help > About** for program information.

## Files

* `bf.c`: Main application C source code.
* `bf.h`: Header file with definitions and declarations.
* `bf.rc`: Resource script (menus, dialogs, strings, manifest).
* `bf.manifest`: Application manifest for common controls v6.
* `Makefile`: Build script.
* `README.md`: This file.

## License

MIT License

Copyright (c) 2015-2025 Kirn Gill II <segin2005@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

