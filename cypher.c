/*** Includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <time.h>
#include <stdarg.h>
#include <fcntl.h>

/*** Defines ***/

#define CYPHER_VERSION      "1.2.1"
#define EMPTY_LINE_SYMBOL   "~"

#define CTRL_KEY(k)         ((k) & 0x1f)
#define APPEND_BUFFER_INIT  {NULL, 0}

#define TAB_SIZE                4
#define QUIT_TIMES              2
#define SAVE_TIMES              2
#define UNDO_REDO_STACK_SIZE    100
#define UNDO_TIMEOUT            1000
#define STATUS_LENGTH           80
#define SMALL_BUFFER_SIZE       32
#define BUFFER_SIZE             128
#define PASTE_CHUNK_SIZE        256
#define STATUS_MSG_TIMEOUT_SEC  5
#define MARGIN                  3

#define NEW_LINE                "\r\n"
#define ESCAPE_CHAR             '\x1b'
#define CLEAR_SCREEN            "\x1b[2J"
#define CLEAR_LINE              "\x1b[K"
#define CURSOR_RESET            "\x1b[H"
#define CURSOR_FORWARD          "\x1b[999C"
#define CURSOR_DOWN             "\x1b[999B"
#define QUERY_CURSOR_POSITION   "\x1b[6n"
#define SHOW_CURSOR             "\x1b[?25h"
#define HIDE_CURSOR             "\x1b[?25l"
#define REMOVE_GRAPHICS         "\x1b[m"
#define INVERTED_COLORS         "\x1b[7m"
#define YELLOW_FG_COLOR         "\x1b[33m"
#define DARK_GRAY_BG_COLOR      "\x1b[48;5;238m"
#define LIGHT_GRAY_BG_COLOR     "\x1b[48;5;242m"
#define ENABLE_MOUSE            "\x1b[?1000h\x1b[?1002h\x1b[?1015h\x1b[?1006h"
#define DISABLE_MOUSE           "\x1b[?1006l\x1b[?1015l\x1b[?1002l\x1b[?1000l"

/*** Structs and Enums ***/

enum editorKey {
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    SHIFT_ARROW_LEFT,
    SHIFT_ARROW_RIGHT,
    SHIFT_ARROW_UP,
    SHIFT_ARROW_DOWN,
    SHIFT_HOME,
    SHIFT_END,
    CTRL_ARROW_LEFT,
    CTRL_ARROW_RIGHT,
    CTRL_ARROW_UP,
    CTRL_ARROW_DOWN,
    CTRL_SHIFT_ARROW_LEFT,
    CTRL_SHIFT_ARROW_RIGHT,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN,
    ALT_ARROW_UP,
    ALT_ARROW_DOWN,
    ALT_SHIFT_ARROW_UP,
    ALT_SHIFT_ARROW_DOWN,
    MOUSE_SCROLL_UP = 2000,
    MOUSE_SCROLL_DOWN,
    MOUSE_LEFT_CLICK,
    MOUSE_DRAG,
    MOUSE_LEFT_RELEASE
};

enum environment {
    WSL,
    LINUX,
    MACOS,
    UNKNOWN
};

typedef struct {
    int size;
    int rsize;
    char *chars;
    char *render;
} editorRow;

typedef struct {
    int cursor_x;
    int cursor_y;
    int render_x;
    int preferred_x;
    int screen_rows;
    int screen_cols;
    int row_offset;
    int col_offset;
    int num_rows;
    editorRow *row;
    int dirty;
    char *filename;
    char status_msg[STATUS_LENGTH];
    time_t status_msg_time;
    struct termios original_termios;
    int select_mode;
    int select_sx;
    int select_sy;
    int select_ex;
    int select_ey;
    char *clipboard;
    int is_pasting;
    char *find_query;
    int *find_match_lines;
    int *find_match_cols;
    int find_num_matches;
    int find_current_idx;
    int find_active;
    int match_bracket_x;
    int match_bracket_y;
    int has_match_bracket;
    int env;
    int quit_times;
    int save_times;
} editorConfig;

typedef struct {
    char *b;
    int len;
} appendBuffer;

typedef struct {
    char *buffer;
    int buf_len;
    int cursor_x;
    int preferred_x;
    int cursor_y;
    int select_mode;
    int select_sx;
    int select_sy;
    int select_ex;
    int select_ey;
} editorState;

typedef struct {
    editorState undo_stack[UNDO_REDO_STACK_SIZE];
    int undo_top;
    editorState redo_stack[UNDO_REDO_STACK_SIZE];
    int redo_top;
    long last_edit_time;
    int undo_in_progress;
} editorUndoRedo;

/*** Global Data ***/

editorConfig E;
editorUndoRedo history = {
    .undo_top = -1,
    .redo_top = -1,
    .last_edit_time = 0,
    .undo_in_progress = 0,
};

/*** Function Prototypes ***/

// utility
void clearTerminal();
int isWordChar(int);
long currentMillis();
char getClosingChar(char);
void clampCursorPosition();
void humanReadableSize(size_t, char *, size_t);

// memory
void *safeMalloc(size_t);
void *safeRealloc(void *, size_t);
char *safeStrdup(const char *);

// terminal
int getEnv();
void die(const char *);
void enableRawMode();
void disableRawMode();
int editorReadKey();
int getWindowSize(int *, int *);
int getCursorPosition(int *, int *);

// input
void editorProcessKeypress();
void editorMoveCursor(int);
char *editorPrompt(const char *, void (*)(char *, int), char *);
void editorMoveWordLeft();
void editorMoveWordRight();
void editorScrollPageUp(int);
void editorScrollPageDown(int);

// output
void editorDrawWelcomeMessage(appendBuffer *);
void editorRefreshScreen();
void editorDrawRows(appendBuffer *);
void editorScroll();
void editorDrawStatusBar(appendBuffer *);
void editorSetStatusMsg(const char *, ...);
void editorDrawMsgBar(appendBuffer *);
void editorManualScreen();

// editor
void editorInit();
void editorCleanup();

// append buffer
void abAppend(appendBuffer *, const char *, int);
void abFree(appendBuffer *);

// file i/o
void editorOpen(const char *);
char *editorRowsToString(int *);
void editorSave();
void editorQuit();

// row operations
void editorInsertRow(int, const char *, size_t);
void editorUpdateRow(editorRow *);
int editorRowCxToRx(const editorRow *, int);
int editorRowRxToCx(const editorRow *, int);
void editorRowInsertChar(editorRow *, int, int);
void editorRowDeleteChar(editorRow *, int);
void editorFreeRow(editorRow *);
void editorDeleteRow(int);
void editorRowAppendString(editorRow *, const char *, size_t);
void editorMoveRowUp();
void editorMoveRowDown();
void editorCopyRowUp();
void editorCopyRowDown();

// editor operations
void editorInsertChar(int);
void editorDeleteChar(int);
void editorInsertNewline();

// select operations
void editorSelectText(int);
void editorSelectAll();
char *editorGetSelectedText();
void editorDeleteSelectedText();

// find-replace operations
void editorFind();
void editorFindCallback(char *, int);
void editorScanLineMatches(int, const char *);
void editorReplace();
void editorReplaceCallback(char *, int);
int editorReplaceAll(const char *);
void editorReplaceJumpToCurrent();
int editorReplaceCurrent(const char *, const char *);

// clipboard operations
void clipboardCopyToSystem(const char *);
void editorCopySelection();
void editorCutSelection();
void editorCutLine();
void editorPaste();

// jump operations
void editorJump();
void editorJumpCallback(char *, int);

// undo-redo operations
void freeEditorState(editorState *);
void saveEditorStateForUndo();
void restoreEditorState(const editorState *);
void editorUndo();
void editorRedo();

// bracket highlighting
char getMatchingBracket(char);
int findMatchingBracketPosition(int, int, int *, int *);
void updateMatchBracket();

// mouse operations
void editorMouseLeftClick();
void editorMouseDragClick();
void editorMouseLeftRelease();

/*** Main ***/

int main(int argc, char *argv[]) {
    clearTerminal();
    enableRawMode();
    editorInit();
    if (argc >= 2)
        editorOpen(argv[1]);

    editorSetStatusMsg("HELP: Ctrl-H");
    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    clearTerminal();
    return 0;
}

/*** Function Definitions ***/

void clearTerminal() {
    system("clear");
}

int isWordChar(int ch) {
    return isalnum(ch) || ch == '_';
}

long currentMillis() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

char getClosingChar(char ch) {
    switch (ch) {
        case '(':  return ')';
        case '{':  return '}';
        case '[':  return ']';
        case '"':  return '"';
        case '\'': return '\'';
        case '`':  return '`';
    }
    return 0;
}

void clampCursorPosition() {
    if (E.num_rows == 0) {
        E.cursor_x = 0;
        E.cursor_y = 0;
        return;
    }

    if (E.cursor_y < 0)
        E.cursor_y = 0;
    else if (E.cursor_y >= E.num_rows) {
        E.cursor_y = E.num_rows - 1;
        E.cursor_x = E.row[E.cursor_y].size;
        return;
    }

    if (E.cursor_x < 0)
        E.cursor_x = 0;
    else if (E.cursor_x > E.row[E.cursor_y].size)
        E.cursor_x = E.row[E.cursor_y].size;
}

void humanReadableSize(size_t bytes, char *buf, size_t bufsize) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = bytes;
    while (size >= 1024 && unit < 4) {
        size /= 1024;
        unit++;
    }
    snprintf(buf, bufsize, "%.1f %s", size, units[unit]);
}

void *safeMalloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) die("malloc");
    return ptr;
}

void *safeRealloc(void *ptr, size_t size) {
    ptr = realloc(ptr, size);
    if (!ptr) die("realloc");
    return ptr;
}

char *safeStrdup(const char *str) {
    char *ptr = strdup(str);
    if (!ptr) die("strdup");
    return ptr;
}

int getEnv() {
    #if defined(__APPLE__)          // macOS
        return MACOS;
    #elif defined(__linux__)        // Linux or WSL
        FILE *test_pipe = popen("grep -i microsoft /proc/version", "r");
        int is_wsl = 0;
        if (test_pipe) {
            char buf[BUFFER_SIZE];
            is_wsl = fgets(buf, sizeof(buf), test_pipe) != NULL;
            pclose(test_pipe);
        }
        if (is_wsl)                 // WSL
            return WSL;
        else                        // Linux
            return LINUX;
    #else                           // Default Fallback
        return UNKNOWN;
    #endif
}

