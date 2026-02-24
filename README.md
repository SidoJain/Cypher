# Cypher - Terminal Text Editor

Cypher is a lightweight terminal-based text editor written in C, featuring native `Tree-Sitter` semantic syntax highlighting.
It runs in the terminal in **raw mode**.  
This README file has been completely written using Cypher.  

## Features

- **File Editing**
  - Open and edit text files directly from the terminal.
  - Save changed (`Ctrl-S`) with overwrite support.

- **Undo and Redo Engine**
  - Time-based Batching: Groups continuous typing or backspacing into a single undo action.
  - Macro Transactions: Massive operations (like pasting a huge block or a 50-item "Replace All") are grouped and undone in a single keystroke.
  - Save State Tracking: Undoing your way back to the last saved state accurately removes the (modified) flag.

- **Navigation**
  - `Arrow keys` for cursor movement.
  - `Home/End` keys to jump to start/end of a line.
  - `Page Up/Page Down` for fast scrolling.
  - `Ctrl-ArrowKeys` for fast navigation.
  - `Ctrl-G` to jump to a specific line.

- **Mouse Support**
  - Mouse Scroll Wheel to scroll viewport up / down.
  - Mouse left click to jump to row and column.
  - Dragging while mouse left click to select text.

- **Editing Operations**
  - Insert text anywhere.
  - Backspace / Delete characters.
  - Insert new lines (`Enter`).
  - Automatic tab expansion to spaces.
  - Paste command (`Ctrl-V`) from clipboard.
  - Copy (`Ctrl-C`) and Cut (`Ctrl-X`) commands to clipboard.
  - Move whole rows up (`Alt-Up`) or down (`Alt-Down`).
  - Smart Bracket Assist: Auto-closes brackets/quotes.
  - Smart Indentation: Pressing Enter between brackets automatically indents the new line and pushes the closing bracket down.
  - Bracket Matching: Highlights the corresponding open/close bracket when your cursor is over one.
  - Row Manipulation: Move rows up/down (`Alt-Up/Down`) or duplicate them (`Shift-Alt-Up/Down`).

- **Search & Replace**
  - Incremental search (`Ctrl-F`) with real time navigation between matches.
  - Works with pre-selected text well.
  - Replace (`Ctrl-R`) functionality with interactive mode as well as replace all mode.

- **Status & Message Bars**
  - Displays filename, total lines and cursor line.
  - Temporary message area for prompts, warnings, information.

- **Syntax Highlighting (Tree-sitter)**
  - Semantic Parsing: Uses Abstract Syntax Trees (ASTs) instead of brittle regular expressions for flawless, context-aware code highlighting.
  - Dynamic Language Loading: Automatically loads language parsers at runtime via `.so` shared libraries. Adding a new language (C, Python, Rust, Go, etc.) requires zero recompilation of the core editor.
  - True Color (24-bit) Rendering: Renders rich, high-fidelity RGB colors directly in your terminal.
  - Hot-Swappable Themes: Fully customizable styling via a simple theme.config file. Map specific AST nodes directly to hex codes to recreate themes like VS Code Dark+ (default).

## Keyboard Shortcuts

| Shortcut                              | Action                            |
|---------------------------------------|-----------------------------------|
| `Ctrl-Q`                              | Quit editor                       |
| `Ctrl-S`                              | Save current file                 |
| `Ctrl-F`                              | Search in file                    |
| `Ctrl-R`                              | Find and replace                  |
| `Ctrl-A`                              | Select all                        |
| `Ctrl-H`                              | Open keybinds manual              |
| `Ctrl-C`                              | Copy selected text                |
| `Ctrl-X`                              | Cut selected text                 |
| `Ctrl-V`                              | Paste from clipboard              |
| `Ctrl-G / L`                          | Jump to line                      |
| `Ctrl-Z`                              | Undo last major change            |
| `Ctrl-Y`                              | Redo last major change            |
| `Ctrl-D`                              | Debug Tree-Sitter Capture         |
| `Arrow Keys`                          | Move cursor                       |
| `Home / End`                          | Move to start / end of line       |
| `Page Up`                             | Scroll up by one screen           |
| `Page Down`                           | Scroll down by one screen         |
| `Backspace`                           | Delete character left of cursor   |
| `Delete`                              | Delete character under cursor     |
| `Enter`                               | Insert new line                   |
| `Shift-ArrowKeys / Home / End`        | Select text                       |
| `Ctrl-Left / Right`                   | Skip words                        |
| `Ctrl-Up / Down`                      | Scroll up / down                  |
| `Alt-Up / Down`                       | Move row up / down                |
| `Shift-Alt-Up / Down`                 | Copy row up / down                |

