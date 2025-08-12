# Cypher - Terminal Text Editor

Cypher is a lightweight terminal-based text editor written in C.
It runs in the terminal in **raw mode**.
This README file has been completely written in Cypher.

## Features

- **File Editing**
  - Open and edit text files directly from the terminal.
  - Save changed (`Ctrl-S`) with overwrite support.

- **Navigation**
  - Arrow keys for cursor movement.
  - Home/End keys to jump to start/end of a line.
  - Page Up/Page Down for fast scrolling.
  - `Ctrl-ArrowKeys` for fast navigation.

- **Editing Operations**
  - Insert text anywhere.
  - Backspace/Delete characters.
  - Insert new lines (`Enter` key).
  - Automatic tab expansion to spaces.

- **Search**
  - Incremental search (`Ctrl-F`) with real time navigation between matches.

- **Status & Message Bars**
  - Displays filename, total lines and cursor line.
  - Temporary message area for prompts, warnings, information.

## Keyboard Shortcuts

| Shortcut                              | Action |
|---------------------------------------|--------|
| `Ctrl-Q`                              | Quit editor |
| `Ctrl-S`                              | Save current file |
| `Ctrl-F`                              | Search in file |
| `Ctrl-A`                              | Select all |
| `Ctrl-H`                              | Open help page |
| `Ctrl-C`                              | Copy selected text |
| `Ctrl-X`                              | Cut selected text |
| `Ctrl-V`                              | Paste from clipboard |
| `Arrow Keys`                          | Move cursor |
| `Home / End`                          | Move to start / end of line |
| `Page Up`                             | Scroll up by one screen |
| `Page Down`                           | Scroll down by one screen |
| `Backspace`                           | Delete character left of cursor |
| `Delete`                              | Delete character under cursor |
| `Enter`                               | Insert new line |
| `Shift + Arrow Keys / Home / End`     | Select text |
| `Ctrl + Left / Right`                 | Skip words |
| `Ctrl + Up / Down`                    | Scroll up / down |
| `Ctrl + Shift + Left / Right`         | Combine Ctrl and Shift  |

## Installation & Compilation

**Requirements:**

- GCC compiler.
- POSIX-compliant system (Linux, macOS, WSL).

**Compile:**

```bash
make
```

**Run:**

```bash
# for new file
make run

# for existing file
make run FILE={filename}
```

**Clean:**

```bash
make clean
```

## License

This project is licensed under the [MIT License](https://opensource.org/licenses/MIT).