void die(const char *str) {
    write(STDOUT_FILENO, CLEAR_SCREEN CURSOR_RESET, sizeof(CLEAR_SCREEN CURSOR_RESET) - 1);

    perror(str);
    exit(1);
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.original_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);
    atexit(editorCleanup);

    struct termios raw = E.original_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);   // Input flags
    raw.c_oflag &= ~(OPOST);                                    // Output flags
    raw.c_cflag |= (CS8);                                       // Set char size to 8 bits/byte
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);            // Local flags
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    write(STDOUT_FILENO, ENABLE_MOUSE, sizeof(ENABLE_MOUSE) - 1);
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.original_termios) == -1)
        die("tcsetattr");
    write(STDOUT_FILENO, DISABLE_MOUSE, sizeof(DISABLE_MOUSE) - 1);
}

int editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
        if (nread == -1 && errno != EAGAIN)
            die("read");

    if (c == ESCAPE_CHAR) {
        char seq[SMALL_BUFFER_SIZE] = {0};
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return ESCAPE_CHAR;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return ESCAPE_CHAR;

        if (seq[0] == '[') {
            if (seq[1] == '<') {
                int i = 2;
                while (1) {
                    if (read(STDIN_FILENO, &seq[i], 1) != 1) return ESCAPE_CHAR;
                    if (seq[i] == 'm' || seq[i] == 'M') break;
                    i++;
                    if (i >= (int)sizeof(seq) - 1) return ESCAPE_CHAR;
                }
                seq[i + 1] = '\0';

                int b, x, y;
                if (sscanf(&seq[2], "%d;%d;%d", &b, &x, &y) == 3) {
                    if (b == 64) return MOUSE_SCROLL_UP;
                    else if (b == 65) return MOUSE_SCROLL_DOWN;

                    x--;
                    y--;
                    int motion = (b & SMALL_BUFFER_SIZE);
                    if ((b & 3) == 0) {
                        E.cursor_x = x + E.col_offset;
                        E.cursor_y = y + E.row_offset;
                        if (!motion && seq[i] == 'M') return MOUSE_LEFT_CLICK;
                        else if (motion && seq[i] == 'M') return MOUSE_DRAG;
                        else if (seq[i] == 'm') return MOUSE_LEFT_RELEASE;
                    }
                }
                return ESCAPE_CHAR;
            } else if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return ESCAPE_CHAR;
                if (seq[2] == ';') {
                    if (read(STDIN_FILENO, &seq[3], 1) != 1) return ESCAPE_CHAR;
                    if (read(STDIN_FILENO, &seq[4], 1) != 1) return ESCAPE_CHAR;
                    if (seq[3] == '6') {
                        switch (seq[4]) {
                            case 'C': return CTRL_SHIFT_ARROW_RIGHT;
                            case 'D': return CTRL_SHIFT_ARROW_LEFT;
                        }
                    }
                    if (seq[3] == '5') {
                        switch (seq[4]) {
                            case 'A': return CTRL_ARROW_UP;
                            case 'B': return CTRL_ARROW_DOWN;
                            case 'C': return CTRL_ARROW_RIGHT;
                            case 'D': return CTRL_ARROW_LEFT;
                        }
                    }
                    if (seq[3] == '4') {
                        switch (seq[4]) {
                            case 'A': return ALT_SHIFT_ARROW_UP;
                            case 'B': return ALT_SHIFT_ARROW_DOWN;
                        }
                    }
                    if (seq[3] == '3') {
                        switch (seq[4]) {
                            case 'A': return ALT_ARROW_UP;
                            case 'B': return ALT_ARROW_DOWN;
                        }
                    }
                    if (seq[3] == '2') {
                        switch (seq[4]) {
                            case 'A': return SHIFT_ARROW_UP;
                            case 'B': return SHIFT_ARROW_DOWN;
                            case 'C': return SHIFT_ARROW_RIGHT;
                            case 'D': return SHIFT_ARROW_LEFT;
                            case 'F': return SHIFT_END;
                            case 'H': return SHIFT_HOME;
                        }
                    }
                } else if (seq[2] == EMPTY_LINE_SYMBOL[0]) {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                    case 'M': {
                        char cb, cx, cy;
                        if (read(STDIN_FILENO, &cb, 1) != 1) return ESCAPE_CHAR;
                        if (read(STDIN_FILENO, &cx, 1) != 1) return ESCAPE_CHAR;
                        if (read(STDIN_FILENO, &cy, 1) != 1) return ESCAPE_CHAR;

                        int button = cb - SMALL_BUFFER_SIZE;
                        if (button == 64) return MOUSE_SCROLL_UP;
                        else if (button == 65) return MOUSE_SCROLL_DOWN;
                        return ESCAPE_CHAR;
                    }
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }

        return ESCAPE_CHAR;
    } else {
        return c;
    }
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    // fallback for calculating window size
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, CURSOR_FORWARD CURSOR_DOWN, sizeof(CURSOR_FORWARD CURSOR_DOWN) - 1) != sizeof(CURSOR_FORWARD CURSOR_DOWN) - 1) return -1;
        return getCursorPosition(rows, cols);
    } else {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
        return 0;
    }
}

int getCursorPosition(int *rows, int *cols) {
    char buf[SMALL_BUFFER_SIZE];
    unsigned int i = 0;
    if (write(STDOUT_FILENO, QUERY_CURSOR_POSITION, sizeof(QUERY_CURSOR_POSITION) - 1) != sizeof(QUERY_CURSOR_POSITION) - 1) return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != ESCAPE_CHAR || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
    return 0;
}

void editorProcessKeypress() {
    int ch = editorReadKey();
    if (ch != CTRL_KEY('q'))
        E.quit_times = QUIT_TIMES;

    switch (ch) {
        case CTRL_KEY('q'):     // quit
            editorQuit();
            break;

        case CTRL_KEY('h'):     // manual
            editorManualScreen();
            break;

        case CTRL_KEY('s'):     // save
            editorSave();
            break;

        case CTRL_KEY('f'):     // find
            editorFind();
            updateMatchBracket();
            break;

        case CTRL_KEY('r'):     // replace
            editorReplace();
            break;

        case CTRL_KEY('c'):     // copy
            editorCopySelection();
            break;

        case CTRL_KEY('x'):     // cut
            saveEditorStateForUndo();
            if (E.select_mode)
                editorCutSelection();
            else
                editorCutLine();
            updateMatchBracket();
            break;

        case CTRL_KEY('v'):     // paste
            editorPaste();
            updateMatchBracket();
            break;

        case CTRL_KEY('a'):     // select all
            editorSelectAll();
            updateMatchBracket();
            break;

        case CTRL_KEY('g'):     // jump to
        case CTRL_KEY('l'):
            editorJump();
            updateMatchBracket();
            break;

        case CTRL_KEY('z'):     // undo
            editorUndo();
            updateMatchBracket();
            break;

        case CTRL_KEY('y'):     // redo
            editorRedo();
            updateMatchBracket();
            break;

        case '\r':              // enter
            saveEditorStateForUndo();
            editorDeleteSelectedText();
            editorInsertNewline();
            updateMatchBracket();
            break;

        case '\t':              // tab
            saveEditorStateForUndo();
            editorInsertChar('\t');
            updateMatchBracket();
            break;

        case HOME_KEY:
            E.cursor_x = 0;
            E.preferred_x = E.cursor_x;
            E.select_mode = 0;
            updateMatchBracket();
            break;
        case END_KEY:
            E.select_mode = 0;
            if (E.cursor_y < E.num_rows) {
                E.cursor_x = E.row[E.cursor_y].size;
                E.preferred_x = E.cursor_x;
            }
            updateMatchBracket();
            break;

        case DEL_KEY:
            saveEditorStateForUndo();
            editorMoveCursor(ARROW_RIGHT);
            editorDeleteChar(0);
            updateMatchBracket();
            break;
        case BACKSPACE:
            saveEditorStateForUndo();
            editorDeleteChar(1);
            updateMatchBracket();
            break;

        case PAGE_UP:
            editorScrollPageUp(E.screen_rows);
            updateMatchBracket();
            break;
        case PAGE_DOWN:
            editorScrollPageDown(E.screen_rows);
            updateMatchBracket();
            break;

        case CTRL_ARROW_LEFT:
            editorMoveWordLeft();
            updateMatchBracket();
            break;
        case CTRL_ARROW_RIGHT:
            editorMoveWordRight();
            updateMatchBracket();
            break;
        case CTRL_ARROW_UP:
            editorScrollPageUp(1);
            updateMatchBracket();
            break;
        case CTRL_ARROW_DOWN:
            editorScrollPageDown(1);
            updateMatchBracket();
            break;

        case SHIFT_ARROW_LEFT:
        case SHIFT_ARROW_RIGHT:
        case SHIFT_ARROW_UP:
        case SHIFT_ARROW_DOWN:
            editorSelectText(ch);
            updateMatchBracket();
            break;

        case CTRL_SHIFT_ARROW_LEFT:
            if (!E.select_mode) {
                E.select_mode = 1;
                E.select_sx = E.cursor_x;
                E.select_sy = E.cursor_y;
            }
            editorMoveWordLeft();
            E.select_ex = E.cursor_x;
            E.select_ey = E.cursor_y;
            updateMatchBracket();
            break;
        case CTRL_SHIFT_ARROW_RIGHT:
            if (!E.select_mode) {
                E.select_mode = 1;
                E.select_sx = E.cursor_x;
                E.select_sy = E.cursor_y;
            }
            editorMoveWordRight();
            E.select_ex = E.cursor_x;
            E.select_ey = E.cursor_y;
            updateMatchBracket();
            break;

        case SHIFT_HOME:
            if (!E.select_mode) {
                E.select_mode = 1;
                E.select_sx = E.cursor_x;
                E.select_sy = E.cursor_y;
            }
            E.cursor_x = 0;
            E.select_ex = E.cursor_x;
            E.select_ey = E.cursor_y;
            updateMatchBracket();
            break;
        case SHIFT_END:
            if (!E.select_mode) {
                E.select_mode = 1;
                E.select_sx = E.cursor_x;
                E.select_sy = E.cursor_y;
            }
            if (E.cursor_y < E.num_rows)
                E.cursor_x = E.row[E.cursor_y].size;
            E.select_ex = E.cursor_x;
            E.select_ey = E.cursor_y;
            updateMatchBracket();
            break;

        case ARROW_LEFT:
        case ARROW_RIGHT:
        case ARROW_UP:
        case ARROW_DOWN:
            history.undo_in_progress = 0;
            E.select_mode = 0;
            editorMoveCursor(ch);
            updateMatchBracket();
            break;

        case ALT_ARROW_UP:
            saveEditorStateForUndo();
            editorMoveRowUp();
            break;
        case ALT_ARROW_DOWN:
            saveEditorStateForUndo();
            editorMoveRowDown();
            break;

        case ALT_SHIFT_ARROW_UP:
            saveEditorStateForUndo();
            editorCopyRowUp();
            break;
        case ALT_SHIFT_ARROW_DOWN:
            saveEditorStateForUndo();
            editorCopyRowDown();
            break;

        case MOUSE_SCROLL_UP:
            editorScrollPageUp(1);
            updateMatchBracket();
            break;
        case MOUSE_SCROLL_DOWN:
            editorScrollPageDown(1);
            updateMatchBracket();
            break;

        case MOUSE_LEFT_CLICK:
            editorMouseLeftClick();
            updateMatchBracket();
            break;
        case MOUSE_DRAG:
            editorMouseDragClick();
            updateMatchBracket();
            break;
        case MOUSE_LEFT_RELEASE:
            editorMouseLeftRelease();
            updateMatchBracket();
            break;

        case ESCAPE_CHAR:
            if (E.select_mode)
                E.select_mode = 0;
            break;

        default:
            if (!iscntrl(ch)) {
                saveEditorStateForUndo();
                if (E.select_mode) {
                    char closing = getClosingChar(ch);
                    if (closing != 0) {
                        char *selected = editorGetSelectedText();
                        if (selected) {
                            int selected_len = strlen(selected);

                            editorDeleteSelectedText();
                            editorInsertChar(ch);
                            for (int i = 0; i < selected_len; i++)
                                editorInsertChar(selected[i]);

                            free(selected);
                            updateMatchBracket();
                            break;
                        }
                    }
                }

                editorInsertChar(ch);
            }
            updateMatchBracket();
            break;
    }
}

