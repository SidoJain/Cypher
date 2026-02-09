# Cypher - Terminal Text Editor

Cypher is a lightweight terminal-based text editor written in C.  
It runs in the terminal in **raw mode**.  
This README file has been completely written using Cypher.  

## Features

- **File Editing**
  - Open and edit text files directly from the terminal.
  - Save changed (`Ctrl-S`) with overwrite support.
  - Time based Undo (`Ctrl-Z`) and Redo (`Ctrl-Y`) functionality.

- **Navigation**
  - Arrow keys for cursor movement.
  - Home/End keys to jump to start/end of a line.
  - Page Up/Page Down for fast scrolling.
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

- **Search & Replace**
  - Incremental search (`Ctrl-F`) with real time navigation between matches.
  - Works with pre-selected text well.
  - Replace (`Ctrl-R`) functionality with interactive mode as well as replace all mode.

- **Status & Message Bars**
  - Displays filename, total lines and cursor line.
  - Temporary message area for prompts, warnings, information.

## Keyboard Shortcuts

| Shortcut                              | Action                            |
|---------------------------------------|--------                           |
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

## Installation & Compilation

- **Requirements:**
  - GCC compiler.
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
git clone https://github.com/SidoJain/Cypher.git
cd Cypher
```

- Make the executable file.

```bash
make
```

- In ~/.bashrc

```bash
# set user friendly alias
alias cypher='cypher.exe'

# add executable file directory to PATH
export PATH="<directory_path>:$PATH"
```

- Use cypher to view/edit any file.

```bash
cypher file.txt
```

## License

This project is licensed under the [MIT License](https://opensource.org/licenses/MIT).