## Clipboard Support

Cypher features integrated system clipboard handling that works across local and remote sessions. It employs a dual strategy approach to ensure compatibility across different operating systems and connection types.

### How it Works

1. **MacOS (Local):** When running natively on MacOS, Cypher detects the environment and uses the `pbcopy` utility to communicate directly with the system clipboard.

2. **Linux & Remote (SSH):** For Linux users or those connected via SSH, Cypher utilizes the **OSC 52** terminal escape sequence. This method Base64-encodes the selected text and "tunnels" it through the terminal emulator to your local machine's clipboard.

### Setup & Requirements

To use the clipboard over SSH or on Linux, your terminal emulator must support the OSC 52 protocol.

#### 1. Terminal Compatibility

| Terminal | Compatibility | Action Required |
| :--- | :--- | :--- |
| **iTerm2** | Excellent | Enable "Applications in terminal may access clipboard" in **Settings > General > Applications**. |
| **Alacritty / Kitty** | Native | Supported by default in most versions. |
| **Windows Terminal** | Good | Supported since version 1.17. |
| **VS Code Terminal** | Native | Works out of the box. |
| **Apple Terminal** | **None** | Does not support OSC 52. Consider using [iTerm2](https://iterm2.com/) for remote clipboard support. |

#### 2. Using with tmux

If you use `tmux`, you must configure it to allow clipboard and mouse operations. Add the following line to your `~/.tmux.conf`:

```bash
set -s set-clipboard on
set -g mouse on
set -s escape-time 0
```

### Known Limitations

- Size Constraints: Most terminal emulators limit OSC 52 transfers to approximately 74 KB. Large copy operations exceeding this limit may fail silently.
- Security Settings: Some Linux distributions or hardened terminal configurations disable clipboard writing by default to prevent malicious scripts from altering your clipboard.

## Tree Sitter Setup

To utilize Cypher's semantic syntax highlighting, ensure the following files are present in the directory where Cypher is executed:

1. `theme.config`: A plain text file defining the hexadecimal colors for Tree-Sitter nodes.
2. `parsers/` A folder containing the compiled Tree-Sitter `.so` shared libraries for the languages you want to edit (e.g., `parsers/tree-sitter-c.so`).
3. `queries/` Directory: A folder containing the Tree-Sitter query files (`highlights.scm`) for each language. These files map the structural code parsed by the `.so` files to the color tags defined in your `theme.config`. They must be placed in a subdirectory matching the language name (e.g., `queries/c/highlights.scm`).

If these files are not present, Cypher will safely fall back to plain text editing.

### Current file formats that work with Tree-Sitter

- C
- Python

## Installation & Compilation

- **Requirements:**
  - GCC compiler.
  - Make utility
  - POSIX-compliant system (Linux, macOS, WSL).

- **Compile:**

```bash
make
```

- **Run:**

```bash
# for new file
make run

# for existing file
make run FILE={filename}
```

- **Clean:**

```bash
make clean
```

## Usage

- Clone this repo.

```bash
git clone --recursive https://github.com/SidoJain/Cypher.git
cd Cypher
```

- Make the executable file.

```bash
make
```

- In ~/.bashrc

```bash
# add executable file directory to PATH
export PATH="<directory_path>:$PATH"
```

- Use cypher to view/edit any file.

```bash
cypher file.txt
```

## License

This project is licensed under the [MIT License](https://opensource.org/licenses/MIT).