void editorMoveCursor(int key) {
    editorRow *row = (E.cursor_y >= E.num_rows) ? NULL : &E.row[E.cursor_y];

    switch (key) {
        case ARROW_LEFT:
            if (E.cursor_x != 0) {
                E.cursor_x--;
            } else if (E.cursor_y > 0) {
                E.cursor_y--;
                E.cursor_x = E.row[E.cursor_y].size;
            }
            E.preferred_x = E.cursor_x;
            break;
        case ARROW_RIGHT:
            if (row && E.cursor_x < row->size) {
                E.cursor_x++;
            } else if (row && E.cursor_x == row->size && E.cursor_y < E.num_rows - 1) {
                E.cursor_y++;
                E.cursor_x = 0;
            }
            E.preferred_x = E.cursor_x;
            break;
        case ARROW_DOWN:
            if (E.cursor_y < E.num_rows - 1) {
                E.cursor_y++;
                row = (E.cursor_y >= E.num_rows) ? NULL : &E.row[E.cursor_y];
                if (E.preferred_x < 0) E.preferred_x = E.cursor_x;
                E.cursor_x = row && row->size > E.preferred_x ? E.preferred_x : (row ? row->size : 0);
            } else {
                E.cursor_x = E.row[E.cursor_y].size;
                E.preferred_x = E.row[E.cursor_y].size;
            }
            break;
        case ARROW_UP:
            if (E.cursor_y > 0) {
                E.cursor_y--;
                row = (E.cursor_y >= E.num_rows) ? NULL : &E.row[E.cursor_y];
                if (E.preferred_x < 0) E.preferred_x = E.cursor_x;
                E.cursor_x = row && row->size > E.preferred_x ? E.preferred_x : (row ? row->size : 0);
            } else {
                E.cursor_x = 0;
                E.preferred_x = 0;
            }
            break;
    }

    row = (E.cursor_y >= E.num_rows) ? NULL : &E.row[E.cursor_y];
    int row_len = row ? row->size : 0;
    if (E.cursor_x > row_len)
        E.cursor_x = row_len;
}

char *editorPrompt(const char *prompt, void (*callback)(char *, int), char *initial) {
    size_t buf_size = BUFFER_SIZE;
    char *buf = safeMalloc(buf_size);

    size_t buf_len = 0;
    buf[0] = '\0';

    if (initial) {
        buf_len = strlen(initial);
        if (buf_len >= buf_size) {
            buf_size = buf_len * 2;
            buf = safeRealloc(buf, buf_size);
        }
        strcpy(buf, initial);
        free(initial);
    }

    while (1) {
        editorSetStatusMsg(prompt, buf);
        editorRefreshScreen();

        int ch = editorReadKey();
        if (ch == DEL_KEY || ch == BACKSPACE) {
            if (buf_len != 0)
                buf[--buf_len] = '\0';
        } else if (ch == ESCAPE_CHAR) {
            editorSetStatusMsg("");
            if (callback)
                callback(buf, ch);
            free(buf);
            return NULL;
        } else if (ch == '\r') {
            if (buf_len != 0) {
                editorSetStatusMsg("");
                if (callback)
                    callback(buf, ch);
                return buf;
            }
        } else if (!iscntrl(ch) && ch < 128) {
            if (buf_len == buf_size - 1) {
                buf_size *= 2;
                buf = safeRealloc(buf, buf_size);
            }
            buf[buf_len++] = ch;
            buf[buf_len] = '\0';
        }

        if (callback)
            callback(buf, ch);
    }
}

void editorMoveWordLeft() {
    if (E.cursor_y >= E.num_rows) return;

    while (E.cursor_x > 0 && !isWordChar(E.row[E.cursor_y].chars[E.cursor_x - 1]))
        E.cursor_x--;
    while (E.cursor_x > 0 && isWordChar(E.row[E.cursor_y].chars[E.cursor_x - 1]))
        E.cursor_x--;
    E.preferred_x = E.cursor_x;
}

void editorMoveWordRight() {
    if (E.cursor_y >= E.num_rows) return;

    int len = E.row[E.cursor_y].size;
    while (E.cursor_x < len && !isWordChar(E.row[E.cursor_y].chars[E.cursor_x]))
        E.cursor_x++;
    while (E.cursor_x < len && isWordChar(E.row[E.cursor_y].chars[E.cursor_x]))
        E.cursor_x++;
    E.preferred_x = E.cursor_x;
}

void editorScrollPageUp(int scroll_amount) {
    if (E.row_offset > 0) {
        if (scroll_amount > E.row_offset)
            scroll_amount = E.row_offset;

        E.row_offset -= scroll_amount;
        E.cursor_y -= scroll_amount;

        if (E.cursor_y < 0)
            E.cursor_y = 0;

        if (E.cursor_y < E.num_rows) {
            int row_len = E.row[E.cursor_y].size;
            if (E.preferred_x > row_len)
                E.cursor_x = row_len;
            else
                E.cursor_x = E.preferred_x;
        }
    } else {
        if (E.cursor_y != 0)
            E.cursor_y = 0;
        else {
            E.cursor_x = 0;
            E.preferred_x = 0;
        }
    }
}

void editorScrollPageDown(int scroll_amount) {
    if (E.row_offset < E.num_rows - E.screen_rows) {
        E.row_offset += scroll_amount;
        E.cursor_y += scroll_amount;

        if (E.cursor_y >= E.num_rows)
            E.cursor_y = E.num_rows - 1;

        if (E.cursor_y < E.num_rows) {
            int row_len = E.row[E.cursor_y].size;
            if (E.preferred_x > row_len)
                E.cursor_x = row_len;
            else
                E.cursor_x = E.preferred_x;
        }
    } else {
        if (E.cursor_y != E.num_rows - 1)
            E.cursor_y = E.num_rows - 1;
        else {
            E.cursor_x = E.row[E.cursor_y].size;
            E.preferred_x = E.row[E.cursor_y].size;
        }
    }
}

void editorDrawWelcomeMessage(appendBuffer *ab) {
    char welcome[STATUS_LENGTH];
    int welcome_len = snprintf(welcome, sizeof(welcome), "Cypher Version %s", CYPHER_VERSION);
    if (welcome_len > E.screen_cols) welcome_len = E.screen_cols;

    int padding = (E.screen_cols - welcome_len) / 2;
    int vertical_center = E.screen_rows / 3;

    for (int y = 0; y < E.screen_rows; y++) {
        if (y == vertical_center) {
            abAppend(ab, EMPTY_LINE_SYMBOL, sizeof(EMPTY_LINE_SYMBOL) - 1);
            for (int i = 1; i < padding; i++)
                abAppend(ab, " ", 1);
            abAppend(ab, welcome, welcome_len);
        }
        else
            abAppend(ab, EMPTY_LINE_SYMBOL, sizeof(EMPTY_LINE_SYMBOL) - 1);
        abAppend(ab, CLEAR_LINE NEW_LINE, sizeof(CLEAR_LINE NEW_LINE) - 1);
    }
}

void editorRefreshScreen() {
    editorScroll();

    appendBuffer ab = APPEND_BUFFER_INIT;
    abAppend(&ab, HIDE_CURSOR CURSOR_RESET, sizeof(HIDE_CURSOR CURSOR_RESET) - 1);

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMsgBar(&ab);

    char buf[SMALL_BUFFER_SIZE];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cursor_y - E.row_offset) + 1, (E.render_x - E.col_offset) + 1);
    abAppend(&ab, buf, strlen(buf));
    abAppend(&ab, SHOW_CURSOR, sizeof(SHOW_CURSOR) - 1);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

