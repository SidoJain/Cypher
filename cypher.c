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

#define CYPHER_VERSION "1.0.0"
#define EMPTY_LINE_SYMBOL "~"

#define CTRL_KEY(k)         ((k) & 0x1f)
#define APPEND_BUFFER_INIT  {NULL, 0}

#define	STDIN_FILENO	0
#define	STDOUT_FILENO	1
#define	STDERR_FILENO	2

#define TAB_SIZE                4
#define QUIT_TIMES              2
#define UNDO_REDO_STACK_SIZE    100

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
#define YELLOW_COLOR            "\x1b[33m"

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
    CTRL_SHIFT_ARROW_UP,
    CTRL_SHIFT_ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
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
    int screen_rows;
    int screen_cols;
    int row_offset;
    int col_offset;
    int num_rows;
    editorRow *row;
    int dirty;
    char *filename;
    char status_msg[80];
    time_t status_msg_time;
    struct termios original_termios;
    int select_mode;
    int select_sx;
    int select_sy;
    int select_ex;
    int select_ey;
    char *clipboard;
    char *find_query;
    int *find_match_lines;
    int *find_match_cols;
    int find_num_matches;
    int find_current_idx;
    int find_active;
} editorConfig;

typedef struct {
    char *b;
    int len;
} appendBuffer;

typedef struct {
    char *buffer;
    int buf_len;
    int cursor_x;
    int cursor_y;
    int select_mode;
    int select_sx;
    int select_sy;
    int select_ex;
    int select_ey;
} editorState;

/*** Global Data ***/

editorConfig E;

editorState undo_stack[UNDO_REDO_STACK_SIZE];
int undo_top = -1;
editorState redo_stack[UNDO_REDO_STACK_SIZE];
int redo_top = -1;

/*** Function Prototypes ***/

// utility
int is_word_char(int);

// terminal
void die(const char *);
void enableRawMode();
void disableRawMode();
int editorReadKey();
int getWindowSize(int *, int *);
int getCursorPosition(int *, int *);

// input
void editorProcessKeypress();
void editorMoveCursor(int);
char *editorPrompt(char *, void (*)(char *, int));
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
void editorHelpScreen();

// editor
void editorInit();
void editorCleanup();

// append buffer
void abAppend(appendBuffer *, const char *, int);
void abFree(appendBuffer *);

// file i/o
void editorOpen(char *);
char *editorRowsToString(int *);
void editorSave();

// row operations
void editorInsertRow(int, char *, size_t);
void editorUpdateRow(editorRow *);
int editorRowCxToRx(editorRow *, int);
int editorRowRxToCx(editorRow *, int);
void editorRowInsertChar(editorRow *, int, int);
void editorRowDeleteChar(editorRow *, int);
void editorFreeRow(editorRow *);
void editorDeleteRow(int);
void editorRowAppendString(editorRow *, char *, size_t);

// editor operations
void editorInsertChar(int);
void editorDeleteChar();
void editorInsertNewline();
void editorDeleteSelectedText();

// find operations
void editorFind();
void editorFindCallback(char *, int);

// clipboard operations
void clipboardCopyToSystem(const char *);
char *editorGetSelectedText();
void editorCopySelection();
void editorCutSelection();
void editorPaste();

// jump operations
void editorJump();
void editorJumpCallback(char *, int);

// undo-redo operations
void freeEditorState(editorState *);
void saveEditorStateForUndo();
void restoreEditorState(editorState *);
void editorUndo();
void editorRedo();

/*** Main ***/

int main(int argc, char *argv[]) {
    enableRawMode();
    editorInit();
    if (argc >= 2)
        editorOpen(argv[1]);

    editorSetStatusMsg("HELP: Ctrl-H");

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}

/*** Function Definitions ***/

int is_word_char(int c) {
    return isalnum(c) || c == '_';
}

void die(const char *str) {
    write(STDOUT_FILENO, CLEAR_SCREEN CURSOR_RESET, sizeof(CLEAR_SCREEN CURSOR_RESET) - 1);
    atexit(editorCleanup);

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

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.original_termios) == -1)
        die("tcsetattr");
}

int editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
        if (nread == -1 && errno != EAGAIN)
            die("read");

    if (c == ESCAPE_CHAR) {
        char seq[5] = {0};
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return ESCAPE_CHAR;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return ESCAPE_CHAR;

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return ESCAPE_CHAR;
                if (seq[2] == ';') {
                    if (read(STDIN_FILENO, &seq[3], 1) != 1) return ESCAPE_CHAR;
                    if (read(STDIN_FILENO, &seq[4], 1) != 1) return ESCAPE_CHAR;
                    if (seq[3] == '6') {
                        switch (seq[4]) {
                            case 'A': return CTRL_SHIFT_ARROW_UP;
                            case 'B': return CTRL_SHIFT_ARROW_DOWN;
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
                    if (seq[3] == '2') {
                        switch (seq[4]) {
                            case 'H': return SHIFT_HOME;
                            case 'F': return SHIFT_END;
                        }
                    }
                    if (seq[3] == '2') {
                        switch (seq[4]) {
                            case 'A': return SHIFT_ARROW_UP;
                            case 'B': return SHIFT_ARROW_DOWN;
                            case 'C': return SHIFT_ARROW_RIGHT;
                            case 'D': return SHIFT_ARROW_LEFT;
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
        if (write(STDOUT_FILENO, CURSOR_FORWARD CURSOR_DOWN, sizeof(CURSOR_FORWARD CURSOR_DOWN) - 1) != sizeof(CURSOR_FORWARD CURSOR_DOWN) - 1)
            return -1;
        return getCursorPosition(rows, cols);
    } else {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
        return 0;
    }
}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
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
    int c = editorReadKey();
    static int quit_times = QUIT_TIMES;

    switch (c) {
        case CTRL_KEY('q'):     // quit
            if (E.dirty && quit_times > 0) {
                editorSetStatusMsg("WARNING!!! File has unsaved changes. Press Ctrl-Q %d more times to quit.", quit_times);
                quit_times--;
                return;
            }

            write(STDOUT_FILENO, CLEAR_SCREEN CURSOR_RESET, sizeof(CLEAR_SCREEN CURSOR_RESET) - 1);
            exit(0);
            break;
        
        case CTRL_KEY('h'):     // help
            editorHelpScreen();
            break;

        case CTRL_KEY('s'):     // save
            editorSave();
            break;
        
        case CTRL_KEY('f'):     // find
            editorFind();
            break;

        case CTRL_KEY('c'):     // copy
            editorCopySelection();
            break;

        case CTRL_KEY('x'):     // cut
            editorCutSelection();
            break;
        
        case CTRL_KEY('v'):     // paste
            editorPaste();
            break;

        case CTRL_KEY('a'):     // select all
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
            break;

        case CTRL_KEY('_'):     // jump to
        case CTRL_KEY('g'):
        case CTRL_KEY('l'):
            editorJump();
            break;

        case CTRL_KEY('z'):     // undo
            editorUndo();
            break;

        case CTRL_KEY('y'):     // redo
            editorRedo();
            break;

        case '\r':              // enter
            editorInsertNewline();
            break;

        case '\t':              // tab
            for (int i = 0; i < 4; i++)
                editorInsertChar(' ');
            break;

        case HOME_KEY:
            E.cursor_x = 0;
            break;
        case END_KEY:
            if (E.cursor_y < E.num_rows)
                E.cursor_x = E.row[E.cursor_y].size;
            break;

        case BACKSPACE:
        case DEL_KEY:
            if (c == DEL_KEY && !E.select_mode)
                editorMoveCursor(ARROW_RIGHT);
            if (E.select_mode)
                saveEditorStateForUndo();
            editorDeleteChar();
            break;

        case PAGE_UP:
        case PAGE_DOWN:
            {
                int row_count = E.screen_rows;
                while (row_count--)
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;

        case CTRL_ARROW_LEFT:
            editorMoveWordLeft();
            break;
        case CTRL_ARROW_RIGHT:
            editorMoveWordRight();
            break;
        case CTRL_ARROW_UP:
            editorScrollPageUp(1);
            break;
        case CTRL_ARROW_DOWN:
            editorScrollPageDown(1);
            break;

        case SHIFT_ARROW_LEFT:
        case SHIFT_ARROW_RIGHT:
        case SHIFT_ARROW_UP:
        case SHIFT_ARROW_DOWN:
            if (!E.select_mode) {
                E.select_mode = 1;
                E.select_sx = E.cursor_x;
                E.select_sy = E.cursor_y;
            }
            editorMoveCursor(c == SHIFT_ARROW_LEFT ? ARROW_LEFT : c == SHIFT_ARROW_RIGHT ? ARROW_RIGHT : c == SHIFT_ARROW_UP ? ARROW_UP : ARROW_DOWN);
            E.select_ex = E.cursor_x;
            E.select_ey = E.cursor_y;
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
            break;
        case CTRL_SHIFT_ARROW_UP:
        case CTRL_SHIFT_ARROW_DOWN:
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
            break;

        case ARROW_LEFT:
        case ARROW_RIGHT:
        case ARROW_UP:
        case ARROW_DOWN:
            E.select_mode = 0;
            editorMoveCursor(c);
            break;

        default:
            if (!iscntrl(c))
                editorInsertChar(c);
            break;
    }

    quit_times = QUIT_TIMES;
}

void editorMoveCursor(int key) {
    editorRow *row = (E.cursor_y >= E.num_rows) ? NULL : &E.row[E.cursor_y];

    switch (key) {
        case ARROW_LEFT:
            if (E.cursor_x != 0)
                E.cursor_x--;
            break;
        case ARROW_RIGHT:
            if (row && E.cursor_x < row->size)
                E.cursor_x++;
            break;
        case ARROW_DOWN:
            if (E.cursor_y < E.num_rows)
                E.cursor_y++;
            break;
        case ARROW_UP:
            if (E.cursor_y != 0)
                E.cursor_y--;
            break;
    }

    row = (E.cursor_y >= E.num_rows) ? NULL : &E.row[E.cursor_y];
    int row_len = row ? row->size : 0;
    if (E.cursor_x > row_len)
        E.cursor_x = row_len;
}

char *editorPrompt(char *prompt, void (*callback)(char *, int)) {
    size_t buf_size = 128;
    char *buf = malloc(buf_size);
    if (buf == NULL)
        die("malloc");

    size_t buf_len = 0;
    buf[0] = '\0';

    while (1) {
        editorSetStatusMsg(prompt, buf);
        editorRefreshScreen();

        int c = editorReadKey();
        if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
            if (buf_len != 0)
                buf[--buf_len] = '\0';
        } else if (c == ESCAPE_CHAR) {
            editorSetStatusMsg("");
            if (callback)
                callback(buf, c);
            free(buf);
            return NULL;
        } else if (c == '\r') {
            if (buf_len != 0) {
                editorSetStatusMsg("");
                if (callback)
                    callback(buf, c);
                return buf;
            }
        } else if (!iscntrl(c) && c < 128) {
            if (buf_len == buf_size - 1) {
                buf_size *= 2;
                buf = realloc(buf, buf_size);
                if (buf == NULL)
                    die("realloc");
            }
            buf[buf_len++] = c;
            buf[buf_len] = '\0';
        }

        if (callback)
            callback(buf, c);
    }
}

void editorMoveWordLeft() {
    if (E.cursor_y >= E.num_rows) return;

    while (E.cursor_x > 0 && !is_word_char(E.row[E.cursor_y].chars[E.cursor_x - 1]))
        E.cursor_x--;
    while (E.cursor_x > 0 && is_word_char(E.row[E.cursor_y].chars[E.cursor_x - 1]))
        E.cursor_x--;
}

void editorMoveWordRight() {
    if (E.cursor_y >= E.num_rows) return;

    int len = E.row[E.cursor_y].size;
    while (E.cursor_x < len && !is_word_char(E.row[E.cursor_y].chars[E.cursor_x]))
        E.cursor_x++;
    while (E.cursor_x < len && is_word_char(E.row[E.cursor_y].chars[E.cursor_x]))
        E.cursor_x++;
}

void editorScrollPageUp(int scroll_amount) {
    if (E.row_offset > 0) {
        E.row_offset -= scroll_amount;
        E.cursor_y -= scroll_amount;

        if (E.cursor_y < 0)
            E.cursor_y = 0;

        if (E.cursor_y < E.num_rows) {
            int row_len = E.row[E.cursor_y].size;
            if (E.cursor_x > row_len)
                E.cursor_x = row_len;
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
            if (E.cursor_x > row_len)
                E.cursor_x = row_len;
        }
    }
}

void editorDrawWelcomeMessage(appendBuffer *ab) {
    char welcome[80];
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

    char buf[32];
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
                if (E.find_active && !E.select_mode && E.find_query) {
                    int match_len = strlen(E.find_query);
                    for (int m = 0; m < E.find_num_matches; m++) {
                        if (E.find_match_lines[m] == file_row) {
                            int col_start = E.find_match_cols[m];
                            int col_end = col_start + match_len;
                            if (j + E.col_offset >= col_start && j + E.col_offset < col_end) {
                                is_find = 1;
                                if (m == E.find_current_idx)
                                    is_current_match = 1;
                                break;
                            }
                        }
                    }
                }

                if (is_sel) abAppend(ab, INVERTED_COLORS, sizeof(INVERTED_COLORS) - 1);
                else if (is_current_match) abAppend(ab, YELLOW_COLOR, sizeof(YELLOW_COLOR) - 1);
                else if (is_find) abAppend(ab, INVERTED_COLORS, sizeof(INVERTED_COLORS) - 1);
                abAppend(ab, &E.row[file_row].render[j + E.col_offset], 1);
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
    char status[80], rstatus[80];
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
    if (msg_len && time(NULL) - E.status_msg_time < 5)
        abAppend(ab, E.status_msg, msg_len);

    if (E.find_active) {
        char buf[32];
        snprintf(buf, sizeof(buf), " %d/%d", E.find_current_idx + 1, E.find_num_matches);
        int right_len = strlen(buf);

        while (msg_len + right_len < E.screen_cols) {
            abAppend(ab, " ", 1);
            msg_len++;
        }
        abAppend(ab, buf, right_len);
    }
}

void editorHelpScreen() {
    write(STDOUT_FILENO, CLEAR_SCREEN CURSOR_RESET HIDE_CURSOR, sizeof(CLEAR_SCREEN CURSOR_RESET HIDE_CURSOR) - 1);

    char *help_text[] = {
        "CYPHER Editor Help Page",
        "",
        "Keyboard Shortcuts:",
        "  Ctrl-S               - Save",
        "  Ctrl-Q               - Quit",
        "  Ctrl-F               - Find",
        "  Ctrl-G / L / _       - Jump to line",
        "  Ctrl-A               - Select all",
        "  Ctrl-Z               - Undo last major change",
        "  Ctrl-Y               - Redo last major change",
        "  Ctrl-C               - Copy selected text",
        "  Ctrl-X               - Cut selected text",
        "  Ctrl-V               - Paste from clipboard",
        "  Ctrl-H               - Show help page",
        "",
        "Press any key to return..."
    };

    int lines = sizeof(help_text) / sizeof(help_text[0]);
    int row = 1;
    for (int i = 0; i < lines; i++) {
        char buf[128];
        int len = snprintf(buf, sizeof(buf), "\x1b[%d;1H%s", row++, help_text[i]);
        write(STDOUT_FILENO, buf, len);
    }

    editorReadKey();
    write(STDOUT_FILENO, CLEAR_SCREEN CURSOR_RESET SHOW_CURSOR, sizeof(CLEAR_SCREEN CURSOR_RESET SHOW_CURSOR) - 1);
}

void editorInit() {
    E.cursor_x = 0;
    E.cursor_y = 0;
    E.render_x = 0;
    E.row_offset = 0;
    E.col_offset = 0;
    E.num_rows = 0;
    E.row = NULL;
    E.dirty = 0;
    E.filename = NULL;
    E.status_msg[0] = '\0';
    E.status_msg_time = 0;
    E.clipboard = NULL;
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
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL)
        die("realloc");

    memcpy(&new[ab->len], str, len);
    ab->b = new;
    ab->len += len;
}

void abFree(appendBuffer *ab) {
    free(ab->b);
}

void editorOpen(char *filename) {
    free(E.filename);
    E.filename = strdup(filename);
    if (E.filename == NULL)
        die("strdup");

    FILE *fp = fopen(filename, "r");
    if (!fp) die("fopen");

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
    for (int i = 0; i < E.num_rows; i++)
        total_len += E.row[i].size + 1;
    *buf_len = total_len;

    char *buf = malloc(total_len);
    if (buf == NULL)
        die("malloc");

    char *ptr = buf;
    for (int i = 0; i < E.num_rows; i++) {
        memcpy(ptr, E.row[i].chars, E.row[i].size);
        ptr += E.row[i].size;
        *ptr = '\n';
        ptr++;
    }

    return buf;
}

void editorSave() {
    if (E.filename == NULL) {
        E.filename = editorPrompt("Save as: %s (ESC to cancel)", NULL);
        if (E.filename == NULL) {
            editorSetStatusMsg("Save aborted");
            return;
        }
    }

    int len;
    char *buf = editorRowsToString(&len);

    int fp = open(E.filename,
                  O_RDWR |     // read and write
                      O_CREAT, // create if doesnt exist
                  0644);       // permissions
    if (fp != -1) {
        if (ftruncate(fp, len) != -1) {
            if (write(fp, buf, len) == len) {
                close(fp);
                free(buf);
                E.dirty = 0;
                editorSetStatusMsg("%d bytes written to disk", len);
                return;
            }
        }
        close(fp);
    }

    free(buf);
    editorSetStatusMsg("Can't save! I/O error: %s", strerror(errno));
}

void editorInsertRow(int at, char *str, size_t len) {
    if (at < 0 || at > E.num_rows) return;

    E.row = realloc(E.row, sizeof(editorRow) * (E.num_rows + 1));
    if (E.row == NULL)
        die("realloc");
    memmove(&E.row[at + 1], &E.row[at], sizeof(editorRow) * (E.num_rows - at));

    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    if (E.row[at].chars == NULL)
        die("malloc");

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
    row->render = malloc(row->size + tabs * (TAB_SIZE - 1) + 1);
    if (row->render == NULL)
        die("malloc");

    int idx = 0;
    for (int i = 0; i < row->size; i++) {
        if (row->chars[i] == '\t') {
            row->render[idx++] = ' ';
            while (idx % TAB_SIZE != 0)
                row->render[idx++] = ' ';
        } else {
            row->render[idx++] = row->chars[i];
        }
    }
    
    row->render[idx] = '\0';
    row->rsize = idx;
}

int editorRowCxToRx(editorRow *row, int cursor_x) {
    int render_x = 0;
    for (int i = 0; i < cursor_x; i++) {
        if (row->chars[i] == '\t')
            render_x += (TAB_SIZE - 1) - (render_x % TAB_SIZE);
        render_x++;
    }

    return render_x;
}

int editorRowRxToCx(editorRow *row, int render_x) {
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

void editorRowInsertChar(editorRow *row, int at, int c) {
    if (at < 0 || at > row->size)
        at = row->size;
    
    row->chars = realloc(row->chars, row->size + 2);
    if (row->chars == NULL)
        die("realloc");
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;
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

void editorRowAppendString(editorRow *row, char *str, size_t len) {
    row->chars = realloc(row->chars, row->size + len + 1);
    if (row->chars == NULL)
        die("realloc");
    memcpy(&row->chars[row->size], str, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    E.dirty++;
}

void editorInsertChar(int c) {
    if (E.select_mode) {
        saveEditorStateForUndo();
        editorDeleteSelectedText();
        E.select_mode = 0;
    }

    if (E.cursor_y == E.num_rows)
        editorInsertRow(E.num_rows, "", 0);
    editorRowInsertChar(&E.row[E.cursor_y], E.cursor_x, c);
    E.cursor_x++;
}

void editorDeleteChar() {
    if (E.select_mode) {
        editorDeleteSelectedText();
        return;
    }

    if (E.cursor_y == E.num_rows) return;
    if (E.cursor_x == 0 && E.cursor_y == 0) return;

    editorRow *row = &E.row[E.cursor_y];
    if (E.cursor_x > 0)
        editorRowDeleteChar(row, --E.cursor_x);
    else {
        E.cursor_x = E.row[E.cursor_y - 1].size;
        editorRowAppendString(&E.row[E.cursor_y - 1], row->chars, row->size);
        editorDeleteRow(E.cursor_y--);
    }
}

void editorInsertNewline() {
    if (E.cursor_x == 0)
        editorInsertRow(E.cursor_y, "", 0);
    else {
        editorRow *row = &E.row[E.cursor_y];
        editorInsertRow(E.cursor_y + 1, &row->chars[E.cursor_x], row->size - E.cursor_x);
        row = &E.row[E.cursor_y];
        row->size = E.cursor_x;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
    }

    E.cursor_y++;
    E.cursor_x = 0;
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

        for (int r = y2 - 1; r > y1; r--)
            editorDeleteRow(r);
    }

    E.cursor_x = x1;
    E.cursor_y = y1;

    E.select_mode = 0;
    E.dirty++;
}

void editorFind() {
    int saved_cursor_x = E.cursor_x;
    int saved_cursor_y = E.cursor_y;
    int saved_col_offset = E.col_offset;
    int saved_row_offset = E.row_offset;

    char *query = editorPrompt("Search: %s (Use ESC/Arrows/Enter)", editorFindCallback);

    if (query) free(query);
    else {
        E.cursor_x = saved_cursor_x;
        E.cursor_y = saved_cursor_y;
        E.col_offset = saved_col_offset;
        E.row_offset = saved_row_offset;
        E.find_active = 0;
    }
}

void editorFindCallback(char *query, int key) {
    int direction = 1;

    if (key == '\r' || key == ESCAPE_CHAR || query[0] == '\0') {
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
        E.find_query = strdup(query);
        if (!E.find_query)
            die("strdup");

        free(E.find_match_lines);
        free(E.find_match_cols);
        E.find_match_lines = NULL;
        E.find_match_cols = NULL;
        E.find_num_matches = 0;
        E.find_current_idx = -1;

        for (int i = 0; i < E.num_rows; i++) {
            char *line = E.row[i].render;
            char *p = line;
            while ((p = strstr(p, query)) != NULL) {
                E.find_match_lines = realloc(E.find_match_lines, sizeof(int) * (E.find_num_matches + 1));
                E.find_match_cols = realloc(E.find_match_cols, sizeof(int) * (E.find_num_matches + 1));
                if (!E.find_match_lines || !E.find_match_cols)
                    die("realloc");

                E.find_match_lines[E.find_num_matches] = i;
                E.find_match_cols[E.find_num_matches] = p - line;
                E.find_num_matches++;

                p += strlen(query);
            }
        }

        if (E.find_num_matches > 0) {
            E.find_current_idx = 0;
            int row = E.find_match_lines[0];
            int col = E.find_match_cols[0];
            E.cursor_y = row;
            E.cursor_x = editorRowRxToCx(&E.row[row], col);
            E.row_offset = E.num_rows;

            int render_pos = editorRowCxToRx(&E.row[row], E.cursor_x);
            if (render_pos >= E.col_offset + E.screen_cols - 1) {
                int margin = strlen(query) + 3;
                if (render_pos > margin) {
                    E.col_offset = render_pos - (E.screen_cols - margin);
                    if (E.col_offset < 0) E.col_offset = 0;
                }
            }
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
            E.cursor_x = editorRowRxToCx(&E.row[row], col);
            E.row_offset = E.num_rows;

            int render_pos = editorRowCxToRx(&E.row[row], E.cursor_x);
            if (render_pos >= E.col_offset + E.screen_cols - 1) {
                int margin = strlen(query) + 3;
                if (render_pos > margin) {
                    E.col_offset = render_pos - (E.screen_cols - margin);
                    if (E.col_offset < 0) E.col_offset = 0;
                }
            }
        }
    }
}

void clipboardCopyToSystem(const char *data) {
    if (!data) return;
    FILE *pipe = NULL;

    #if defined(__APPLE__)          // macOS
        pipe = popen("pbcopy", "w");
    #elif defined(__linux__)        // Linux or WSL
        FILE *test_pipe = popen("grep -i microsoft /proc/version", "r");
        int is_wsl = 0;
        if (test_pipe) {
            char buf[128];
            is_wsl = fgets(buf, sizeof(buf), test_pipe) != NULL;
            pclose(test_pipe);
        }

        if (is_wsl)                 // WSL
            pipe = popen("clip.exe", "w");
        else                        // Linux
            pipe = popen("xclip -selection clipboard", "w");
    #else                           // Default Fallback
        pipe = popen("xclip -selection clipboard", "w");
    #endif

    if (pipe) {
        fwrite(data, 1, strlen(data), pipe);
        pclose(pipe);
    } else {
        editorSetStatusMsg("Unable to copy to system clipboard");
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

    char *buf = malloc(total + 1);
    if (buf == NULL)
        die("malloc");

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

void editorCopySelection() {
    if (!E.select_mode) {
        editorSetStatusMsg("No selection to copy");
        return;
    }

    free(E.clipboard);
    E.clipboard = editorGetSelectedText();

    if (E.clipboard) {
        editorSetStatusMsg("Copied %zu bytes", strlen(E.clipboard));
        clipboardCopyToSystem(E.clipboard);
    }
}

void editorCutSelection() {
    if (!E.select_mode) {
        editorSetStatusMsg("No selection to cut");
        return;
    }

    saveEditorStateForUndo();
    free(E.clipboard);
    E.clipboard = editorGetSelectedText();
    if (!E.clipboard) return;

    clipboardCopyToSystem(E.clipboard);
    editorDeleteSelectedText();

    editorSetStatusMsg("Cut %zu bytes", strlen(E.clipboard));
}

void editorPaste() {
    char *clipboard_data = NULL;
    size_t buf_size = 0;
    FILE *pipe = NULL;

    #if defined(__APPLE__)          // macOS
        pipe = popen("pbpaste", "r");
    #elif defined(__linux__)        // Linux or WSL
        FILE *test_pipe = popen("grep -i microsoft /proc/version", "r");
        int is_wsl = 0;
        if (test_pipe) {
            char buf[128];
            is_wsl = fgets(buf, sizeof(buf), test_pipe) != NULL;
            pclose(test_pipe);
        }
        if (is_wsl)                 // WSL
            pipe = popen("powershell.exe Get-Clipboard", "r");
        else                        // Linux
            pipe = popen("xclip -selection clipboard -o 2>/dev/null", "r");
    #else                           // Default Fallback
        pipe = popen("xclip -selection clipboard -o", "r");
    #endif

    if (!pipe) {
        editorSetStatusMsg("Clipboard tool missing or inaccessible");
        return;
    }

    saveEditorStateForUndo();
    char chunk[256];
    size_t len;
    while ((len = fread(chunk, 1, sizeof(chunk), pipe)) > 0) {
        clipboard_data = realloc(clipboard_data, buf_size + len + 1);
        if (!clipboard_data)
            die("realloc");

        memcpy(clipboard_data + buf_size, chunk, len);
        buf_size += len;
    }
    pclose(pipe);

    if (!clipboard_data || buf_size == 0) {
        free(clipboard_data);
        editorSetStatusMsg("Clipboard is empty");
        return;
    }

    clipboard_data[buf_size] = '\0';
    size_t j = 0;
    for (size_t i = 0; i < buf_size; i++)
        if (clipboard_data[i] != '\r')
            clipboard_data[j++] = clipboard_data[i];
    clipboard_data[j] = '\0';

    if (E.select_mode) {
        editorDeleteSelectedText();
    }

    char *line_start = clipboard_data;
    char *newline_pos;
    while ((newline_pos = strchr(line_start, '\n')) != NULL) {
        size_t chunk_len = newline_pos - line_start;
        for (size_t i = 0; i < chunk_len; i++)
            editorInsertChar(line_start[i]);
        if (*(newline_pos + 1) != '\0')
            editorInsertNewline();
        line_start = newline_pos + 1;
    }

    while (*line_start) {
        editorInsertChar(*line_start++);
    }

    editorSetStatusMsg("Pasted %zu bytes from system clipboard", buf_size);
    free(clipboard_data);
}

void editorJump() {
    int saved_cursor_x = E.cursor_x;
    int saved_cursor_y = E.cursor_y;
    int saved_col_offset = E.col_offset;
    int saved_row_offset = E.row_offset;

    char *input = editorPrompt("Jump to (row:col): %s (ESC to cancel)", editorJumpCallback);

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
    editorScroll();
}

void freeEditorState(editorState *state) {
    if (state->buffer) free(state->buffer);
    state->buffer = NULL;
}

void saveEditorStateForUndo() {
    if (undo_top >= UNDO_REDO_STACK_SIZE - 1)
        return;

    for (int i = 0; i <= redo_top; i++)
        freeEditorState(&redo_stack[i]);
    redo_top = -1;

    undo_top++;
    editorState *state = &undo_stack[undo_top];

    state->buffer = editorRowsToString(&state->buf_len);
    state->cursor_x = E.cursor_x;
    state->cursor_y = E.cursor_y;
    state->select_mode = E.select_mode;
    state->select_sx = E.select_sx;
    state->select_sy = E.select_sy;
    state->select_ex = E.select_ex;
    state->select_ey = E.select_ey;
}

void restoreEditorState(editorState *state) {
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
    E.select_mode = state->select_mode;
    E.select_sx = state->select_sx;
    E.select_sy = state->select_sy;
    E.select_ex = state->select_ex;
    E.select_ey = state->select_ey;

    E.dirty++;
}

void editorUndo() {
    if (undo_top < 0) {
        editorSetStatusMsg("Nothing to undo");
        return;
    }

    if (redo_top < UNDO_REDO_STACK_SIZE - 1) {
        redo_top++;
        editorState *state = &redo_stack[redo_top];

        state->buffer = editorRowsToString(&state->buf_len);
        state->cursor_x = E.cursor_x;
        state->cursor_y = E.cursor_y;
        state->select_mode = E.select_mode;
        state->select_sx = E.select_sx;
        state->select_sy = E.select_sy;
        state->select_ex = E.select_ex;
        state->select_ey = E.select_ey;
    }

    editorState *undo_st = &undo_stack[undo_top];
    restoreEditorState(undo_st);

    freeEditorState(undo_st);
    undo_top--;
    editorSetStatusMsg("Undo");
}

void editorRedo() {
    if (redo_top < 0) {
        editorSetStatusMsg("Nothing to redo");
        return;
    }

    if (undo_top < UNDO_REDO_STACK_SIZE - 1) {
        undo_top++;
        editorState *state = &undo_stack[undo_top];

        state->buffer = editorRowsToString(&state->buf_len);
        state->cursor_x = E.cursor_x;
        state->cursor_y = E.cursor_y;
        state->select_mode = E.select_mode;
        state->select_sx = E.select_sx;
        state->select_sy = E.select_sy;
        state->select_ex = E.select_ex;
        state->select_ey = E.select_ey;
    }

    editorState *redo_st = &redo_stack[redo_top];
    restoreEditorState(redo_st);

    freeEditorState(redo_st);
    redo_top--;
    editorSetStatusMsg("Redo");
}