/*** Includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>

/*** Defines ***/

#define CYPHER_VERSION      "1.2.5"
#define EMPTY_LINE_SYMBOL   "~"

#define CTRL_KEY(k)         ((k) & 0x1f)
#define is_cntrl(k)         ((k) < 32 || (k) == 127)
#define is_alnum(k)         (((k) >= 'a' && (k) <= 'z') || ((k) >= 'A' && (k) <= 'Z') || ((k) >= '0' && (k) <= '9'))

#define TAB_SIZE                4
#define QUIT_TIMES              2
#define SAVE_TIMES              2
#define UNDO_REDO_STACK_SIZE    100
#define UNDO_TIMEOUT            1000
#define STATUS_LENGTH           80
#define SMALL_BUFFER_SIZE       32
#define BUFFER_SIZE             128
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
#define BRACKETED_PASTE_ON      "\x1b[?2004h"
#define BRACKETED_PASTE_OFF     "\x1b[?2004l"
#define ENTER_ALTERNATE_SCREEN  "\x1b[?1049h"
#define EXIT_ALTERNATE_SCREEN   "\x1b[?1049l"
#define ENABLE_MOUSE            "\x1b[?1000h\x1b[?1002h\x1b[?1015h\x1b[?1006h"
#define DISABLE_MOUSE           "\x1b[?1006l\x1b[?1015l\x1b[?1002l\x1b[?1000l"

/*** Structs and Enums ***/

typedef enum {
    false = 0,
    true = 1
} bool;

typedef enum {
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
    MOUSE_LEFT_RELEASE,
    PASTE_START,
    PASTE_END
} editorKey;

typedef struct {
    int size;
    int rsize;
    char *chars;
    char *render;
} editorRow;

typedef struct {
    int x;
    int y;
    int render_x;
    int preferred_x;
} editorCursor;

typedef struct {
    int screen_rows;
    int screen_cols;
    int row_offset;
    int col_offset;
    volatile sig_atomic_t resized;
} editorView;

typedef struct {
    editorRow *rows;
    int num_rows;
    int row_capacity;
    char *filename;
    bool dirty;
    int save_times;
    int quit_times;
} editorBuffer;

typedef struct {
    bool active;
    int sx;
    int sy;
    int ex;
    int ey;
    char *clipboard;
    bool is_pasting;
    int paste_len;
} editorSelection;

typedef struct {
    bool active;
    char *query;
    int *match_lines;
    int *match_cols;
    int num_matches;
    int current_idx;
} editorFinder;

typedef struct {
    char status_msg[STATUS_LENGTH];
    time_t status_msg_time;
    struct termios orig_termios;
    int bracket_x;
    int bracket_y;
    bool has_bracket;
} editorSystem;

typedef struct {
    editorCursor cursor;
    editorView view;
    editorBuffer buf;
    editorSelection sel;
    editorFinder find;
    editorSystem sys;
} editorConfig;

typedef struct {
    char *b;
    int len;
    int capacity;
} appendBuffer;

typedef struct {
    char *buffer;
    size_t buf_len;
    editorCursor cursor;
    editorSelection sel;
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

static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
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
bool isWordChar(int);
long currentMillis();
char getClosingChar(char);
void clampCursorPosition();
void humanReadableSize(size_t, char *, size_t);
void base64_encode(const char *, int, char *);

// memory
void *safeMalloc(size_t);
void *safeRealloc(void *, size_t);
char *safeStrdup(const char *);

// terminal
void handleSigWinCh(int);
void die(const char *);
void enableRawMode();
void disableRawMode();
int editorReadKey();
int getWindowSize(int *, int *);
int getCursorPosition(int *, int *);

// input
void editorProcessKeypress();
void editorMoveCursor(int);
char *editorPrompt(char *, void (*)(const char *, int), char *);
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
void editorSetStatusMsg(const char *);
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
char *editorRowsToString(size_t *);
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
char *editorGetSelectedText(int *);
void editorDeleteSelectedText();

// find-replace operations
void editorFind();
void editorFindCallback(const char *, int);
void editorScanLineMatches(int, const char *);
void editorReplace();
void editorReplaceCallback(const char *, int);
int editorReplaceAll(const char *);
void editorReplaceJumpToCurrent();
bool editorReplaceCurrent(const char *, const char *);

// clipboard operations
void clipboardCopyToSystem(const char *, int);
void editorCopySelection();
void editorCutSelection();
void editorCutLine();

// jump operations
void editorJump();
void editorJumpCallback(const char *, int);

// undo-redo operations
void freeEditorState(editorState *);
void saveEditorStateForUndo();
void restoreEditorState(const editorState *);
void editorUndo();
void editorRedo();

// bracket highlighting
char getMatchingBracket(char);
bool findMatchingBracketPosition(int, int, int *, int *);
void updateMatchBracket();

// mouse operations
void editorMouseLeftClick();
void editorMouseDragClick();
void editorMouseLeftRelease();

/*** Main ***/

int main(int argc, char *argv[]) {
    enableRawMode();

    struct sigaction sa;
    sa.sa_handler = handleSigWinCh;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGWINCH, &sa, NULL);

    editorInit();
    if (argc >= 2)
        editorOpen(argv[1]);

    editorSetStatusMsg("HELP: Ctrl-H");
    while (1) {
        if (E.view.resized) {
            E.view.resized = 0;
            if (getWindowSize(&E.view.screen_rows, &E.view.screen_cols) == -1) die("getWindowSize");
            E.view.screen_rows -= 2;
            editorRefreshScreen();
        }
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}

/*** Function Definitions ***/

void clearTerminal() {
    if (write(STDOUT_FILENO, CLEAR_SCREEN  CURSOR_RESET, sizeof(CLEAR_SCREEN  CURSOR_RESET)) == -1) {}
}

bool isWordChar(int ch) {
    return is_alnum(ch) || ch == '_';
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
    if (E.buf.num_rows == 0) {
        E.cursor.x = 0;
        E.cursor.y = 0;
        return;
    }

    if (E.cursor.y < 0)
        E.cursor.y = 0;
    else if (E.cursor.y >= E.buf.num_rows) {
        E.cursor.y = E.buf.num_rows - 1;
        E.cursor.x = E.buf.rows[E.cursor.y].size;
        return;
    }

    if (E.cursor.x < 0)
        E.cursor.x = 0;
    else if (E.cursor.x > E.buf.rows[E.cursor.y].size)
        E.cursor.x = E.buf.rows[E.cursor.y].size;
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

void base64_encode(const char *src, int len, char *out) {
    int i, j;
    for (i = 0, j = 0; i < len; i += 3, j += 4) {
        int v = src[i];
        v = i + 1 < len ? v << 8 | src[i + 1] : v << 8;
        v = i + 2 < len ? v << 8 | src[i + 2] : v << 8;

        out[j] = base64_table[(v >> 18) & 0x3F];
        out[j + 1] = base64_table[(v >> 12) & 0x3F];
        if (i + 1 < len) out[j + 2] = base64_table[(v >> 6) & 0x3F];
        else out[j + 2] = '=';
        if (i + 2 < len) out[j + 3] = base64_table[v & 0x3F];
        else out[j + 3] = '=';
    }
    out[j] = '\0';
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

void handleSigWinCh(int unused) {
    (void)unused;
    E.view.resized = 1;
}

void die(const char *str) {
    write(STDOUT_FILENO, EXIT_ALTERNATE_SCREEN, sizeof(EXIT_ALTERNATE_SCREEN) - 1);
    perror(str);
    exit(1);
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.sys.orig_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);
    atexit(editorCleanup);

    struct termios raw = E.sys.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);   // Input flags
    raw.c_oflag &= ~(OPOST);                                    // Output flags
    raw.c_cflag |= (CS8);                                       // Set char size to 8 bits/byte
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);            // Local flags
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    write(STDOUT_FILENO, ENTER_ALTERNATE_SCREEN, sizeof(ENTER_ALTERNATE_SCREEN) - 1);
    write(STDOUT_FILENO, ENABLE_MOUSE, sizeof(ENABLE_MOUSE) - 1);
    write(STDOUT_FILENO, BRACKETED_PASTE_ON, sizeof(BRACKETED_PASTE_ON) - 1);
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.sys.orig_termios) == -1) die("tcsetattr");
    write(STDOUT_FILENO, DISABLE_MOUSE, sizeof(DISABLE_MOUSE) - 1);
    write(STDOUT_FILENO, BRACKETED_PASTE_OFF, sizeof(BRACKETED_PASTE_OFF) - 1);
    write(STDOUT_FILENO, EXIT_ALTERNATE_SCREEN, sizeof(EXIT_ALTERNATE_SCREEN) - 1);
}

int editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno == EAGAIN) continue;
        if (nread == -1 && errno == EINTR) return 0;
        if (nread == 0) return 0;
        die("read");
    }

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
                        E.cursor.x = x + E.view.col_offset;
                        E.cursor.y = y + E.view.row_offset;
                        if (!motion && seq[i] == 'M') return MOUSE_LEFT_CLICK;
                        else if (motion && seq[i] == 'M') return MOUSE_DRAG;
                        else if (seq[i] == 'm') return MOUSE_LEFT_RELEASE;
                    }
                }
                return ESCAPE_CHAR;
            } else if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return ESCAPE_CHAR;
                if (seq[2] == '0' || seq[2] == '1') {
                    char seq3, seq4;
                    if (read(STDIN_FILENO, &seq3, 1) == 1 && (seq3 == '0' || seq3 == '1')) {
                        if (read(STDIN_FILENO, &seq4, 1) == 1 && seq4 == '~') {
                            if (seq[1] == '2' && seq[2] == '0' && seq3 == '0') return PASTE_START;
                            if (seq[1] == '2' && seq[2] == '0' && seq3 == '1') return PASTE_END;
                        }
                    }
                }
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
    if (ch == 0) return;        // phantom key
    if (ch != CTRL_KEY('q'))
        E.buf.quit_times = QUIT_TIMES;

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
            if (E.sel.active)
                editorCutSelection();
            else
                editorCutLine();
            updateMatchBracket();
            break;

        case PASTE_START:       // paste
            E.sel.is_pasting = true;
            E.sel.paste_len = 0;
            break;
        case PASTE_END:
            E.sel.is_pasting = false;
            {
                char sizebuf[SMALL_BUFFER_SIZE];
                humanReadableSize(E.sel.paste_len, sizebuf, sizeof(sizebuf));
                char msg[STATUS_LENGTH];
                snprintf(msg, sizeof(msg), "Pasted %s", sizebuf);
                editorSetStatusMsg(msg);
            }
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
            if (E.sel.is_pasting) E.sel.paste_len++;
            saveEditorStateForUndo();
            editorDeleteSelectedText();
            editorInsertNewline();
            updateMatchBracket();
            break;

        case '\t':              // tab
            if (E.sel.is_pasting) E.sel.paste_len++;
            saveEditorStateForUndo();
            editorInsertChar('\t');
            updateMatchBracket();
            break;

        case HOME_KEY:
            E.cursor.x = 0;
            E.cursor.preferred_x = E.cursor.x;
            E.sel.active = false;
            updateMatchBracket();
            break;
        case END_KEY:
            E.sel.active = false;
            if (E.cursor.y < E.buf.num_rows) {
                E.cursor.x = E.buf.rows[E.cursor.y].size;
                E.cursor.preferred_x = E.cursor.x;
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
            editorScrollPageUp(E.view.screen_rows);
            updateMatchBracket();
            break;
        case PAGE_DOWN:
            editorScrollPageDown(E.view.screen_rows);
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
            if (!E.sel.active) {
                E.sel.active = true;
                E.sel.sx = E.cursor.x;
                E.sel.sy = E.cursor.y;
            }
            editorMoveWordLeft();
            E.sel.ex = E.cursor.x;
            E.sel.ey = E.cursor.y;
            updateMatchBracket();
            break;
        case CTRL_SHIFT_ARROW_RIGHT:
            if (!E.sel.active) {
                E.sel.active = true;
                E.sel.sx = E.cursor.x;
                E.sel.sy = E.cursor.y;
            }
            editorMoveWordRight();
            E.sel.ex = E.cursor.x;
            E.sel.ey = E.cursor.y;
            updateMatchBracket();
            break;

        case SHIFT_HOME:
            if (!E.sel.active) {
                E.sel.active = true;
                E.sel.sx = E.cursor.x;
                E.sel.sy = E.cursor.y;
            }
            E.cursor.x = 0;
            E.sel.ex = E.cursor.x;
            E.sel.ey = E.cursor.y;
            updateMatchBracket();
            break;
        case SHIFT_END:
            if (!E.sel.active) {
                E.sel.active = true;
                E.sel.sx = E.cursor.x;
                E.sel.sy = E.cursor.y;
            }
            if (E.cursor.y < E.buf.num_rows)
                E.cursor.x = E.buf.rows[E.cursor.y].size;
            E.sel.ex = E.cursor.x;
            E.sel.ey = E.cursor.y;
            updateMatchBracket();
            break;

        case ARROW_LEFT:
        case ARROW_RIGHT:
        case ARROW_UP:
        case ARROW_DOWN:
            history.undo_in_progress = 0;
            E.sel.active = false;
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
            if (E.sel.active)
                E.sel.active = false;
            break;

        default:
            if (!is_cntrl(ch)) {
                if (E.sel.is_pasting) E.sel.paste_len++;

                saveEditorStateForUndo();
                if (E.sel.active) {
                    char closing = getClosingChar(ch);
                    if (closing != 0) {
                        char *selected = editorGetSelectedText(NULL);
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
    if (E.buf.num_rows == 0)
        return;
    editorRow *row = (E.cursor.y >= E.buf.num_rows) ? NULL : &E.buf.rows[E.cursor.y];

    switch (key) {
        case ARROW_LEFT:
            if (E.cursor.x != 0) {
                E.cursor.x--;
            } else if (E.cursor.y > 0) {
                E.cursor.y--;
                E.cursor.x = E.buf.rows[E.cursor.y].size;
            }
            E.cursor.preferred_x = E.cursor.x;
            break;
        case ARROW_RIGHT:
            if (row && E.cursor.x < row->size) {
                E.cursor.x++;
            } else if (row && E.cursor.x == row->size && E.cursor.y < E.buf.num_rows - 1) {
                E.cursor.y++;
                E.cursor.x = 0;
            }
            E.cursor.preferred_x = E.cursor.x;
            break;
        case ARROW_DOWN:
            if (E.cursor.y < E.buf.num_rows - 1) {
                E.cursor.y++;
                row = (E.cursor.y >= E.buf.num_rows) ? NULL : &E.buf.rows[E.cursor.y];
                if (E.cursor.preferred_x < 0) E.cursor.preferred_x = E.cursor.x;
                E.cursor.x = row && row->size > E.cursor.preferred_x ? E.cursor.preferred_x : (row ? row->size : 0);
            } else {
                E.cursor.x = E.buf.rows[E.cursor.y].size;
                E.cursor.preferred_x = E.buf.rows[E.cursor.y].size;
            }
            break;
        case ARROW_UP:
            if (E.cursor.y > 0) {
                E.cursor.y--;
                row = (E.cursor.y >= E.buf.num_rows) ? NULL : &E.buf.rows[E.cursor.y];
                if (E.cursor.preferred_x < 0) E.cursor.preferred_x = E.cursor.x;
                E.cursor.x = row && row->size > E.cursor.preferred_x ? E.cursor.preferred_x : (row ? row->size : 0);
            } else {
                E.cursor.x = 0;
                E.cursor.preferred_x = 0;
            }
            break;
    }

    row = (E.cursor.y >= E.buf.num_rows) ? NULL : &E.buf.rows[E.cursor.y];
    int row_len = row ? row->size : 0;
    if (E.cursor.x > row_len)
        E.cursor.x = row_len;
}

char *editorPrompt(char *prompt, void (*callback)(const char *, int), char *initial) {
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
        char msg[STATUS_LENGTH];
        snprintf(msg, sizeof(msg), prompt, buf);
        editorSetStatusMsg(msg);
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
        } else if (!is_cntrl(ch) && ch < 128) {
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
    if (E.cursor.y >= E.buf.num_rows) return;

    while (E.cursor.x > 0 && !isWordChar(E.buf.rows[E.cursor.y].chars[E.cursor.x - 1]))
        E.cursor.x--;
    while (E.cursor.x > 0 && isWordChar(E.buf.rows[E.cursor.y].chars[E.cursor.x - 1]))
        E.cursor.x--;
    E.cursor.preferred_x = E.cursor.x;
}

void editorMoveWordRight() {
    if (E.cursor.y >= E.buf.num_rows) return;

    int len = E.buf.rows[E.cursor.y].size;
    while (E.cursor.x < len && !isWordChar(E.buf.rows[E.cursor.y].chars[E.cursor.x]))
        E.cursor.x++;
    while (E.cursor.x < len && isWordChar(E.buf.rows[E.cursor.y].chars[E.cursor.x]))
        E.cursor.x++;
    E.cursor.preferred_x = E.cursor.x;
}

void editorScrollPageUp(int scroll_amount) {
    if (E.buf.num_rows == 0)
        return;

    if (E.view.row_offset > 0) {
        if (scroll_amount > E.view.row_offset)
            scroll_amount = E.view.row_offset;

        E.view.row_offset -= scroll_amount;
        E.cursor.y -= scroll_amount;

        if (E.cursor.y < 0)
            E.cursor.y = 0;

        if (E.cursor.y < E.buf.num_rows) {
            int row_len = E.buf.rows[E.cursor.y].size;
            if (E.cursor.preferred_x > row_len)
                E.cursor.x = row_len;
            else
                E.cursor.x = E.cursor.preferred_x;
        }
    } else {
        if (E.cursor.y != 0)
            E.cursor.y = 0;
        else {
            E.cursor.x = 0;
            E.cursor.preferred_x = 0;
        }
    }
}

void editorScrollPageDown(int scroll_amount) {
    if (E.buf.num_rows == 0)
        return;

    if (E.view.row_offset < E.buf.num_rows - E.view.screen_rows) {
        E.view.row_offset += scroll_amount;
        E.cursor.y += scroll_amount;

        if (E.cursor.y >= E.buf.num_rows)
            E.cursor.y = E.buf.num_rows - 1;

        if (E.cursor.y < E.buf.num_rows) {
            int row_len = E.buf.rows[E.cursor.y].size;
            if (E.cursor.preferred_x > row_len)
                E.cursor.x = row_len;
            else
                E.cursor.x = E.cursor.preferred_x;
        }
    } else {
        if (E.cursor.y != E.buf.num_rows - 1)
            E.cursor.y = E.buf.num_rows - 1;
        else {
            E.cursor.x = E.buf.rows[E.cursor.y].size;
            E.cursor.preferred_x = E.buf.rows[E.cursor.y].size;
        }
    }
}

void editorDrawWelcomeMessage(appendBuffer *ab) {
    char welcome[STATUS_LENGTH];
    int welcome_len = snprintf(welcome, sizeof(welcome), "Cypher Version %s", CYPHER_VERSION);
    if (welcome_len > E.view.screen_cols) welcome_len = E.view.screen_cols;

    int padding = (E.view.screen_cols - welcome_len) / 2;
    int vertical_center = E.view.screen_rows / 3;

    for (int y = 0; y < E.view.screen_rows; y++) {
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

    appendBuffer ab = {
        .b = NULL,
        .len = 0,
        .capacity = 0,
    };
    abAppend(&ab, HIDE_CURSOR CURSOR_RESET, sizeof(HIDE_CURSOR CURSOR_RESET) - 1);

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMsgBar(&ab);

    char buf[SMALL_BUFFER_SIZE];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cursor.y - E.view.row_offset) + 1, (E.cursor.render_x - E.view.col_offset) + 1);
    abAppend(&ab, buf, strlen(buf));
    abAppend(&ab, SHOW_CURSOR, sizeof(SHOW_CURSOR) - 1);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

void editorDrawRows(appendBuffer *ab) {
    if (E.buf.num_rows == 0) {
        editorDrawWelcomeMessage(ab);
        return;
    }

    for (int y = 0; y < E.view.screen_rows; y++) {
        int file_row = y + E.view.row_offset;
        if (file_row >= E.buf.num_rows) {
            abAppend(ab, EMPTY_LINE_SYMBOL, sizeof(EMPTY_LINE_SYMBOL) - 1);
        } else {
            int len = E.buf.rows[file_row].rsize - E.view.col_offset;
            if (len < 0) len = 0;
            if (len > E.view.screen_cols) len = E.view.screen_cols;

            for (int j = 0; j < len; j++) {
                int cx = editorRowRxToCx(&E.buf.rows[file_row], j + E.view.col_offset);
                int is_sel = 0;
                if (E.sel.active) {
                    int y1 = E.sel.sy, x1 = E.sel.sx;
                    int y2 = E.sel.ey, x2 = E.sel.ex;
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
                if (E.find.active && E.find.query) {
                    int match_len = strlen(E.find.query);
                    for (int m = 0; m < E.find.num_matches; m++) {
                        if (E.find.match_lines[m] == file_row) {
                            int col_start = E.find.match_cols[m];
                            int col_end = col_start + match_len;
                            if (cx >= col_start && cx < col_end) {
                                is_find = 1;
                                if (m == E.find.current_idx)
                                    is_current_match = 1;
                                break;
                            }
                        }
                    }
                }

                if (is_sel && !is_find) abAppend(ab, LIGHT_GRAY_BG_COLOR, sizeof(LIGHT_GRAY_BG_COLOR) - 1);
                else if (is_current_match) abAppend(ab, YELLOW_FG_COLOR, sizeof(YELLOW_FG_COLOR) - 1);
                else if (is_find) abAppend(ab, LIGHT_GRAY_BG_COLOR, sizeof(LIGHT_GRAY_BG_COLOR) - 1);

                if (E.sys.has_bracket && !E.sel.active && !E.find.active) {
                    int start_row = E.cursor.y;
                    int start_col = E.cursor.x;
                    int end_row = E.sys.bracket_y;
                    int end_col = E.sys.bracket_x;

                    if (start_row > end_row || (start_row == end_row && start_col > end_col)) {
                        int tmp_r = start_row, tmp_c = start_col;
                        start_row = end_row;
                        start_col = end_col;
                        end_row = tmp_r;
                        end_col = tmp_c;
                    }

                    if (file_row >= start_row && file_row <= end_row) {
                        int highlight_start_col = 0;
                        int highlight_end_col = E.buf.rows[file_row].size;

                        if (file_row == start_row)
                            highlight_start_col = start_col;
                        if (file_row == end_row)
                            highlight_end_col = end_col + 1;

                        if (cx >= highlight_start_col && cx < highlight_end_col) {
                            abAppend(ab, DARK_GRAY_BG_COLOR, sizeof(DARK_GRAY_BG_COLOR) - 1);
                            abAppend(ab, &E.buf.rows[file_row].render[j + E.view.col_offset], 1);
                            abAppend(ab, REMOVE_GRAPHICS, sizeof(REMOVE_GRAPHICS) - 1);
                        } else {
                            abAppend(ab, &E.buf.rows[file_row].render[j + E.view.col_offset], 1);
                        }
                    } else {
                        abAppend(ab, &E.buf.rows[file_row].render[j + E.view.col_offset], 1);
                    }
                } else {
                    abAppend(ab, &E.buf.rows[file_row].render[j + E.view.col_offset], 1);
                }

                if (is_sel || is_find || is_current_match) abAppend(ab, REMOVE_GRAPHICS, sizeof(REMOVE_GRAPHICS) - 1);
            }
        }
        abAppend(ab, CLEAR_LINE NEW_LINE, sizeof(CLEAR_LINE NEW_LINE) - 1);
    }
}

void editorScroll() {
    E.cursor.render_x = 0;
    if (E.cursor.y < E.buf.num_rows)
        E.cursor.render_x = editorRowCxToRx(&E.buf.rows[E.cursor.y], E.cursor.x);

    if (E.cursor.y < E.view.row_offset)
        E.view.row_offset = E.cursor.y;
    if (E.cursor.y >= E.view.row_offset + E.view.screen_rows)
        E.view.row_offset = E.cursor.y - E.view.screen_rows + 1;
    if (E.cursor.render_x < E.view.col_offset)
        E.view.col_offset = E.cursor.render_x;
    if (E.cursor.render_x >= E.view.col_offset + E.view.screen_cols)
        E.view.col_offset = E.cursor.render_x - E.view.screen_cols + 1;
}

void editorDrawStatusBar(appendBuffer *ab) {
    abAppend(ab, INVERTED_COLORS, sizeof(INVERTED_COLORS) - 1);
    char status[STATUS_LENGTH], rstatus[STATUS_LENGTH];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s", E.buf.filename ? E.buf.filename : "[No Name]", E.buf.num_rows, E.buf.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d:%d", E.cursor.y + 1, E.cursor.x + 1);
    if (len > E.view.screen_cols)
        len = E.view.screen_cols;
    abAppend(ab, status, len);

    while (len < E.view.screen_cols) {
        if (E.view.screen_cols - len == rlen) {
            abAppend(ab, rstatus, rlen);
            break;
        } else {
            abAppend(ab, " ", 1);
            len++;
        }
    }
    abAppend(ab, REMOVE_GRAPHICS NEW_LINE, sizeof(REMOVE_GRAPHICS NEW_LINE) - 1);
}

void editorSetStatusMsg(const char *msg) {
    snprintf(E.sys.status_msg, sizeof(E.sys.status_msg), "%s", msg);
    E.sys.status_msg_time = time(NULL);
}

void editorDrawMsgBar(appendBuffer *ab) {
    abAppend(ab, CLEAR_LINE, sizeof(CLEAR_LINE));

    int msg_len = strlen(E.sys.status_msg);
    if (msg_len > E.view.screen_cols)
        msg_len = E.view.screen_cols;
    if (msg_len && time(NULL) - E.sys.status_msg_time < STATUS_MSG_TIMEOUT_SEC)
        abAppend(ab, E.sys.status_msg, msg_len);

    if (E.find.active) {
        char buf[SMALL_BUFFER_SIZE];
        snprintf(buf, sizeof(buf), " %d/%d", E.find.current_idx + 1, E.find.num_matches);
        int right_len = strlen(buf);

        while (msg_len + right_len < E.view.screen_cols) {
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
    E.cursor.x = 0;
    E.cursor.y = 0;
    E.cursor.render_x = 0;
    E.cursor.preferred_x = 0;
    E.view.row_offset = 0;
    E.view.col_offset = 0;
    E.view.resized = 0;
    E.buf.rows = NULL;
    E.buf.num_rows = 0;
    E.buf.row_capacity = 0;
    E.buf.filename = NULL;
    E.buf.dirty = false;
    E.buf.save_times = SAVE_TIMES;
    E.buf.quit_times = QUIT_TIMES;
    E.sel.active = false;
    E.sel.sx = 0;
    E.sel.sy = 0;
    E.sel.ex = 0;
    E.sel.ey = 0;
    E.sel.clipboard = NULL;
    E.sel.is_pasting = false;
    E.sel.paste_len = 0;
    E.find.active = false;
    E.find.query = NULL;
    E.find.match_lines = NULL;
    E.find.match_cols = NULL;
    E.find.num_matches = 0;
    E.find.current_idx = -1;
    E.sys.status_msg[0] = '\0';
    E.sys.status_msg_time = 0;
    E.sys.bracket_x = 0;
    E.sys.bracket_y = 0;
    E.sys.has_bracket = false;

    if (getWindowSize(&E.view.screen_rows, &E.view.screen_cols) == -1) die("getWindowSize");
    E.view.screen_rows -= 2;
}

void editorCleanup() {
    if (E.buf.rows != NULL) {
        for (int i = 0; i < E.buf.num_rows; i++)
            editorFreeRow(&E.buf.rows[i]);
        free(E.buf.rows);
        E.buf.rows = NULL;
    }
    free(E.buf.filename);
    E.buf.filename = NULL;

    free(E.sel.clipboard);
    E.sel.clipboard = NULL;

    free(E.find.query);
    free(E.find.match_lines);
    free(E.find.match_cols);
    E.find.query = NULL;
    E.find.match_lines = NULL;
    E.find.match_cols = NULL;
    E.find.num_matches = 0;
    E.find.current_idx = -1;
    E.find.active = false;
}

void abAppend(appendBuffer *ab, const char *str, int len) {
    if (ab->len + len >= ab->capacity) {
        int new_capacity = ab->capacity == 0 ? 1024 : ab->capacity * 2;
        while (new_capacity < ab->len + len)
            new_capacity *= 2;

        ab->b = safeRealloc(ab->b, new_capacity);
        ab->capacity = new_capacity;
    }
    memcpy(&ab->b[ab->len], str, len);
    ab->len += len;
}

void abFree(appendBuffer *ab) {
    free(ab->b);
}

void editorOpen(const char *filename) {
    free(E.buf.filename);
    E.buf.filename = safeStrdup(filename);

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        if (errno == ENOENT)
            return;
        die("fopen");
    }

    char *line = NULL;
    size_t line_cap = 0;
    ssize_t line_len;
    while ((line_len = getline(&line, &line_cap, fp)) != -1) {
        while (line_len > 0 && (line[line_len - 1] == '\n' || line[line_len - 1] == '\r'))
            line_len--;
        editorInsertRow(E.buf.num_rows, line, line_len);
    }
    free(line);
    fclose(fp);
    E.buf.dirty = true;
}

char *editorRowsToString(size_t *buf_len) {
    size_t total_len = 0;
    for (int i = 0; i < E.buf.num_rows; i++) {
        total_len += E.buf.rows[i].size;
        if (i < E.buf.num_rows - 1 || E.buf.rows[i].size > 0)
            total_len += 1;
    }
    *buf_len = total_len;

    char *buf = safeMalloc(total_len + 1);
    char *ptr = buf;
    for (int i = 0; i < E.buf.num_rows; i++) {
        memcpy(ptr, E.buf.rows[i].chars, E.buf.rows[i].size);
        ptr += E.buf.rows[i].size;
        if (i < E.buf.num_rows - 1 || E.buf.rows[i].size > 0) {
            *ptr = '\n';
            ptr++;
        }
    }

    return buf;
}

void editorSave() {
    static bool new_file = false;
    if (E.buf.filename == NULL) {
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
        E.buf.filename = input;
        new_file = true;
    }

    if (new_file && access(E.buf.filename, F_OK) == 0 && E.buf.save_times != 0) {
        char msg[STATUS_LENGTH];
        snprintf(msg, sizeof(msg), "File exists! Press Ctrl-S %d more time%s to overwrite.", E.buf.save_times, E.buf.save_times == 1 ? "" : "s");
        editorSetStatusMsg(msg);
        E.buf.save_times--;
        return;
    }
    E.buf.save_times = SAVE_TIMES;
    new_file = false;

    size_t len = strlen(E.buf.filename) + 5;
    char *tmp_filename = safeMalloc(len);
    snprintf(tmp_filename, len, "%s.tmp", E.buf.filename);

    mode_t file_mode = 0644;
    struct stat st;
    if (stat(E.buf.filename, &st) == 0)
        file_mode = st.st_mode; // Keep existing permissions
    int fd = open(tmp_filename,
                  O_RDWR  |     // read and write
                  O_CREAT |     // create if doesn't exist
                  O_TRUNC,      // clear the file
                  file_mode);        // permissions
    if (fd == -1) {
        free(tmp_filename);
        char msg[STATUS_LENGTH];
        snprintf(msg, sizeof(msg), "Can't save! I/O error: %s", strerror(errno));
        editorSetStatusMsg(msg);
        return;
    }

    int total_bytes = 0;
    bool success = true;
    for (int i = 0; i < E.buf.num_rows; i++) {
        if (E.buf.rows[i].size > 0) {
            if (write(fd, E.buf.rows[i].chars, E.buf.rows[i].size) != E.buf.rows[i].size) {
                success = false;
                break;
            }
            total_bytes += E.buf.rows[i].size;
        }

        if (i < E.buf.num_rows - 1 || E.buf.rows[i].size > 0) {
            if (write(fd, "\n", 1) != 1) {
                success = false;
                break;
            }
            total_bytes++;
        }
    }

    close(fd);
    if (success) {
        E.buf.dirty = false;
        E.buf.quit_times = QUIT_TIMES;
        char sizebuf[SMALL_BUFFER_SIZE];
        humanReadableSize(total_bytes, sizebuf, sizeof(sizebuf));

        if (rename(tmp_filename, E.buf.filename) == -1) {
            unlink(tmp_filename);
            editorSetStatusMsg("Save failed! Could not rename tmp file.");
        }
        else {
            char msg[STATUS_LENGTH];
            snprintf(msg, sizeof(msg), "%s written to disk", sizebuf);
            editorSetStatusMsg(msg);
        }
    }
    else {
        unlink(tmp_filename);
        editorSetStatusMsg("Can't save! Write error on disk.");
    }
    free(tmp_filename);
}

void editorQuit() {
    if (E.buf.dirty && E.buf.quit_times > 0) {
        char msg[STATUS_LENGTH];
        snprintf(msg, sizeof(msg), "WARNING!!! File has unsaved changes. Press Ctrl-Q %d more time%s to quit.", E.buf.quit_times, E.buf.quit_times == 1 ? "" : "s");
        editorSetStatusMsg(msg);
        E.buf.quit_times--;
        return;
    }
    write(STDOUT_FILENO, CLEAR_SCREEN CURSOR_RESET, sizeof(CLEAR_SCREEN CURSOR_RESET) - 1);
    clearTerminal();
    exit(0);
}

void editorInsertRow(int at, const char *str, size_t len) {
    if (at < 0 || at > E.buf.num_rows) return;

    if (E.buf.num_rows >= E.buf.row_capacity) {
        int new_capacity = E.buf.row_capacity == 0 ? 32 : E.buf.row_capacity * 2;
        E.buf.rows = safeRealloc(E.buf.rows, sizeof(editorRow) * new_capacity);
        E.buf.row_capacity = new_capacity;
    }
    memmove(&E.buf.rows[at + 1], &E.buf.rows[at], sizeof(editorRow) * (E.buf.num_rows - at));

    E.buf.rows[at].size = len;
    E.buf.rows[at].chars = safeMalloc(len + 1);
    memcpy(E.buf.rows[at].chars, str, len);
    E.buf.rows[at].chars[len] = '\0';

    E.buf.rows[at].rsize = 0;
    E.buf.rows[at].render = NULL;
    editorUpdateRow(&E.buf.rows[at]);

    E.buf.num_rows++;
    E.buf.dirty = true;
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
        else {
            if (is_cntrl(row->chars[i]))
                row->render[idx++] = '?';
            else
                row->render[idx++] = row->chars[i];
        }
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
    E.buf.dirty = true;
}

void editorRowDeleteChar(editorRow *row, int at) {
    if (at < 0 || at >= row->size) return;

    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editorUpdateRow(row);
    E.buf.dirty = true;
}

void editorFreeRow(editorRow *row) {
    free(row->chars);
    free(row->render);
}

void editorDeleteRow(int at) {
    if (at < 0 || at >= E.buf.num_rows) return;

    editorFreeRow(&E.buf.rows[at]);
    memmove(&E.buf.rows[at], &E.buf.rows[at + 1], sizeof(editorRow) * (E.buf.num_rows - at - 1));
    E.buf.num_rows--;
    E.buf.dirty = true;
}

void editorRowAppendString(editorRow *row, const char *str, size_t len) {
    row->chars = safeRealloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], str, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    E.buf.dirty = true;
}

void editorMoveRowUp() {
    if (E.cursor.y <= 0 || E.cursor.y >= E.buf.num_rows) return;

    editorRow temp = E.buf.rows[E.cursor.y - 1];
    E.buf.rows[E.cursor.y - 1] = E.buf.rows[E.cursor.y];
    E.buf.rows[E.cursor.y] = temp;

    E.cursor.y--;
    E.buf.dirty = true;
}

void editorMoveRowDown() {
    if (E.cursor.y < 0 || E.cursor.y >= E.buf.num_rows - 1) return;

    editorRow temp = E.buf.rows[E.cursor.y + 1];
    E.buf.rows[E.cursor.y + 1] = E.buf.rows[E.cursor.y];
    E.buf.rows[E.cursor.y] = temp;

    E.cursor.y++;
    E.buf.dirty = true;
}

void editorCopyRowUp() {
    if (E.cursor.y >= E.buf.num_rows) return;

    editorRow *row = &E.buf.rows[E.cursor.y];
    editorInsertRow(E.cursor.y, row->chars, row->size);
    E.buf.dirty = true;
}

void editorCopyRowDown() {
    if (E.cursor.y >= E.buf.num_rows) return;

    editorRow *row = &E.buf.rows[E.cursor.y];
    editorInsertRow(++E.cursor.y, row->chars, row->size);
    E.buf.dirty = true;
}

void editorInsertChar(int ch) {
    if (E.sel.active) {
        saveEditorStateForUndo();
        editorDeleteSelectedText();
        E.sel.active = false;
    }

    if (E.cursor.y == E.buf.num_rows)
        editorInsertRow(E.buf.num_rows, "", 0);
    editorRowInsertChar(&E.buf.rows[E.cursor.y], E.cursor.x, ch);
    E.cursor.x++;

    char closing_char = getClosingChar(ch);
    if (!E.sel.is_pasting && closing_char)
        editorRowInsertChar(&E.buf.rows[E.cursor.y], E.cursor.x, closing_char);
    E.cursor.preferred_x = E.cursor.x;
    E.buf.dirty = true;
}

void editorDeleteChar(int is_backspace) {
    if (E.sel.active) {
        editorDeleteSelectedText();
        return;
    }

    if (E.cursor.y == E.buf.num_rows) return;
    if (E.cursor.x == 0 && E.cursor.y == 0) return;

    editorRow *row = &E.buf.rows[E.cursor.y];
    if (E.cursor.x > 0) {
        char prev_char = row->chars[E.cursor.x - 1];
        char next_char = (E.cursor.x < row->size) ? row->chars[E.cursor.x] : '\0';
        char closing_char = getClosingChar(prev_char);

        if (is_backspace) {
            int spaces = 0;
            while (spaces < TAB_SIZE && (E.cursor.x - spaces - 1) >= 0 && row->chars[E.cursor.x - spaces - 1] == ' ')
                spaces++;

            int only_spaces = 1;
            for (int i = 0; i < E.cursor.x; i++) {
                if (row->chars[i] != ' ') {
                    only_spaces = 0;
                    break;
                }
            }

            if (spaces > 0 && (E.cursor.x % TAB_SIZE == 0) && only_spaces) {
                for (int i = 0; i < spaces; i++)
                    editorRowDeleteChar(row, --E.cursor.x);
                E.cursor.preferred_x = E.cursor.x;
                E.buf.dirty = true;
                return;
            }
        }

        if (is_backspace && closing_char && next_char == closing_char) {
            editorRowDeleteChar(row, E.cursor.x);
            editorRowDeleteChar(row, E.cursor.x - 1);
            E.cursor.x--;
            E.cursor.preferred_x = E.cursor.x;
            E.buf.dirty = true;
            return;
        }

        editorRowDeleteChar(row, --E.cursor.x);
    } else {
        E.cursor.x = E.buf.rows[E.cursor.y - 1].size;
        editorRowAppendString(&E.buf.rows[E.cursor.y - 1], row->chars, row->size);
        editorDeleteRow(E.cursor.y--);
    }
    E.cursor.preferred_x = E.cursor.x;
    E.buf.dirty = true;
}

void editorInsertNewline() {
    editorRow *row = (E.cursor.y >= E.buf.num_rows) ? NULL : &E.buf.rows[E.cursor.y];
    int indent_len = 0;
    while (indent_len < row->size && (row->chars[indent_len] == ' ' || row->chars[indent_len] == '\t'))
        indent_len++;
    if (indent_len > E.cursor.x)
        indent_len = E.cursor.x;
    if (E.sel.is_pasting)
        indent_len = 0;

    char *indent_str = safeMalloc(indent_len + 1);
    memcpy(indent_str, row->chars, indent_len);
    indent_str[indent_len] = '\0';

    if (E.cursor.x == 0)
        editorInsertRow(E.cursor.y, indent_str, indent_len);
    else {
        editorInsertRow(E.cursor.y + 1, &row->chars[E.cursor.x], row->size - E.cursor.x);
        row = &E.buf.rows[E.cursor.y];
        row->size = E.cursor.x;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);

        editorRow *new_row = &E.buf.rows[E.cursor.y + 1];
        new_row->chars = safeRealloc(new_row->chars, new_row->size + indent_len + 1);

        memmove(new_row->chars + indent_len, new_row->chars, new_row->size + 1);
        memcpy(new_row->chars, indent_str, indent_len);
        new_row->size += indent_len;
        editorUpdateRow(new_row);

        if (E.cursor.x > 0 && (row->chars[E.cursor.x - 1] == '{' || row->chars[E.cursor.x - 1] == '[' || row->chars[E.cursor.x - 1] == '(') && new_row->size > 0 && new_row->chars[indent_len] == getClosingChar(row->chars[E.cursor.x - 1])) {
            int new_indent_len = indent_len + TAB_SIZE;
            char *block_indent = safeMalloc(new_indent_len + 1);
            memset(block_indent, ' ', new_indent_len);
            block_indent[new_indent_len] = '\0';
            editorInsertRow(E.cursor.y + 1, block_indent, new_indent_len);
            free(block_indent);

            E.cursor.y++;
            E.cursor.x = new_indent_len;
            E.cursor.preferred_x = E.cursor.x;

            free(indent_str);
            return;
        }
    }

    free(indent_str);
    E.cursor.y++;
    E.cursor.x = indent_len;
    E.cursor.preferred_x = E.cursor.x;
}

void editorSelectText(int ch) {
    if (!E.sel.active) {
        E.sel.active = true;
        E.sel.sx = E.cursor.x;
        E.sel.sy = E.cursor.y;
    }
    editorMoveCursor(ch == SHIFT_ARROW_LEFT ? ARROW_LEFT : ch == SHIFT_ARROW_RIGHT ? ARROW_RIGHT : ch == SHIFT_ARROW_UP ? ARROW_UP : ARROW_DOWN);
    E.sel.ex = E.cursor.x;
    E.sel.ey = E.cursor.y;
}

void editorSelectAll() {
    if (E.buf.num_rows > 0) {
        E.sel.active = true;
        E.sel.sx = 0;
        E.sel.sy = 0;
        E.sel.ex = E.buf.rows[E.buf.num_rows - 1].size;
        E.sel.ey = E.buf.num_rows - 1;

        E.cursor.x = E.sel.ex;
        E.cursor.y = E.sel.ey;

        char msg[STATUS_LENGTH];
        snprintf(msg, sizeof(msg), "Selected all %d lines", E.buf.num_rows);
        editorSetStatusMsg(msg);
    }
}

char *editorGetSelectedText(int *len_out) {
    if (!E.sel.active) {
        if (len_out) *len_out = 0;
        return NULL;
    }

    int x1 = E.sel.sx, y1 = E.sel.sy;
    int x2 = E.sel.ex, y2 = E.sel.ey;
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
        int endx = (row == y2) ? x2 : E.buf.rows[row].size;
        if (endx > startx)
            total += (endx - startx);
        if (row != y2)
            total++;
    }

    if (len_out) *len_out = total;
    char *buf = safeMalloc(total + 1);
    char *ptr = buf;
    for (int row = y1; row <= y2; row++) {
        int startx = (row == y1) ? x1 : 0;
        int endx = (row == y2) ? x2 : E.buf.rows[row].size;
        if (endx > startx) {
            memcpy(ptr, &E.buf.rows[row].chars[startx], endx - startx);
            ptr += (endx - startx);
        }
        if (row != y2)
            *ptr++ = '\n';
    }
    *ptr = '\0';
    return buf;
}

void editorDeleteSelectedText() {
    if (!E.sel.active) return;

    int y1 = E.sel.sy, x1 = E.sel.sx;
    int y2 = E.sel.ey, x2 = E.sel.ex;
    if (y1 > y2 || (y1 == y2 && x1 > x2)) {
        int tmpx = x1, tmpy = y1;
        x1 = x2;
        y1 = y2;
        x2 = tmpx;
        y2 = tmpy;
    }

    if (y1 == y2) {
        memmove(&E.buf.rows[y1].chars[x1], &E.buf.rows[y1].chars[x2], E.buf.rows[y1].size - x2);
        E.buf.rows[y1].size -= (x2 - x1);
        E.buf.rows[y1].chars[E.buf.rows[y1].size] = '\0';
        editorUpdateRow(&E.buf.rows[y1]);
    } else {
        E.buf.rows[y1].size = x1;
        E.buf.rows[y1].chars[x1] = '\0';
        editorUpdateRow(&E.buf.rows[y1]);

        memmove(E.buf.rows[y2].chars, &E.buf.rows[y2].chars[x2], E.buf.rows[y2].size - x2);
        E.buf.rows[y2].size -= x2;
        E.buf.rows[y2].chars[E.buf.rows[y2].size] = '\0';
        editorUpdateRow(&E.buf.rows[y2]);

        editorRowAppendString(&E.buf.rows[y1], E.buf.rows[y2].chars, E.buf.rows[y2].size);
        editorDeleteRow(y2);

        for (int row = y2 - 1; row > y1; row--)
            editorDeleteRow(row);
    }

    E.cursor.x = x1;
    E.cursor.y = y1;
    E.cursor.preferred_x = E.cursor.x;

    E.sel.active = false;
    E.buf.dirty = true;
}

void editorFind() {
    E.sys.has_bracket = false;
    int saved_cursor_x = E.cursor.x;
    int saved_cursor_y = E.cursor.y;
    int saved_col_offset = E.view.col_offset;
    int saved_row_offset = E.view.row_offset;

    char *query = editorPrompt("Search: %s (Use ESC/Arrows/Enter)", editorFindCallback, editorGetSelectedText(NULL));

    if (query) free(query);
    else {
        E.cursor.x = saved_cursor_x;
        E.cursor.preferred_x = E.cursor.x;
        E.cursor.y = saved_cursor_y;
        E.view.col_offset = saved_col_offset;
        E.view.row_offset = saved_row_offset;
        E.find.active = false;
    }
}

void editorFindCallback(const char *query, int key) {
    int direction = 1;

    if (key == '\r' || key == ESCAPE_CHAR || query[0] == '\0') {
        if (key == ESCAPE_CHAR)
            editorSetStatusMsg("Find cancelled");
        E.find.active = false;
        free(E.find.query);
        E.find.query = NULL;
        free(E.find.match_lines);
        E.find.match_lines = NULL;
        free(E.find.match_cols);
        E.find.match_cols = NULL;
        E.find.num_matches = 0;
        E.find.current_idx = -1;
        return;
    }

    if (key == ARROW_RIGHT || key == ARROW_DOWN) direction = 1;
    else if (key == ARROW_LEFT || key == ARROW_UP) direction = -1;
    else if (key == BACKSPACE) direction = 1;
    else if (is_cntrl(key)) return;
    else direction = 1;

    if (E.find.query == NULL || strcmp(E.find.query, query) != 0) {
        free(E.find.query);
        E.find.query = safeStrdup(query);

        free(E.find.match_lines);
        free(E.find.match_cols);
        E.find.match_lines = NULL;
        E.find.match_cols = NULL;
        E.find.num_matches = 0;
        E.find.current_idx = -1;

        for (int i = 0; i < E.buf.num_rows; i++) {
            char *line = E.buf.rows[i].chars;
            char *ptr = line;
            while ((ptr = strstr(ptr, query)) != NULL) {
                E.find.match_lines = safeRealloc(E.find.match_lines, sizeof(int) * (E.find.num_matches + 1));
                E.find.match_cols = safeRealloc(E.find.match_cols, sizeof(int) * (E.find.num_matches + 1));

                E.find.match_lines[E.find.num_matches] = i;
                E.find.match_cols[E.find.num_matches] = ptr - line;
                E.find.num_matches++;

                ptr += strlen(query);
            }
        }

        if (E.find.num_matches > 0) {
            E.find.current_idx = 0;
            int row = E.find.match_lines[0];
            int col = E.find.match_cols[0];
            E.cursor.x = col;
            E.cursor.y = row;
            E.view.row_offset = E.buf.num_rows;
            E.cursor.preferred_x = E.cursor.x;

            if (row < E.view.row_offset)
                E.view.row_offset = row;
            else if (row >= E.view.row_offset + E.view.screen_rows)
                E.view.row_offset = row - E.view.screen_rows + MARGIN;

            int render_pos = editorRowCxToRx(&E.buf.rows[row], E.cursor.x);
            int margin = strlen(query) + MARGIN;
            if (render_pos < E.view.col_offset)
                E.view.col_offset = render_pos;
            else if (render_pos >= E.view.col_offset + E.view.screen_cols - 1 && render_pos > margin)
                E.view.col_offset = render_pos - (E.view.screen_cols - margin);
            if (E.view.col_offset < 0) E.view.col_offset = 0;
        }
        E.find.active = true;
    } else {
        if (E.find.num_matches > 0) {
            E.find.current_idx += direction;
            if (E.find.current_idx < 0) E.find.current_idx = E.find.num_matches - 1;
            else if (E.find.current_idx >= E.find.num_matches) E.find.current_idx = 0;

            int row = E.find.match_lines[E.find.current_idx];
            int col = E.find.match_cols[E.find.current_idx];
            E.cursor.x = col;
            E.cursor.y = row;
            E.cursor.preferred_x = E.cursor.x;

            if (row < E.view.row_offset)
                E.view.row_offset = row;
            else if (row >= E.view.row_offset + E.view.screen_rows)
                E.view.row_offset = row - E.view.screen_rows + MARGIN;

            int render_pos = editorRowCxToRx(&E.buf.rows[row], E.cursor.x);
            int margin = strlen(query) + MARGIN;
            if (render_pos < E.view.col_offset)
                E.view.col_offset = render_pos;
            else if (render_pos >= E.view.col_offset + E.view.screen_cols - 1 && render_pos > margin)
                E.view.col_offset = render_pos - (E.view.screen_cols - margin);
            if (E.view.col_offset < 0) E.view.col_offset = 0;
        }
    }
}

void editorScanLineMatches(int line, const char *query) {
    int new_num_matches = 0;

    int max_matches = E.find.num_matches + 50;
    int *new_lines = safeMalloc(sizeof(int) * max_matches);
    int *new_cols = safeMalloc(sizeof(int) * max_matches);

    for (int i = 0; i < E.find.num_matches; i++) {
        if (E.find.match_lines[i] != line) {
            new_lines[new_num_matches] = E.find.match_lines[i];
            new_cols[new_num_matches] = E.find.match_cols[i];
            new_num_matches++;
        }
    }

    if (line < E.buf.num_rows) {
        char *render_line = E.buf.rows[line].chars;
        int query_len = strlen(query);
        char *ptr = render_line;
        while ((ptr = strstr(ptr, query)) != NULL) {
            if (new_num_matches >= max_matches) {
                max_matches *= 2;
                new_lines = safeRealloc(new_lines, sizeof(int) * max_matches);
                new_cols = safeRealloc(new_cols, sizeof(int) * max_matches);
            }
            
            new_lines[new_num_matches] = line;
            new_cols[new_num_matches] = ptr - render_line;
            new_num_matches++;
            ptr += query_len;
        }
    }

    free(E.find.match_lines);
    free(E.find.match_cols);
    E.find.match_lines = new_lines;
    E.find.match_cols = new_cols;
    E.find.num_matches = new_num_matches;
}

void editorReplace() {
    int saved_cursor_x = E.cursor.x;
    int saved_cursor_y = E.cursor.y;
    int saved_col_offset = E.view.col_offset;
    int saved_row_offset = E.view.row_offset;

    char *find_query = editorPrompt("Replace - Find: %s (ESC to cancel)", editorReplaceCallback, editorGetSelectedText(NULL));
    if (!find_query || strlen(find_query) == 0 || E.find.num_matches == 0) {
        editorSetStatusMsg("Replace cancelled");
        E.cursor.x = saved_cursor_x;
        E.cursor.y = saved_cursor_y;
        E.view.col_offset = saved_col_offset;
        E.view.row_offset = saved_row_offset;
        E.find.active = false;
        free(find_query);
        return;
    }

    char *replace_query = editorPrompt("Replace - With: %s (ESC to cancel)", NULL, NULL);
    if (!replace_query) {
        editorSetStatusMsg("Replace cancelled");
        free(find_query);
        free(replace_query);
        free(E.find.query);
        free(E.find.match_lines);
        free(E.find.match_cols);
        E.find.query = NULL;
        E.find.match_lines = NULL;
        E.find.match_cols = NULL;
        E.find.num_matches = 0;
        E.find.current_idx = -1;
        E.find.active = false;
        return;
    }

    bool first = true;
    int replaced = 0;
    bool done = false;
    int idx = E.find.current_idx < 0 ? 0 : E.find.current_idx;
    while (!done && E.find.num_matches > 0) {
        editorReplaceJumpToCurrent();
        editorRefreshScreen();
        editorSetStatusMsg("Arrows: navigate, Enter: replace, A: all, ESC: cancel");
        editorRefreshScreen();

        int key = editorReadKey();
        switch (key) {
            case ESCAPE_CHAR:
                done = true;
                E.sel.active = false;
                break;
            case ARROW_DOWN:
            case ARROW_RIGHT:
                idx = (idx + 1) % E.find.num_matches;
                E.find.current_idx = idx;
                editorReplaceJumpToCurrent();
                break;
            case ARROW_UP:
            case ARROW_LEFT:
                idx = (idx - 1 + E.find.num_matches) % E.find.num_matches;
                E.find.current_idx = idx;
                editorReplaceJumpToCurrent();
                break;
            case 'a':
            case 'A':
                if (first) {
                    saveEditorStateForUndo();
                    first = false;
                }
                replaced += editorReplaceAll(replace_query);
                done = true;
                break;
            case '\r':
            case '\n':
                if (first) {
                    saveEditorStateForUndo();
                    first = false;
                }
                if (editorReplaceCurrent(find_query, replace_query)) {
                    replaced++;
                    editorScanLineMatches(E.cursor.y, find_query);
                    if (E.find.num_matches == 0) {
                        done = true;
                    } else {
                        int line = E.cursor.y;
                        int next_idx = -1;
                        int cursor_pos = E.cursor.x;
                        for (int i = 0; i < E.find.num_matches; i++) {
                            if (E.find.match_lines[i] == line && E.find.match_cols[i] > cursor_pos) {
                                next_idx = i;
                                break;
                            }
                        }
                        if (next_idx == -1) {
                            for (int i = 0; i < E.find.num_matches; i++) {
                                if (E.find.match_lines[i] > line) {
                                    next_idx = i;
                                    break;
                                }
                            }
                        }

                        if (next_idx == -1)
                            next_idx = 0;
                        E.find.current_idx = next_idx;
                        editorReplaceJumpToCurrent();
                    }
                }
                editorReplaceCallback(find_query, 0);
                break;
            default:
                break;
        }
        char msg[STATUS_LENGTH];
        snprintf(msg, sizeof(msg), "Replaced %d occurrence%s", replaced, replaced == 1 ? "" : "s");
        editorSetStatusMsg(msg);
    }

    free(replace_query);
    free(find_query);
    free(E.find.query);
    free(E.find.match_lines);
    free(E.find.match_cols);
    E.find.query = NULL;
    E.find.match_lines = NULL;
    E.find.match_cols = NULL;
    E.find.num_matches = 0;
    E.find.current_idx = -1;
    E.find.active = false;
}

void editorReplaceCallback(const char *query, int key) {
    (void)key;
    if (query == NULL || !query[0]) {
        E.find.active = false;
        free(E.find.query);
        E.find.query = NULL;
        free(E.find.match_lines);
        E.find.match_lines = NULL;
        free(E.find.match_cols);
        E.find.match_cols = NULL;
        E.find.num_matches = 0;
        E.find.current_idx = -1;
        return;
    }

    if (E.find.query == NULL || strcmp(E.find.query, query) != 0) {
        free(E.find.query);
        E.find.query = safeStrdup(query);

        free(E.find.match_lines);
        free(E.find.match_cols);
        E.find.match_lines = NULL;
        E.find.match_cols = NULL;
        E.find.num_matches = 0;
        E.find.current_idx = -1;

        int query_len = strlen(query);
        for (int i = 0; i < E.buf.num_rows; i++) {
            char *line = E.buf.rows[i].render;
            char *ptr = line;
            while ((ptr = strstr(ptr, query)) != NULL) {
                E.find.match_lines = safeRealloc(E.find.match_lines, sizeof(int) * (E.find.num_matches + 1));
                E.find.match_cols = safeRealloc(E.find.match_cols, sizeof(int) * (E.find.num_matches + 1));
                E.find.match_lines[E.find.num_matches] = i;
                E.find.match_cols[E.find.num_matches] = ptr - line;
                E.find.num_matches++;
                ptr += query_len;
            }
        }

        if (E.find.num_matches > 0) {
            E.find.current_idx = 0;
            int row = E.find.match_lines[0];
            int col = E.find.match_cols[0];
            E.cursor.y = row;
            E.cursor.x = col;
            E.view.row_offset = (row >= E.view.screen_rows) ? row - E.view.screen_rows + MARGIN : 0;
            int render_pos = editorRowCxToRx(&E.buf.rows[row], E.cursor.x);
            int margin = query_len + MARGIN;
            E.view.col_offset = (render_pos > margin) ? render_pos - (E.view.screen_cols - margin) : 0;
            if (E.view.col_offset < 0)
                E.view.col_offset = 0;
        }
        E.find.active = true;
    }
}

int editorReplaceAll(const char *replace_str) {
    char *find_str = E.find.query;
    int find_len = strlen(find_str);
    int replace_len = strlen(replace_str);
    int replaced_count = 0;

    for (int i = 0; i < E.buf.num_rows; i++) {
        editorRow *row = &E.buf.rows[i];

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
    if (E.find.num_matches > 0 && E.find.current_idx >= 0 && E.find.current_idx < E.find.num_matches) {
        int row = E.find.match_lines[E.find.current_idx];
        int col = E.find.match_cols[E.find.current_idx];
        E.cursor.x = col;
        E.cursor.y = row;
        E.cursor.preferred_x = E.cursor.x;
        E.view.row_offset = (row >= E.view.screen_rows) ? row - E.view.screen_rows + MARGIN : 0;
        int render_pos = editorRowCxToRx(&E.buf.rows[row], E.cursor.x);
        int margin = strlen(E.find.query) + MARGIN;
        E.view.col_offset = (render_pos > margin) ? render_pos - (E.view.screen_cols - margin) : 0;
        if (E.view.col_offset < 0)
            E.view.col_offset = 0;
    }
}

bool editorReplaceCurrent(const char *find_str, const char *replace_str) {
    if (!find_str || !replace_str || E.find.num_matches == 0 || E.find.current_idx < 0) return false;

    int row = E.find.match_lines[E.find.current_idx];
    int col = E.find.match_cols[E.find.current_idx];
    editorRow *er = &E.buf.rows[row];
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
    E.buf.dirty = true;
    E.cursor.y = row;
    E.cursor.x = cx + replace_len;
    E.cursor.preferred_x = E.cursor.x;
    return true;
}

void clipboardCopyToSystem(const char *data, int len) {
    if (!data || len <= 0)
        return;

    #ifdef __APPLE__                                 // MacOS
        if (getenv("SSH_TTY") == NULL) {
            FILE *pipe = popen("pbcopy 2>/dev/null", "w");
            if (pipe != NULL) {
                fwrite(data, 1, len, pipe);
                pclose(pipe);
                return; 
            }
        }
    #endif

    if (getenv("SSH_TTY") == NULL) {
        if (getenv("WAYLAND_DISPLAY")) {            // Wayland
            FILE *pipe = popen("wl-copy 2>/dev/null", "w");
            if (pipe) {
                fwrite(data, 1, len, pipe);
                if (pclose(pipe) == 0) return;
            }
        }
        if (getenv("DISPLAY")) {                    // X11
            FILE *pipe = popen("xclip -selection clipboard 2>/dev/null", "w");
            if (pipe) {
                fwrite(data, 1, len, pipe);
                if (pclose(pipe) == 0) return;
            }
        }
    }

    size_t output_len = 4 * ((len + 2) / 3);        // Fallback - OSC 52
    char *b64_data = safeMalloc(output_len + 1);
    base64_encode(data, len, b64_data);

    char header[] = "\033]52;c;";
    char footer[] = "\007";

    if (write(STDOUT_FILENO, header, sizeof(header) - 1) == -1) {}
    if (write(STDOUT_FILENO, b64_data, output_len) == -1) {}
    if (write(STDOUT_FILENO, footer, sizeof(footer) - 1) == -1) {}

    free(b64_data);
}

void editorCopySelection() {
    if (!E.sel.active) {
        editorSetStatusMsg("No selection to copy");
        return;
    }

    free(E.sel.clipboard);
    int len;
    E.sel.clipboard = editorGetSelectedText(&len);

    if (E.sel.clipboard) {
        char sizebuf[SMALL_BUFFER_SIZE];
        humanReadableSize(len, sizebuf, sizeof(sizebuf));

        char msg[STATUS_LENGTH];
        snprintf(msg, sizeof(msg), "Copied %s", sizebuf);
        editorSetStatusMsg(msg);
        clipboardCopyToSystem(E.sel.clipboard, len);
    }
}

void editorCutSelection() {
    if (!E.sel.active) {
        editorSetStatusMsg("No selection to cut");
        return;
    }

    free(E.sel.clipboard);
    int len;
    E.sel.clipboard = editorGetSelectedText(&len);
    if (!E.sel.clipboard) return;

    clipboardCopyToSystem(E.sel.clipboard, len);
    editorDeleteSelectedText();

    char sizebuf[SMALL_BUFFER_SIZE];
    humanReadableSize(strlen(E.sel.clipboard), sizebuf, sizeof(sizebuf));

    char msg[STATUS_LENGTH];
    snprintf(msg, sizeof(msg), "Cut %s", sizebuf);
    editorSetStatusMsg(msg);
}

void editorCutLine() {
    if (E.cursor.y >= E.buf.num_rows) return;

    editorRow *current_row = &E.buf.rows[E.cursor.y];
    int line_length = current_row->size;
    char *line_content = safeMalloc(line_length + 2);
    memcpy(line_content, current_row->chars, line_length);
    line_content[line_length] = '\n';
    line_content[line_length + 1] = '\0';

    free(E.sel.clipboard);
    E.sel.clipboard = line_content;
    clipboardCopyToSystem(E.sel.clipboard, line_length + 1);
    editorDeleteRow(E.cursor.y);

    if (E.cursor.y >= E.buf.num_rows && E.buf.num_rows > 0)
        E.cursor.y = E.buf.num_rows - 1;

    if (E.cursor.y >= 0 && E.cursor.y < E.buf.num_rows) {
        int row_len = E.buf.rows[E.cursor.y].size;
        E.cursor.x = E.cursor.x > row_len ? row_len : E.cursor.x;
    }
    else
        E.cursor.x = 0;
    E.cursor.preferred_x = E.cursor.x;

    char sizebuf[SMALL_BUFFER_SIZE];
    humanReadableSize(strlen(E.sel.clipboard), sizebuf, sizeof(sizebuf));

    char msg[STATUS_LENGTH];
    snprintf(msg, sizeof(msg), "Cut %s", sizebuf);
    editorSetStatusMsg(msg);
    E.buf.dirty = true;
}

void editorJump() {
    int saved_cursor_x = E.cursor.x;
    int saved_cursor_y = E.cursor.y;
    int saved_col_offset = E.view.col_offset;
    int saved_row_offset = E.view.row_offset;

    char *input = editorPrompt("Jump to (row:col): %s (ESC to cancel)", editorJumpCallback, NULL);

    if (input) {
        free(input);
        if (E.cursor.x == saved_cursor_x && E.cursor.y == saved_cursor_y && E.view.col_offset == saved_col_offset && E.view.row_offset == saved_row_offset)
            editorSetStatusMsg("Invalid input");
        else
            editorSetStatusMsg("Jumped");
    } else {
        E.cursor.x = saved_cursor_x;
        E.cursor.y = saved_cursor_y;
        E.view.col_offset = saved_col_offset;
        E.view.row_offset = saved_row_offset;
        editorSetStatusMsg("Jump cancelled");
    }
}

void editorJumpCallback(const char *buf, int key) {
    if (key == '\r' || key == ESCAPE_CHAR)
        return;

    int row = 0, col = 1;
    if (sscanf(buf, "%d:%d", &row, &col) != 2)
        sscanf(buf, "%d", &row);

    row = row > 0 ? row - 1 : 0;
    col = col > 0 ? col - 1 : 0;

    if (row >= E.buf.num_rows) row = E.buf.num_rows - 1;
    if (row < 0) row = 0;
    if (col < 0) col = 0;
    if (row >= 0 && row < E.buf.num_rows && col > E.buf.rows[row].size) col = E.buf.rows[row].size;

    E.cursor.x = col;
    E.cursor.y = row;
    E.cursor.preferred_x = col;
    editorScroll();
}

void freeEditorState(editorState *state) {
    if (state->buffer) free(state->buffer);
    state->buffer = NULL;
}

void saveEditorStateForUndo() {
    long now = currentMillis();

    int should_save = !history.undo_in_progress || (now - history.last_edit_time > UNDO_TIMEOUT);
    if (!should_save) {
        history.last_edit_time = now;
        return;
    }

    for (int i = 0; i <= history.redo_top; i++)
        freeEditorState(&history.redo_stack[i]);
    history.redo_top = -1;

    if (history.undo_top >= UNDO_REDO_STACK_SIZE - 1) {
        freeEditorState(&history.undo_stack[0]);
        memmove(&history.undo_stack[0], &history.undo_stack[1], sizeof(editorState) * (UNDO_REDO_STACK_SIZE - 1));
        history.undo_stack[history.undo_top].buffer = NULL;
        history.undo_top--; 
    }

    history.undo_top++;
    editorState *state = &history.undo_stack[history.undo_top];
    freeEditorState(state);

    state->buffer = editorRowsToString(&state->buf_len);
    state->cursor = E.cursor;
    state->sel = E.sel;

    history.undo_in_progress = 1;
    history.last_edit_time = now;
}

void restoreEditorState(const editorState *state) {
    for (int i = 0; i < E.buf.num_rows; i++)
        editorFreeRow(&E.buf.rows[i]);
    free(E.buf.rows);
    E.buf.rows = NULL;
    E.buf.num_rows = 0;
    E.buf.row_capacity = 0;

    size_t start = 0;
    for (size_t i = 0; i < state->buf_len; i++) {
        if (state->buffer[i] == '\n') {
            editorInsertRow(E.buf.num_rows, &state->buffer[start], i - start);
            start = i + 1;
        }
    }

    char *saved_clipboard = E.sel.clipboard;
    E.sel = state->sel;
    E.sel.clipboard = saved_clipboard;
    E.cursor = state->cursor;
    E.buf.dirty = true;
}

void editorUndo() {
    if (history.undo_top < 0) {
        editorSetStatusMsg("Nothing to undo");
        return;
    }

    if (history.redo_top < UNDO_REDO_STACK_SIZE - 1) {
        history.redo_top++;
        editorState *state = &history.redo_stack[history.redo_top];
        freeEditorState(state);

        state->buffer = editorRowsToString(&state->buf_len);
        state->cursor = E.cursor;
        state->sel = E.sel;
    }

    editorState *undo_st = &history.undo_stack[history.undo_top];
    restoreEditorState(undo_st);
    freeEditorState(undo_st);
    history.undo_top--;
    editorSetStatusMsg("Undid");
}

void editorRedo() {
    if (history.redo_top < 0) {
        editorSetStatusMsg("Nothing to redo");
        return;
    }

    if (history.undo_top < UNDO_REDO_STACK_SIZE - 1) {
        history.undo_top++;
        editorState *state = &history.undo_stack[history.undo_top];
        freeEditorState(state);

        state->buffer = editorRowsToString(&state->buf_len);
        state->cursor = E.cursor;
        state->sel = E.sel;
    }

    editorState *redo_st = &history.redo_stack[history.redo_top];
    restoreEditorState(redo_st);
    freeEditorState(redo_st);
    history.redo_top--;
    editorSetStatusMsg("Redid");
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

bool findMatchingBracketPosition(int cursor_y, int cursor_x, int *match_y, int *match_x) {
    if (cursor_y >= E.buf.num_rows || cursor_x >= E.buf.rows[cursor_y].size)
        return false;

    char bracket = E.buf.rows[cursor_y].chars[cursor_x];
    if (!(bracket == '(' || bracket == ')' || bracket == '{' || bracket == '}' || bracket == '[' || bracket == ']'))
        return false;

    char match = getMatchingBracket(bracket);
    int direction = (bracket == '(' || bracket == '{' || bracket == '[') ? 1 : -1;
    int count = 1;

    int y = cursor_y;
    int x = cursor_x;
    while (1) {
        if (direction == 1) {
            x++;
            while (y < E.buf.num_rows && x >= E.buf.rows[y].size) {
                y++;
                x = 0;
            }
            if (y >= E.buf.num_rows) break;
        } else {
            x--;
            while (y >= 0 && x < 0) {
                y--;
                if (y >= 0) x = E.buf.rows[y].size - 1;
            }
            if (y < 0) break;
        }

        char ch = E.buf.rows[y].chars[x];
        if (ch == bracket)
            count++;
        else if (ch == match)
            count--;

        if (count == 0) {
            *match_y = y;
            *match_x = x;
            return true;
        }
    }

    return false;
}

void updateMatchBracket() {
    int mx, my;
    if (findMatchingBracketPosition(E.cursor.y, E.cursor.x, &my, &mx)) {
        E.sys.bracket_x = mx;
        E.sys.bracket_y = my;
        E.sys.has_bracket = true;
    } else {
        E.sys.has_bracket = false;
    }
}

void editorMouseLeftClick() {
    clampCursorPosition();
    E.sel.active = true;
    E.sel.sx = E.cursor.x;
    E.sel.sy = E.cursor.y;
    E.sel.ex = E.cursor.x;
    E.sel.ey = E.cursor.y;
    E.cursor.preferred_x = E.cursor.x;
}

void editorMouseDragClick() {
    clampCursorPosition();
    E.sel.ex = E.cursor.x;
    E.sel.ey = E.cursor.y;
    E.cursor.preferred_x = E.cursor.x;
}

void editorMouseLeftRelease() {
    clampCursorPosition();
    if (E.sel.ex == E.sel.sx && E.sel.ey == E.sel.sy)
        E.sel.active = false;
}