void editorDrawRows(appendBuffer *ab) {
    if (E.num_rows == 0) {
        editorDrawWelcomeMessage(ab);
        return;
    }

    for (int y = 0; y < E.screen_rows; y++) {
        int file_row = y + E.row_offset;
        if (file_row >= E.num_rows) {
            abAppend(ab, EMPTY_LINE_SYMBOL, sizeof(EMPTY_LINE_SYMBOL) - 1);
        } else {
            int len = E.row[file_row].rsize - E.col_offset;
            if (len < 0) len = 0;
            if (len > E.screen_cols) len = E.screen_cols;

            for (int j = 0; j < len; j++) {
                int cx = editorRowRxToCx(&E.row[file_row], j + E.col_offset);
                int is_sel = 0;
                if (E.select_mode) {
                    int y1 = E.select_sy, x1 = E.select_sx;
                    int y2 = E.select_ey, x2 = E.select_ex;
                    if (y1 > y2 || (y1 == y2 && x1 > x2)) {
                        int tmpy = y1, tmpx = x1;
                        y1 = y2;
                        x1 = x2;
                        y2 = tmpy;
                        x2 = tmpx;
                    }
                    if (file_row > y1 && file_row < y2) is_sel = 1;
                    else if (file_row == y1 && file_row == y2 && cx >= x1 && cx < x2) is_sel = 1;
                    else if (file_row == y1 && file_row != y2 && cx >= x1) is_sel = 1;
                    else if (file_row == y2 && file_row != y1 && cx < x2) is_sel = 1;
                }

                int is_find = 0;
                int is_current_match = 0;
                if (E.find_active && E.find_query) {
                    int match_len = strlen(E.find_query);
                    for (int m = 0; m < E.find_num_matches; m++) {
                        if (E.find_match_lines[m] == file_row) {
                            int col_start = E.find_match_cols[m];
                            int col_end = col_start + match_len;
                            if (cx >= col_start && cx < col_end) {
                                is_find = 1;
                                if (m == E.find_current_idx)
                                    is_current_match = 1;
                                break;
                            }
                        }
                    }
                }

                if (is_sel && !is_find) abAppend(ab, LIGHT_GRAY_BG_COLOR, sizeof(LIGHT_GRAY_BG_COLOR) - 1);
                else if (is_current_match) abAppend(ab, YELLOW_FG_COLOR, sizeof(YELLOW_FG_COLOR) - 1);
                else if (is_find) abAppend(ab, LIGHT_GRAY_BG_COLOR, sizeof(LIGHT_GRAY_BG_COLOR) - 1);

                if (E.has_match_bracket && !E.select_mode && !E.find_active) {
                    int start_row = E.cursor_y;
                    int start_col = E.cursor_x;
                    int end_row = E.match_bracket_y;
                    int end_col = E.match_bracket_x;

                    if (start_row > end_row || (start_row == end_row && start_col > end_col)) {
                        int tmp_r = start_row, tmp_c = start_col;
                        start_row = end_row;
                        start_col = end_col;
                        end_row = tmp_r;
                        end_col = tmp_c;
                    }

                    if (file_row >= start_row && file_row <= end_row) {
                        int highlight_start_col = 0;
                        int highlight_end_col = E.row[file_row].size;

                        if (file_row == start_row)
                            highlight_start_col = start_col;
                        if (file_row == end_row)
                            highlight_end_col = end_col + 1;

                        if (cx >= highlight_start_col && cx < highlight_end_col) {
                            abAppend(ab, DARK_GRAY_BG_COLOR, sizeof(DARK_GRAY_BG_COLOR) - 1);
                            abAppend(ab, &E.row[file_row].render[j + E.col_offset], 1);
                            abAppend(ab, REMOVE_GRAPHICS, sizeof(REMOVE_GRAPHICS) - 1);
                        } else {
                            abAppend(ab, &E.row[file_row].render[j + E.col_offset], 1);
                        }
                    } else {
                        abAppend(ab, &E.row[file_row].render[j + E.col_offset], 1);
                    }
                } else {
                    abAppend(ab, &E.row[file_row].render[j + E.col_offset], 1);
                }

                if (is_sel || is_find || is_current_match) abAppend(ab, REMOVE_GRAPHICS, sizeof(REMOVE_GRAPHICS) - 1);
            }
        }
        abAppend(ab, CLEAR_LINE NEW_LINE, sizeof(CLEAR_LINE NEW_LINE) - 1);
    }
}

void editorScroll() {
    E.render_x = 0;
    if (E.cursor_y < E.num_rows)
        E.render_x = editorRowCxToRx(&E.row[E.cursor_y], E.cursor_x);

    if (E.cursor_y < E.row_offset)
        E.row_offset = E.cursor_y;
    if (E.cursor_y >= E.row_offset + E.screen_rows)
        E.row_offset = E.cursor_y - E.screen_rows + 1;
    if (E.render_x < E.col_offset)
        E.col_offset = E.render_x;
    if (E.render_x >= E.col_offset + E.screen_cols)
        E.col_offset = E.render_x - E.screen_cols + 1;
}

void editorDrawStatusBar(appendBuffer *ab) {
    abAppend(ab, INVERTED_COLORS, sizeof(INVERTED_COLORS) - 1);
    char status[STATUS_LENGTH], rstatus[STATUS_LENGTH];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s", E.filename ? E.filename : "[No Name]", E.num_rows, E.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d:%d", E.cursor_y + 1, E.cursor_x + 1);
    if (len > E.screen_cols)
        len = E.screen_cols;
    abAppend(ab, status, len);

    while (len < E.screen_cols) {
        if (E.screen_cols - len == rlen) {
            abAppend(ab, rstatus, rlen);
            break;
        } else {
            abAppend(ab, " ", 1);
            len++;
        }
    }
    abAppend(ab, REMOVE_GRAPHICS NEW_LINE, sizeof(REMOVE_GRAPHICS NEW_LINE) - 1);
}

void editorSetStatusMsg(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.status_msg, sizeof(E.status_msg), fmt, ap);
    va_end(ap);
    E.status_msg_time = time(NULL);
}

void editorDrawMsgBar(appendBuffer *ab) {
    abAppend(ab, CLEAR_LINE, sizeof(CLEAR_LINE));

    int msg_len = strlen(E.status_msg);
    if (msg_len > E.screen_cols)
        msg_len = E.screen_cols;
    if (msg_len && time(NULL) - E.status_msg_time < STATUS_MSG_TIMEOUT_SEC)
        abAppend(ab, E.status_msg, msg_len);

    if (E.find_active) {
        char buf[SMALL_BUFFER_SIZE];
        snprintf(buf, sizeof(buf), " %d/%d", E.find_current_idx + 1, E.find_num_matches);
        int right_len = strlen(buf);

        while (msg_len + right_len < E.screen_cols) {
            abAppend(ab, " ", 1);
            msg_len++;
        }
        abAppend(ab, buf, right_len);
    }
}

void editorManualScreen() {
    write(STDOUT_FILENO, CLEAR_SCREEN CURSOR_RESET HIDE_CURSOR, sizeof(CLEAR_SCREEN CURSOR_RESET HIDE_CURSOR) - 1);

    char *text[] = {
        "CYPHER Editor Manual",
        "",
        "Keyboard Shortcuts:",
        "  Ctrl-S               - Save",
        "  Ctrl-Q               - Quit",
        "  Ctrl-F               - Find",
        "  Ctrl-R               - Find & Replace",
        "  Ctrl-G / L           - Jump to line",
        "  Ctrl-A               - Select all",
        "  Ctrl-Z               - Undo last major change",
        "  Ctrl-Y               - Redo last major change",
        "  Ctrl-C               - Copy selected text",
        "  Ctrl-X               - Cut selected text",
        "  Ctrl-V               - Paste from clipboard",
        "  Ctrl-H               - Show manual",
        "  Alt-Up/Down          - Move row up / down",
        "  Shift-Alt-Up/Down    - Copy row up / down",
        "",
        "Press any key to return..."
    };

    int lines = sizeof(text) / sizeof(text[0]);
    int row = 1;
    for (int i = 0; i < lines; i++) {
        char buf[BUFFER_SIZE];
        int len = snprintf(buf, sizeof(buf), "\x1b[%d;1H%s", row++, text[i]);
        write(STDOUT_FILENO, buf, len);
    }

    editorReadKey();
    write(STDOUT_FILENO, CLEAR_SCREEN CURSOR_RESET SHOW_CURSOR, sizeof(CLEAR_SCREEN CURSOR_RESET SHOW_CURSOR) - 1);
}

void editorInit() {
    E.cursor_x = 0;
    E.cursor_y = 0;
    E.render_x = 0;
    E.preferred_x = 0;
    E.row_offset = 0;
    E.col_offset = 0;
    E.num_rows = 0;
    E.row = NULL;
    E.dirty = 0;
    E.filename = NULL;
    E.status_msg[0] = '\0';
    E.status_msg_time = 0;
    E.clipboard = NULL;
    E.is_pasting = 0;
    E.select_mode = 0;
    E.select_sx = 0;
    E.select_sy = 0;
    E.select_ex = 0;
    E.select_ey = 0;
    E.find_query = NULL;
    E.find_match_lines = NULL;
    E.find_match_cols = NULL;
    E.find_num_matches = 0;
    E.find_current_idx = -1;
    E.find_active = 0;
    E.match_bracket_x = 0;
    E.match_bracket_y = 0;
    E.has_match_bracket = 0;
    E.env = getEnv();
    E.quit_times = QUIT_TIMES;
    E.save_times = SAVE_TIMES;

    if (getWindowSize(&E.screen_rows, &E.screen_cols) == -1) die("getWindowSize");
    E.screen_rows -= 2;
}

void editorCleanup() {
    if (E.row != NULL) {
        for (int i = 0; i < E.num_rows; i++)
            editorFreeRow(&E.row[i]);
        free(E.row);
        E.row = NULL;
    }
    free(E.filename);
    E.filename = NULL;

    free(E.clipboard);
    E.clipboard = NULL;

    free(E.find_query);
    free(E.find_match_lines);
    free(E.find_match_cols);
    E.find_query = NULL;
    E.find_match_lines = NULL;
    E.find_match_cols = NULL;
    E.find_num_matches = 0;
    E.find_current_idx = -1;
    E.find_active = 0;
}

void abAppend(appendBuffer *ab, const char *str, int len) {
    char *new = safeRealloc(ab->b, ab->len + len);

    memcpy(&new[ab->len], str, len);
    ab->b = new;
    ab->len += len;
}

void abFree(appendBuffer *ab) {
    free(ab->b);
}

void editorOpen(const char *filename) {
    free(E.filename);
    E.filename = safeStrdup(filename);

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        if (errno == ENOENT) {
            E.num_rows = 0;
            E.row = NULL;
            E.dirty = 0;
            return;
        }
        die("fopen");
    }

    char *line = NULL;
    size_t line_cap = 0;
    ssize_t line_len;
    while ((line_len = getline(&line, &line_cap, fp)) != -1) {
        while (line_len > 0 && (line[line_len - 1] == '\n' || line[line_len - 1] == '\r'))
            line_len--;
        editorInsertRow(E.num_rows, line, line_len);
    }
    free(line);
    fclose(fp);
    E.dirty = 0;
}

char *editorRowsToString(int *buf_len) {
    int total_len = 0;
    for (int i = 0; i < E.num_rows; i++) {
        total_len += E.row[i].size;
        if (i < E.num_rows - 1 || E.row[i].size > 0)
            total_len += 1;
    }
    *buf_len = total_len;

    char *buf = safeMalloc(total_len);
    char *ptr = buf;
    for (int i = 0; i < E.num_rows; i++) {
        memcpy(ptr, E.row[i].chars, E.row[i].size);
        ptr += E.row[i].size;
        if (i < E.num_rows - 1 || E.row[i].size > 0) {
            *ptr = '\n';
            ptr++;
        }
    }

    return buf;
}

