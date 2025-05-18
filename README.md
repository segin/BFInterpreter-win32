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

This project is a port and the original licensing of BFInterpreter should be considered.
(You might want to add specific license details here if different from the original or if you are relicensing your port).