void editorSave() {
    static int new_file = 0;
    if (E.filename == NULL) {
        char *input = editorPrompt("Save as: %s (ESC to cancel)", NULL, NULL);
        if (input == NULL) {
            editorSetStatusMsg("Save aborted");
            return;
        }

        if (strchr(input, '.') == NULL) {
            size_t len = strlen(input);
            char *new_name = safeMalloc(len + 5);

            strcpy(new_name, input);
            strcat(new_name, ".txt");
            free(input);
            input = new_name;
        }
        E.filename = input;
        new_file = 1;
    }

    if (new_file && access(E.filename, F_OK) == 0 && E.save_times != 0) {
        editorSetStatusMsg("File exists! Press Ctrl-S %d more time%s to overwrite.", E.save_times, E.save_times == 1 ? "" : "s");
        E.save_times--;
        return;
    }
    E.save_times = SAVE_TIMES;
    new_file = 0;

    int len;
    char *buf = editorRowsToString(&len);

    int fp = open(E.filename,
                  O_RDWR |      // read and write
                  O_CREAT,      // create if doesn't exist
                  0644);        // permissions
    if (fp != -1) {
        if (ftruncate(fp, len) != -1) {
            if (write(fp, buf, len) == len) {
                close(fp);
                free(buf);
                E.dirty = 0;
                char sizebuf[SMALL_BUFFER_SIZE];
                humanReadableSize(len, sizebuf, sizeof(sizebuf));
                editorSetStatusMsg("%s written to disk", sizebuf);
                E.quit_times = QUIT_TIMES;
                return;
            }
        }
        close(fp);
    }

    free(buf);
    editorSetStatusMsg("Can't save! I/O error: %s", strerror(errno));
}

void editorQuit() {
    if (E.dirty && E.quit_times > 0) {
        editorSetStatusMsg("WARNING!!! File has unsaved changes. Press Ctrl-Q %d more time%s to quit.", E.quit_times, E.quit_times == 1 ? "" : "s");
        E.quit_times--;
        return;
    }
    write(STDOUT_FILENO, CLEAR_SCREEN CURSOR_RESET, sizeof(CLEAR_SCREEN CURSOR_RESET) - 1);
    clearTerminal();
    exit(0);
}

void editorInsertRow(int at, const char *str, size_t len) {
    if (at < 0 || at > E.num_rows) return;

    E.row = safeRealloc(E.row, sizeof(editorRow) * (E.num_rows + 1));
    memmove(&E.row[at + 1], &E.row[at], sizeof(editorRow) * (E.num_rows - at));

    E.row[at].size = len;
    E.row[at].chars = safeMalloc(len + 1);

    memcpy(E.row[at].chars, str, len);
    E.row[at].chars[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    editorUpdateRow(&E.row[at]);

    E.num_rows++;
    E.dirty++;
}

void editorUpdateRow(editorRow *row) {
    int tabs = 0;
    for (int i = 0; i < row->size; i++)
        if (row->chars[i] == '\t')
            tabs++;
    free(row->render);
    row->render = safeMalloc(row->size + tabs * (TAB_SIZE - 1) + 1);

    int idx = 0;
    for (int i = 0; i < row->size; i++) {
        if (row->chars[i] == '\t') {
            row->render[idx++] = ' ';
            while (idx % TAB_SIZE != 0)
                row->render[idx++] = ' ';
        }
        else
            row->render[idx++] = row->chars[i];
    }

    row->render[idx] = '\0';
    row->rsize = idx;
}

int editorRowCxToRx(const editorRow *row, int cursor_x) {
    int render_x = 0;
    for (int i = 0; i < cursor_x; i++) {
        if (row->chars[i] == '\t')
            render_x += (TAB_SIZE - 1) - (render_x % TAB_SIZE);
        render_x++;
    }

    return render_x;
}

int editorRowRxToCx(const editorRow *row, int render_x) {
    int cur_render_x = 0;
    int cursor_x;
    for (cursor_x = 0; cursor_x < row->size; cursor_x++) {
        if (row->chars[cursor_x] == '\t')
            cur_render_x += (TAB_SIZE - 1) - (cur_render_x % TAB_SIZE);
        cur_render_x++;

        if (cur_render_x > render_x) return cursor_x;
    }

    return cursor_x;
}

void editorRowInsertChar(editorRow *row, int at, int ch) {
    if (at < 0 || at > row->size)
        at = row->size;

    row->chars = safeRealloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = ch;
    editorUpdateRow(row);
    E.dirty++;
}

void editorRowDeleteChar(editorRow *row, int at) {
    if (at < 0 || at >= row->size) return;

    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editorUpdateRow(row);
    E.dirty++;
}

void editorFreeRow(editorRow *row) {
    free(row->chars);
    free(row->render);
}

void editorDeleteRow(int at) {
    if (at < 0 || at >= E.num_rows) return;

    editorFreeRow(&E.row[at]);
    memmove(&E.row[at], &E.row[at + 1], sizeof(editorRow) * (E.num_rows - at - 1));
    E.num_rows--;
    E.dirty++;
}

void editorRowAppendString(editorRow *row, const char *str, size_t len) {
    row->chars = safeRealloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], str, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    E.dirty++;
}

void editorMoveRowUp() {
    if (E.cursor_y <= 0 || E.cursor_y >= E.num_rows) return;

    editorRow temp = E.row[E.cursor_y - 1];
    E.row[E.cursor_y - 1] = E.row[E.cursor_y];
    E.row[E.cursor_y] = temp;

    E.cursor_y--;
    E.dirty++;
}

void editorMoveRowDown() {
    if (E.cursor_y < 0 || E.cursor_y >= E.num_rows - 1) return;

    editorRow temp = E.row[E.cursor_y + 1];
    E.row[E.cursor_y + 1] = E.row[E.cursor_y];
    E.row[E.cursor_y] = temp;

    E.cursor_y++;
    E.dirty++;
}

void editorCopyRowUp() {
    if (E.cursor_y >= E.num_rows) return;

    editorRow *row = &E.row[E.cursor_y];
    editorInsertRow(E.cursor_y, row->chars, row->size);
    E.dirty++;
}

void editorCopyRowDown() {
    if (E.cursor_y >= E.num_rows) return;

    editorRow *row = &E.row[E.cursor_y];
    editorInsertRow(++E.cursor_y, row->chars, row->size);
    E.dirty++;
}

void editorInsertChar(int ch) {
    if (E.select_mode) {
        saveEditorStateForUndo();
        editorDeleteSelectedText();
        E.select_mode = 0;
    }

    if (E.cursor_y == E.num_rows)
        editorInsertRow(E.num_rows, "", 0);
    editorRowInsertChar(&E.row[E.cursor_y], E.cursor_x, ch);
    E.cursor_x++;

    char closing_char = getClosingChar(ch);
    if (!E.is_pasting && closing_char)
        editorRowInsertChar(&E.row[E.cursor_y], E.cursor_x, closing_char);
    E.preferred_x = E.cursor_x;
    E.dirty++;
}

void editorDeleteChar(int is_backspace) {
    if (E.select_mode) {
        editorDeleteSelectedText();
        return;
    }

    if (E.cursor_y == E.num_rows) return;
    if (E.cursor_x == 0 && E.cursor_y == 0) return;

    editorRow *row = &E.row[E.cursor_y];
    if (E.cursor_x > 0) {
        char prev_char = row->chars[E.cursor_x - 1];
        char next_char = (E.cursor_x < row->size) ? row->chars[E.cursor_x] : '\0';
        char closing_char = getClosingChar(prev_char);

        if (is_backspace) {
            int spaces = 0;
            while (spaces < TAB_SIZE && (E.cursor_x - spaces - 1) >= 0 && row->chars[E.cursor_x - spaces - 1] == ' ')
                spaces++;

            int only_spaces = 1;
            for (int i = 0; i < E.cursor_x; i++) {
                if (row->chars[i] != ' ') {
                    only_spaces = 0;
                    break;
                }
            }

            if (spaces > 0 && (E.cursor_x % TAB_SIZE == 0) && only_spaces) {
                for (int i = 0; i < spaces; i++)
                    editorRowDeleteChar(row, --E.cursor_x);
                E.preferred_x = E.cursor_x;
                E.dirty++;
                return;
            }
        }

        if (is_backspace && closing_char && next_char == closing_char) {
            editorRowDeleteChar(row, E.cursor_x);
            editorRowDeleteChar(row, E.cursor_x - 1);
            E.cursor_x--;
            E.preferred_x = E.cursor_x;
            E.dirty++;
            return;
        }

        editorRowDeleteChar(row, --E.cursor_x);
    } else {
        E.cursor_x = E.row[E.cursor_y - 1].size;
        editorRowAppendString(&E.row[E.cursor_y - 1], row->chars, row->size);
        editorDeleteRow(E.cursor_y--);
    }
    E.preferred_x = E.cursor_x;
    E.dirty++;
}

void editorInsertNewline() {
    editorRow *row = (E.cursor_y >= E.num_rows) ? NULL : &E.row[E.cursor_y];
    int indent_len = 0;
    while (indent_len < row->size && (row->chars[indent_len] == ' ' || row->chars[indent_len] == '\t'))
        indent_len++;
    if (indent_len > E.cursor_x)
        indent_len = E.cursor_x;
    if (E.is_pasting)
        indent_len = 0;

    char *indent_str = safeMalloc(indent_len + 1);
    memcpy(indent_str, row->chars, indent_len);
    indent_str[indent_len] = '\0';

    if (E.cursor_x == 0)
        editorInsertRow(E.cursor_y, indent_str, indent_len);
    else {
        editorInsertRow(E.cursor_y + 1, &row->chars[E.cursor_x], row->size - E.cursor_x);
        row = &E.row[E.cursor_y];
        row->size = E.cursor_x;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);

        editorRow *new_row = &E.row[E.cursor_y + 1];
        new_row->chars = safeRealloc(new_row->chars, new_row->size + indent_len + 1);

        memmove(new_row->chars + indent_len, new_row->chars, new_row->size + 1);
        memcpy(new_row->chars, indent_str, indent_len);
        new_row->size += indent_len;
        editorUpdateRow(new_row);

        if (E.cursor_x > 0 && (row->chars[E.cursor_x - 1] == '{' || row->chars[E.cursor_x - 1] == '[' || row->chars[E.cursor_x - 1] == '(') && new_row->size > 0 && new_row->chars[indent_len] == getClosingChar(row->chars[E.cursor_x - 1])) {
            int new_indent_len = indent_len + TAB_SIZE;
            char *block_indent = safeMalloc(new_indent_len + 1);
            memset(block_indent, ' ', new_indent_len);
            block_indent[new_indent_len] = '\0';
            editorInsertRow(E.cursor_y + 1, block_indent, new_indent_len);
            free(block_indent);

            E.cursor_y++;
            E.cursor_x = new_indent_len;
            E.preferred_x = E.cursor_x;

            free(indent_str);
            return;
        }
    }

    free(indent_str);
    E.cursor_y++;
    E.cursor_x = indent_len;
    E.preferred_x = E.cursor_x;
}

void editorSelectText(int ch) {
    if (!E.select_mode) {
        E.select_mode = 1;
        E.select_sx = E.cursor_x;
        E.select_sy = E.cursor_y;
    }
    editorMoveCursor(ch == SHIFT_ARROW_LEFT ? ARROW_LEFT : ch == SHIFT_ARROW_RIGHT ? ARROW_RIGHT : ch == SHIFT_ARROW_UP ? ARROW_UP : ARROW_DOWN);
    E.select_ex = E.cursor_x;
    E.select_ey = E.cursor_y;
}

void editorSelectAll() {
    if (E.num_rows > 0) {
        E.select_mode = 1;
        E.select_sx = 0;
        E.select_sy = 0;
        E.select_ex = E.row[E.num_rows - 1].size;
        E.select_ey = E.num_rows - 1;

        E.cursor_x = E.select_ex;
        E.cursor_y = E.select_ey;

        editorSetStatusMsg("Selected all %d lines", E.num_rows);
    }
}

char *editorGetSelectedText() {
    if (!E.select_mode) return NULL;

    int x1 = E.select_sx, y1 = E.select_sy;
    int x2 = E.select_ex, y2 = E.select_ey;
    if (y1 > y2 || (y1 == y2 && x1 > x2)) {
        int tmpx = x1, tmpy = y1;
        x1 = x2;
        y1 = y2;
        x2 = tmpx;
        y2 = tmpy;
    }

    size_t total = 0;
    for (int row = y1; row <= y2; row++) {
        int startx = (row == y1) ? x1 : 0;
        int endx = (row == y2) ? x2 : E.row[row].size;
        if (endx > startx)
            total += (endx - startx);
        if (row != y2)
            total++;
    }

    char *buf = safeMalloc(total + 1);
    char *ptr = buf;
    for (int row = y1; row <= y2; row++) {
        int startx = (row == y1) ? x1 : 0;
        int endx = (row == y2) ? x2 : E.row[row].size;
        if (endx > startx) {
            memcpy(ptr, &E.row[row].chars[startx], endx - startx);
            ptr += (endx - startx);
        }
        if (row != y2)
            *ptr++ = '\n';
    }
    *ptr = '\0';
    return buf;
}

void editorDeleteSelectedText() {
    if (!E.select_mode)
        return;

    int y1 = E.select_sy, x1 = E.select_sx;
    int y2 = E.select_ey, x2 = E.select_ex;
    if (y1 > y2 || (y1 == y2 && x1 > x2)) {
        int tmpx = x1, tmpy = y1;
        x1 = x2;
        y1 = y2;
        x2 = tmpx;
        y2 = tmpy;
    }

    if (y1 == y2) {
        memmove(&E.row[y1].chars[x1], &E.row[y1].chars[x2], E.row[y1].size - x2);
        E.row[y1].size -= (x2 - x1);
        E.row[y1].chars[E.row[y1].size] = '\0';
        editorUpdateRow(&E.row[y1]);
    } else {
        E.row[y1].size = x1;
        E.row[y1].chars[x1] = '\0';
        editorUpdateRow(&E.row[y1]);

        memmove(E.row[y2].chars, &E.row[y2].chars[x2], E.row[y2].size - x2);
        E.row[y2].size -= x2;
        E.row[y2].chars[E.row[y2].size] = '\0';
        editorUpdateRow(&E.row[y2]);

        editorRowAppendString(&E.row[y1], E.row[y2].chars, E.row[y2].size);
        editorDeleteRow(y2);

        for (int row = y2 - 1; row > y1; row--)
            editorDeleteRow(row);
    }

    E.cursor_x = x1;
    E.cursor_y = y1;
    E.preferred_x = E.cursor_x;

    E.select_mode = 0;
    E.dirty++;
}

void editorFind() {
    E.has_match_bracket = 0;
    int saved_cursor_x = E.cursor_x;
    int saved_cursor_y = E.cursor_y;
    int saved_col_offset = E.col_offset;
    int saved_row_offset = E.row_offset;

    char *query = editorPrompt("Search: %s (Use ESC/Arrows/Enter)", editorFindCallback, editorGetSelectedText());

    if (query) free(query);
    else {
        E.cursor_x = saved_cursor_x;
        E.preferred_x = E.cursor_x;
        E.cursor_y = saved_cursor_y;
        E.col_offset = saved_col_offset;
        E.row_offset = saved_row_offset;
        E.find_active = 0;
    }
}

void editorFindCallback(char *query, int key) {
    int direction = 1;

    if (key == '\r' || key == ESCAPE_CHAR || query[0] == '\0') {
        if (key == ESCAPE_CHAR)
            editorSetStatusMsg("Find cancelled");
        E.find_active = 0;
        free(E.find_query);
        E.find_query = NULL;
        free(E.find_match_lines);
        E.find_match_lines = NULL;
        free(E.find_match_cols);
        E.find_match_cols = NULL;
        E.find_num_matches = 0;
        E.find_current_idx = -1;
        return;
    }

    if (key == ARROW_RIGHT || key == ARROW_DOWN) direction = 1;
    else if (key == ARROW_LEFT || key == ARROW_UP) direction = -1;
    else if (key == BACKSPACE) direction = 1;
    else if (iscntrl(key)) return;
    else direction = 1;

    if (E.find_query == NULL || strcmp(E.find_query, query) != 0) {
        free(E.find_query);
        E.find_query = safeStrdup(query);

        free(E.find_match_lines);
        free(E.find_match_cols);
        E.find_match_lines = NULL;
        E.find_match_cols = NULL;
        E.find_num_matches = 0;
        E.find_current_idx = -1;

        for (int i = 0; i < E.num_rows; i++) {
            char *line = E.row[i].chars;
            char *ptr = line;
            while ((ptr = strstr(ptr, query)) != NULL) {
                E.find_match_lines = safeRealloc(E.find_match_lines, sizeof(int) * (E.find_num_matches + 1));
                E.find_match_cols = safeRealloc(E.find_match_cols, sizeof(int) * (E.find_num_matches + 1));

                E.find_match_lines[E.find_num_matches] = i;
                E.find_match_cols[E.find_num_matches] = ptr - line;
                E.find_num_matches++;

                ptr += strlen(query);
            }
        }

        if (E.find_num_matches > 0) {
            E.find_current_idx = 0;
            int row = E.find_match_lines[0];
            int col = E.find_match_cols[0];
            E.cursor_y = row;
            E.cursor_x = col;
            E.row_offset = E.num_rows;
            E.preferred_x = E.cursor_x;

            if (row < E.row_offset)
                E.row_offset = row;
            else if (row >= E.row_offset + E.screen_rows)
                E.row_offset = row - E.screen_rows + MARGIN;

            int render_pos = editorRowCxToRx(&E.row[row], E.cursor_x);
            int margin = strlen(query) + MARGIN;
            if (render_pos < E.col_offset)
                E.col_offset = render_pos;
            else if (render_pos >= E.col_offset + E.screen_cols - 1 && render_pos > margin)
                E.col_offset = render_pos - (E.screen_cols - margin);
            if (E.col_offset < 0) E.col_offset = 0;
        }
        E.find_active = 1;
    } else {
        if (E.find_num_matches > 0) {
            E.find_current_idx += direction;
            if (E.find_current_idx < 0) E.find_current_idx = E.find_num_matches - 1;
            else if (E.find_current_idx >= E.find_num_matches) E.find_current_idx = 0;

            int row = E.find_match_lines[E.find_current_idx];
            int col = E.find_match_cols[E.find_current_idx];
            E.cursor_y = row;
            E.cursor_x = col;
            E.preferred_x = E.cursor_x;

            if (row < E.row_offset)
                E.row_offset = row;
            else if (row >= E.row_offset + E.screen_rows)
                E.row_offset = row - E.screen_rows + MARGIN;

            int render_pos = editorRowCxToRx(&E.row[row], E.cursor_x);
            int margin = strlen(query) + MARGIN;
            if (render_pos < E.col_offset)
                E.col_offset = render_pos;
            else if (render_pos >= E.col_offset + E.screen_cols - 1 && render_pos > margin)
                E.col_offset = render_pos - (E.screen_cols - margin);
            if (E.col_offset < 0) E.col_offset = 0;
        }
    }
}

void editorScanLineMatches(int line, const char *query) {
    int new_num_matches = 0;

    int max_matches = E.find_num_matches + 50;
    int *new_lines = safeMalloc(sizeof(int) * max_matches);
    int *new_cols = safeMalloc(sizeof(int) * max_matches);

    for (int i = 0; i < E.find_num_matches; i++) {
        if (E.find_match_lines[i] != line) {
            new_lines[new_num_matches] = E.find_match_lines[i];
            new_cols[new_num_matches] = E.find_match_cols[i];
            new_num_matches++;
        }
    }

    if (line < E.num_rows) {
        char *render_line = E.row[line].chars;
        int query_len = strlen(query);
        char *ptr = render_line;
        while ((ptr = strstr(ptr, query)) != NULL) {
            new_lines[new_num_matches] = line;
            new_cols[new_num_matches] = ptr - render_line;
            new_num_matches++;
            ptr += query_len;
        }
    }

    free(E.find_match_lines);
    free(E.find_match_cols);
    E.find_match_lines = new_lines;
    E.find_match_cols = new_cols;
    E.find_num_matches = new_num_matches;
}

void editorReplace() {
    int saved_cursor_x = E.cursor_x;
    int saved_cursor_y = E.cursor_y;
    int saved_col_offset = E.col_offset;
    int saved_row_offset = E.row_offset;

    char *find_query = editorPrompt("Replace - Find: %s (ESC to cancel)", editorReplaceCallback, editorGetSelectedText());
    if (!find_query || strlen(find_query) == 0 || E.find_num_matches == 0) {
        editorSetStatusMsg("Replace cancelled");
        E.cursor_x = saved_cursor_x;
        E.cursor_y = saved_cursor_y;
        E.col_offset = saved_col_offset;
        E.row_offset = saved_row_offset;
        E.find_active = 0;
        free(find_query);
        return;
    }

    char *replace_query = editorPrompt("Replace - With: %s (ESC to cancel)", NULL, NULL);
    if (!replace_query) {
        editorSetStatusMsg("Replace cancelled");
        free(find_query);
        free(replace_query);
        free(E.find_query);
        free(E.find_match_lines);
        free(E.find_match_cols);
        E.find_query = NULL;
        E.find_match_lines = NULL;
        E.find_match_cols = NULL;
        E.find_num_matches = 0;
        E.find_current_idx = -1;
        E.find_active = 0;
        return;
    }

    int first = 1;
    int replaced = 0;
    int done = 0;
    int idx = E.find_current_idx < 0 ? 0 : E.find_current_idx;
    while (!done && E.find_num_matches > 0) {
        editorReplaceJumpToCurrent();
        editorRefreshScreen();
        editorSetStatusMsg("Arrows: navigate, Enter: replace, A: all, ESC: cancel");
        editorRefreshScreen();

        int key = editorReadKey();
        switch (key) {
            case ESCAPE_CHAR:
                done = 1;
                E.select_mode = 0;
                break;
            case ARROW_DOWN:
            case ARROW_RIGHT:
                idx = (idx + 1) % E.find_num_matches;
                E.find_current_idx = idx;
                editorReplaceJumpToCurrent();
                break;
            case ARROW_UP:
            case ARROW_LEFT:
                idx = (idx - 1 + E.find_num_matches) % E.find_num_matches;
                E.find_current_idx = idx;
                editorReplaceJumpToCurrent();
                break;
            case 'a':
            case 'A':
                if (first) {
                    saveEditorStateForUndo();
                    first = 0;
                }
                replaced += editorReplaceAll(replace_query);
                done = 1;
                break;
            case '\r':
            case '\n':
                if (first) {
                    saveEditorStateForUndo();
                    first = 0;
                }
                if (editorReplaceCurrent(find_query, replace_query)) {
                    replaced++;
                    editorScanLineMatches(E.cursor_y, find_query);
                    if (E.find_num_matches == 0) {
                        done = 1;
                    } else {
                        int line = E.cursor_y;
                        int next_idx = -1;
                        int cursor_pos = E.cursor_x;
                        for (int i = 0; i < E.find_num_matches; i++) {
                            if (E.find_match_lines[i] == line && E.find_match_cols[i] > cursor_pos) {
                                next_idx = i;
                                break;
                            }
                        }
                        if (next_idx == -1) {
                            for (int i = 0; i < E.find_num_matches; i++) {
                                if (E.find_match_lines[i] > line) {
                                    next_idx = i;
                                    break;
                                }
                            }
                        }

                        if (next_idx == -1)
                            next_idx = 0;
                        E.find_current_idx = next_idx;
                        editorReplaceJumpToCurrent();
                    }
                }
                editorReplaceCallback(find_query, 0);
                break;
            default:
                break;
        }
        editorSetStatusMsg("Replaced %d occurrence%s", replaced, replaced == 1 ? "" : "s");
    }

    free(replace_query);
    free(find_query);
    free(E.find_query);
    free(E.find_match_lines);
    free(E.find_match_cols);
    E.find_query = NULL;
    E.find_match_lines = NULL;
    E.find_match_cols = NULL;
    E.find_num_matches = 0;
    E.find_current_idx = -1;
    E.find_active = 0;
}

void editorReplaceCallback(char *query, int key) {
    (void)key;
    if (query == NULL || !query[0]) {
        E.find_active = 0;
        free(E.find_query);
        E.find_query = NULL;
        free(E.find_match_lines);
        E.find_match_lines = NULL;
        free(E.find_match_cols);
        E.find_match_cols = NULL;
        E.find_num_matches = 0;
        E.find_current_idx = -1;
        return;
    }

    if (E.find_query == NULL || strcmp(E.find_query, query) != 0) {
        free(E.find_query);
        E.find_query = safeStrdup(query);

        free(E.find_match_lines);
        free(E.find_match_cols);
        E.find_match_lines = NULL;
        E.find_match_cols = NULL;
        E.find_num_matches = 0;
        E.find_current_idx = -1;

        int query_len = strlen(query);
        for (int i = 0; i < E.num_rows; i++) {
            char *line = E.row[i].render;
            char *ptr = line;
            while ((ptr = strstr(ptr, query)) != NULL) {
                E.find_match_lines = safeRealloc(E.find_match_lines, sizeof(int) * (E.find_num_matches + 1));
                E.find_match_cols = safeRealloc(E.find_match_cols, sizeof(int) * (E.find_num_matches + 1));
                E.find_match_lines[E.find_num_matches] = i;
                E.find_match_cols[E.find_num_matches] = ptr - line;
                E.find_num_matches++;
                ptr += query_len;
            }
        }

        if (E.find_num_matches > 0) {
            E.find_current_idx = 0;
            int row = E.find_match_lines[0];
            int col = E.find_match_cols[0];
            E.cursor_y = row;
            E.cursor_x = editorRowRxToCx(&E.row[row], col);
            E.row_offset = (row >= E.screen_rows) ? row - E.screen_rows + MARGIN : 0;
            int render_pos = editorRowCxToRx(&E.row[row], E.cursor_x);
            int margin = query_len + MARGIN;
            E.col_offset = (render_pos > margin) ? render_pos - (E.screen_cols - margin) : 0;
            if (E.col_offset < 0)
                E.col_offset = 0;
        }
        E.find_active = 1;
    }
}

int editorReplaceAll(const char *replace_str) {
    char *find_str = E.find_query;
    int find_len = strlen(find_str);
    int replace_len = strlen(replace_str);
    int replaced_count = 0;

    for (int i = 0; i < E.num_rows; i++) {
        editorRow *row = &E.row[i];

        int j = 0;
        while (j <= row->size - find_len) {
            if (strncmp(&row->chars[j], find_str, find_len) == 0) {
                int new_size = row->size - find_len + replace_len;
                char *new_chars = safeRealloc(row->chars, new_size + 1);
                row->chars = new_chars;
                memmove(&row->chars[j + replace_len], &row->chars[j + find_len], row->size - (j + find_len) + 1);
                memcpy(&row->chars[j], replace_str, replace_len);
                row->size = new_size;
                row->chars[row->size] = '\0';

                editorUpdateRow(row);
                replaced_count++;
                j += replace_len;
            } else {
                j++;
            }
        }
    }
    return replaced_count;
}

void editorReplaceJumpToCurrent() {
    if (E.find_num_matches > 0 && E.find_current_idx >= 0 && E.find_current_idx < E.find_num_matches) {
        int row = E.find_match_lines[E.find_current_idx];
        int col = E.find_match_cols[E.find_current_idx];
        E.cursor_y = row;
        E.cursor_x = editorRowRxToCx(&E.row[row], col);
        E.preferred_x = E.cursor_x;
        E.row_offset = (row >= E.screen_rows) ? row - E.screen_rows + MARGIN : 0;
        int render_pos = editorRowCxToRx(&E.row[row], E.cursor_x);
        int margin = strlen(E.find_query) + MARGIN;
        E.col_offset = (render_pos > margin) ? render_pos - (E.screen_cols - margin) : 0;
        if (E.col_offset < 0)
            E.col_offset = 0;
    }
}

int editorReplaceCurrent(const char *find_str, const char *replace_str) {
    if (!find_str || !replace_str || E.find_num_matches == 0 || E.find_current_idx < 0) return 0;

    int row = E.find_match_lines[E.find_current_idx];
    int col = E.find_match_cols[E.find_current_idx];
    editorRow *er = &E.row[row];
    int find_len = strlen(find_str);
    int replace_len = strlen(replace_str);

    int cx = editorRowRxToCx(er, col);
    if (find_len != replace_len) {
        er->chars = safeRealloc(er->chars, er->size + replace_len - find_len + 1);
        memmove(&er->chars[cx + replace_len], &er->chars[cx + find_len], er->size - (cx + find_len) + 1);
        er->size = er->size + replace_len - find_len;
        er->chars[er->size] = '\0';
    }
    memcpy(&er->chars[cx], replace_str, replace_len);

    editorUpdateRow(er);
    E.dirty++;
    E.cursor_y = row;
    E.cursor_x = cx + replace_len;
    E.preferred_x = E.cursor_x;
    return 1;
}

void clipboardCopyToSystem(const char *data) {
    if (!data) return;
    FILE *pipe = NULL;

    switch (E.env) {
        case MACOS:
            pipe = popen("pbcopy", "w");
            break;
        case WSL:
            pipe = popen("clip.exe", "w");
            break;
        case LINUX:
            pipe = popen("xclip -selection clipboard", "w");
            break;
        case UNKNOWN:
            pipe = popen("xclip -selection clipboard", "w");
            break;
    }

    if (pipe) {
        fwrite(data, 1, strlen(data), pipe);
        pclose(pipe);
    } else {
        editorSetStatusMsg("Unable to copy to system clipboard");
    }
}

void editorCopySelection() {
    if (!E.select_mode) {
        editorSetStatusMsg("No selection to copy");
        return;
    }

    free(E.clipboard);
    E.clipboard = editorGetSelectedText();

    if (E.clipboard) {
        char sizebuf[SMALL_BUFFER_SIZE];
        humanReadableSize(strlen(E.clipboard), sizebuf, sizeof(sizebuf));
        editorSetStatusMsg("Copied %s", sizebuf);
        clipboardCopyToSystem(E.clipboard);
    }
}

void editorCutSelection() {
    if (!E.select_mode) {
        editorSetStatusMsg("No selection to cut");
        return;
    }

    free(E.clipboard);
    E.clipboard = editorGetSelectedText();
    if (!E.clipboard) return;

    clipboardCopyToSystem(E.clipboard);
    editorDeleteSelectedText();

    char sizebuf[SMALL_BUFFER_SIZE];
    humanReadableSize(strlen(E.clipboard), sizebuf, sizeof(sizebuf));
    editorSetStatusMsg("Cut %s", sizebuf);
}

void editorCutLine() {
    if (E.cursor_y >= E.num_rows) return;

    editorRow *current_row = &E.row[E.cursor_y];
    int line_length = current_row->size;
    char *line_content = safeMalloc(line_length + 2);
    memcpy(line_content, current_row->chars, line_length);
    line_content[line_length] = '\n';
    line_content[line_length + 1] = '\0';

    free(E.clipboard);
    E.clipboard = line_content;
    clipboardCopyToSystem(E.clipboard);
    editorDeleteRow(E.cursor_y);

    if (E.cursor_y >= E.num_rows && E.num_rows > 0)
        E.cursor_y = E.num_rows - 1;

    if (E.cursor_y >= 0 && E.cursor_y < E.num_rows) {
        int row_len = E.row[E.cursor_y].size;
        E.cursor_x = E.cursor_x > row_len ? row_len : E.cursor_x;
    }
    else
        E.cursor_x = 0;
    E.preferred_x = E.cursor_x;

    char sizebuf[SMALL_BUFFER_SIZE];
    humanReadableSize(strlen(E.clipboard), sizebuf, sizeof(sizebuf));
    editorSetStatusMsg("Cut %s", sizebuf);
    E.dirty++;
}

void editorPaste() {
    char *clipboard_data = NULL;
    size_t buf_size = 0;
    FILE *pipe = NULL;

    switch (E.env) {
        case MACOS:
            pipe = popen("pbpaste", "r");
            break;
        case WSL:
            pipe = popen("powershell.exe Get-Clipboard", "r");
            break;
        case LINUX:
            pipe = popen("xclip -selection clipboard -o 2>/dev/null", "r");
            break;
        case UNKNOWN:
            pipe = popen("xclip -selection clipboard -o", "r");
            break;
    }

    if (!pipe) {
        editorSetStatusMsg("Clipboard tool missing or inaccessible");
        return;
    }

    E.is_pasting = 1;
    saveEditorStateForUndo();
    char chunk[PASTE_CHUNK_SIZE];
    size_t len;
    while ((len = fread(chunk, 1, sizeof(chunk), pipe)) > 0) {
        clipboard_data = safeRealloc(clipboard_data, buf_size + len + 1);

        memcpy(clipboard_data + buf_size, chunk, len);
        buf_size += len;
    }
    pclose(pipe);

    if (!clipboard_data || buf_size == 0) {
        free(clipboard_data);
        editorSetStatusMsg("Clipboard is empty");
        E.is_pasting = 0;
        return;
    }

    clipboard_data[buf_size] = '\0';
    size_t j = 0;
    for (size_t i = 0; i < buf_size; i++)
        if (clipboard_data[i] != '\r')
            clipboard_data[j++] = clipboard_data[i];
    clipboard_data[j] = '\0';
    buf_size = j - 1;

    char indent[BUFFER_SIZE] = {0};
    int indent_len = 0;
    if (E.cursor_y < E.num_rows) {
        editorRow *row = &E.row[E.cursor_y];
        while (indent_len < row->size && (row->chars[indent_len] == ' ' || row->chars[indent_len] == '\t')) {
            indent[indent_len] = row->chars[indent_len];
            indent_len++;
        }
        indent[indent_len] = '\0';
    }

    if (E.select_mode)
        editorDeleteSelectedText();

    char *line_start = clipboard_data;
    char *newline_pos;
    while ((newline_pos = strchr(line_start, '\n')) != NULL) {
        size_t chunk_len = newline_pos - line_start;
        for (size_t i = 0; i < chunk_len; i++)
            editorInsertChar(line_start[i]);
        if (*(newline_pos + 1) != '\0') {
            editorInsertNewline();
            for (int i = 0; i < indent_len; i++)
                editorInsertChar(indent[i]);
        }
        line_start = newline_pos + 1;
    }

    while (*line_start)
        editorInsertChar(*line_start++);

    char sizebuf[SMALL_BUFFER_SIZE];
    humanReadableSize(buf_size, sizebuf, sizeof(sizebuf));
    editorSetStatusMsg("Pasted %s from system clipboard", sizebuf);
    free(clipboard_data);
    E.is_pasting = 0;
}

void editorJump() {
    int saved_cursor_x = E.cursor_x;
    int saved_cursor_y = E.cursor_y;
    int saved_col_offset = E.col_offset;
    int saved_row_offset = E.row_offset;

    char *input = editorPrompt("Jump to (row:col): %s (ESC to cancel)", editorJumpCallback, NULL);

    if (input) {
        free(input);
        if (E.cursor_x == saved_cursor_x && E.cursor_y == saved_cursor_y && E.col_offset == saved_col_offset && E.row_offset == saved_row_offset)
            editorSetStatusMsg("Invalid input");
        else
            editorSetStatusMsg("Jumped");
    } else {
        E.cursor_x = saved_cursor_x;
        E.cursor_y = saved_cursor_y;
        E.col_offset = saved_col_offset;
        E.row_offset = saved_row_offset;
        editorSetStatusMsg("Jump cancelled");
    }
}

void editorJumpCallback(char *buf, int key) {
    if (key == '\r' || key == ESCAPE_CHAR)
        return;

    int row = 0, col = 1;
    if (sscanf(buf, "%d:%d", &row, &col) != 2)
        sscanf(buf, "%d", &row);

    row = row > 0 ? row - 1 : 0;
    col = col > 0 ? col - 1 : 0;

    if (row >= E.num_rows) row = E.num_rows - 1;
    if (row < 0) row = 0;
    if (col < 0) col = 0;
    if (row >= 0 && row < E.num_rows && col > E.row[row].size) col = E.row[row].size;

    E.cursor_y = row;
    E.cursor_x = col;
    E.preferred_x = col;
    editorScroll();
}

void freeEditorState(editorState *state) {
    if (state->buffer) free(state->buffer);
    state->buffer = NULL;
}

void saveEditorStateForUndo() {
    long now = currentMillis();

    if (history.undo_top >= UNDO_REDO_STACK_SIZE - 1)
        return;

    if (!history.undo_in_progress || (now - history.last_edit_time > UNDO_TIMEOUT)) {
        for (int i = 0; i <= history.redo_top; i++)
            freeEditorState(&history.redo_stack[i]);
        history.redo_top = -1;

        history.undo_top++;
        editorState *state = &history.undo_stack[history.undo_top];
        freeEditorState(state);

        state->buffer = editorRowsToString(&state->buf_len);
        state->cursor_x = E.cursor_x;
        state->cursor_y = E.cursor_y;
        state->preferred_x = E.preferred_x;
        state->select_mode = E.select_mode;
        state->select_sx = E.select_sx;
        state->select_sy = E.select_sy;
        state->select_ex = E.select_ex;
        state->select_ey = E.select_ey;

        history.undo_in_progress = 1;
    }

    history.last_edit_time = now;
}

void restoreEditorState(const editorState *state) {
    for (int i = 0; i < E.num_rows; i++)
        editorFreeRow(&E.row[i]);
    free(E.row);
    E.row = NULL;
    E.num_rows = 0;

    size_t start = 0;
    for (int i = 0; i < state->buf_len; i++) {
        if (state->buffer[i] == '\n') {
            editorInsertRow(E.num_rows, &state->buffer[start], i - start);
            start = i + 1;
        }
    }

    E.cursor_x = state->cursor_x;
    E.cursor_y = state->cursor_y;
    E.preferred_x = state->preferred_x;
    E.select_mode = state->select_mode;
    E.select_sx = state->select_sx;
    E.select_sy = state->select_sy;
    E.select_ex = state->select_ex;
    E.select_ey = state->select_ey;
    E.dirty++;
}

void editorUndo() {
    if (history.undo_top < 0) {
        editorSetStatusMsg("Nothing to undo");
        return;
    }

    if (history.redo_top < UNDO_REDO_STACK_SIZE - 1) {
        history.redo_top++;
        editorState *state = &history.redo_stack[history.redo_top];

        state->buffer = editorRowsToString(&state->buf_len);
        state->cursor_x = E.cursor_x;
        state->cursor_y = E.cursor_y;
        state->preferred_x = E.preferred_x;
        state->select_mode = E.select_mode;
        state->select_sx = E.select_sx;
        state->select_sy = E.select_sy;
        state->select_ex = E.select_ex;
        state->select_ey = E.select_ey;
    }

    editorState *undo_st = &history.undo_stack[history.undo_top];
    restoreEditorState(undo_st);
    freeEditorState(undo_st);
    history.undo_top--;
    editorSetStatusMsg("Undo");
}

void editorRedo() {
    if (history.redo_top < 0) {
        editorSetStatusMsg("Nothing to redo");
        return;
    }

    if (history.undo_top < UNDO_REDO_STACK_SIZE - 1) {
        history.undo_top++;
        editorState *state = &history.undo_stack[history.undo_top];

        state->buffer = editorRowsToString(&state->buf_len);
        state->cursor_x = E.cursor_x;
        state->cursor_y = E.cursor_y;
        state->preferred_x = E.preferred_x;
        state->select_mode = E.select_mode;
        state->select_sx = E.select_sx;
        state->select_sy = E.select_sy;
        state->select_ex = E.select_ex;
        state->select_ey = E.select_ey;
    }

    editorState *redo_st = &history.redo_stack[history.redo_top];
    restoreEditorState(redo_st);
    freeEditorState(redo_st);
    history.redo_top--;
    editorSetStatusMsg("Redo");
}

char getMatchingBracket(char ch) {
    switch (ch) {
        case '(': return ')';
        case ')': return '(';
        case '{': return '}';
        case '}': return '{';
        case '[': return ']';
        case ']': return '[';
    }
    return 0;
}

int findMatchingBracketPosition(int cursor_y, int cursor_x, int *match_y, int *match_x) {
    if (cursor_y >= E.num_rows || cursor_x >= E.row[cursor_y].size)
        return 0;

    char bracket = E.row[cursor_y].chars[cursor_x];
    if (!(bracket == '(' || bracket == ')' || bracket == '{' || bracket == '}' || bracket == '[' || bracket == ']'))
        return 0;

    char match = getMatchingBracket(bracket);
    int direction = (bracket == '(' || bracket == '{' || bracket == '[') ? 1 : -1;
    int count = 1;

    int y = cursor_y;
    int x = cursor_x;
    while (1) {
        if (direction == 1) {
            x++;
            while (y < E.num_rows && x >= E.row[y].size) {
                y++;
                x = 0;
            }
            if (y >= E.num_rows) break;
        } else {
            x--;
            while (y >= 0 && x < 0) {
                y--;
                if (y >= 0) x = E.row[y].size - 1;
            }
            if (y < 0) break;
        }

        char ch = E.row[y].chars[x];
        if (ch == bracket)
            count++;
        else if (ch == match)
            count--;

        if (count == 0) {
            *match_y = y;
            *match_x = x;
            return 1;
        }
    }

    return 0;
}

void updateMatchBracket() {
    int mx, my;
    if (findMatchingBracketPosition(E.cursor_y, E.cursor_x, &my, &mx)) {
        E.match_bracket_x = mx;
        E.match_bracket_y = my;
        E.has_match_bracket = 1;
    } else {
        E.has_match_bracket = 0;
    }
}

void editorMouseLeftClick() {
    clampCursorPosition();
    E.select_mode = 1;
    E.select_sx = E.cursor_x;
    E.select_sy = E.cursor_y;
    E.select_ex = E.cursor_x;
    E.select_ey = E.cursor_y;
    E.preferred_x = E.cursor_x;
}

void editorMouseDragClick() {
    clampCursorPosition();
    E.select_ex = E.cursor_x;
    E.select_ey = E.cursor_y;
    E.preferred_x = E.cursor_x;
}

void editorMouseLeftRelease() {
    clampCursorPosition();
    if (E.select_ex == E.select_sx && E.select_ey == E.select_sy)
        E.select_mode = 0;
}
