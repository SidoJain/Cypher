/*** includes ***/

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

/*** defines ***/

#define CYPHER_VERSION "0.0.1"

#define CTRL_KEY(k)         ((k) & 0x1f)
#define APPEND_BUFFER_INIT  {NULL, 0}

#define	STDIN_FILENO	0
#define	STDOUT_FILENO	1
#define	STDERR_FILENO	2

#define TAB_SIZE 8

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
#define INVERTED_COLORS         "\x1b[7m"
#define REMOVE_GRAPHICS         "\x1b[m"

/*** structs and enums ***/

enum editorKey {
    BACKSPACE = 127,
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
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
    char *filename;
    char status_msg[80];
    time_t status_msg_time;
    struct termios original_termios;
} editorConfig;

typedef struct {
    char *b;
    int len;
} appendBuffer;

/*** data ***/

editorConfig E;

/*** declaration ***/

// Terminal
void die(const char *);
void enableRawMode();
void disableRawMode();
int editorReadKey();
int getWindowSize(int *, int *);
int getCursorPosition(int *, int *);

// Input
void editorProcessKeypress();
void editorMoveCursor(int);

// Output
void editorRefreshScreen();
void editorDrawRows(appendBuffer *);
void editorScroll();
void editorDrawStatusBar(appendBuffer *);
void editorSetStatusMsg(const char *, ...);
void editorDrawMsgBar(appendBuffer *);

// Initialize
void initEditor();

// Append Buffer
void abAppend(appendBuffer *, const char *, int);
void abFree(appendBuffer *);

// File I/O
void editorOpen(char *);
char *editorRowsToString(int *);
void editorSave();

// Row Operations
void editorAppendRow(char *, size_t);
void editorUpdateRow(editorRow *);
int editorRowCxToRx(editorRow *, int);
void editorRowInsertChar(editorRow *, int, int);

// Editor Operations
void editorInsertChar(int);

/*** main ***/

int main(int argc, char *argv[]) {
    enableRawMode();
    initEditor();
    if (argc >= 2) {
        editorOpen(argv[1]);
    }

    editorSetStatusMsg("HELP: Ctrl-Q = quit");

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}

/*** definitions ***/

void die(const char *str) {
    write(STDOUT_FILENO, CLEAR_SCREEN, 4);
    write(STDOUT_FILENO, CURSOR_RESET, 3);

    perror(str);
    exit(1);
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.original_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);

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
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die ("read");
    }
    
    if (c == ESCAPE_CHAR) {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return ESCAPE_CHAR;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return ESCAPE_CHAR;

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return ESCAPE_CHAR;
                if (seq[2] == '~') {
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
        if (write(STDOUT_FILENO, CURSOR_FORWARD CURSOR_DOWN, 12) != 12)
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

    if (write(STDOUT_FILENO, QUERY_CURSOR_POSITION, 4) != 4) return -1;
    
    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
    return 0;
}

void editorProcessKeypress() {
    int c = editorReadKey();
    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, CLEAR_SCREEN, 4);
            write(STDOUT_FILENO, CURSOR_RESET, 3);

            exit(0);
            break;

        case CTRL_KEY('s'):
            editorSave();
            break;

        case '\r':
            // TODO
            break;

        case HOME_KEY:
            E.cursor_x = 0;
            break;
        case END_KEY:
            if (E.cursor_y < E.num_rows)
                E.cursor_x = E.row[E.cursor_y].size;
            break;

        case BACKSPACE:
        case CTRL_KEY('h'):
        case DEL_KEY:
            // TODO
            break;
        
        case CTRL_KEY('l'):
            // TODO
            break;

        case PAGE_UP:
        case PAGE_DOWN:
            {
                int row_count = E.screen_rows;
                while (row_count--)
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;

        default:
            editorInsertChar(c);
            break;
    }
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

void editorRefreshScreen() {
    editorScroll();

    appendBuffer ab = APPEND_BUFFER_INIT;
    abAppend(&ab, HIDE_CURSOR, 6);
    abAppend(&ab, CURSOR_RESET, 3);

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMsgBar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cursor_y - E.row_offset) + 1, (E.render_x - E.col_offset) + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, SHOW_CURSOR, 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

void editorDrawRows(appendBuffer *ab) {
    for (int y = 0; y < E.screen_rows; y++) {
        int file_row = y + E.row_offset;
        if (file_row >= E.num_rows) {
            if (E.num_rows == 0 && y == E.screen_rows / 3) {
                char welcome[80];
                int welcome_len = snprintf(welcome, sizeof(welcome), "CYPHER editor -- version %s", CYPHER_VERSION);
                if (welcome_len > E.screen_cols)
                    welcome_len = E.screen_cols;
                
                int padding = (E.screen_cols - welcome_len) / 2;
                if (padding) {
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while (padding--)
                    abAppend(ab, " ", 1);

                abAppend(ab, welcome, welcome_len);
            } else {
                abAppend(ab, "~", 1);
            }
        } else {
            int len = E.row[file_row].rsize - E.col_offset;
            if (len < 0)
                len = 0;
            if (len > E.screen_cols)
                len = E.screen_cols;
            abAppend(ab, &E.row[file_row].render[E.col_offset], len);
        }

        abAppend(ab, CLEAR_LINE, 3);
        abAppend(ab, NEW_LINE, 2);
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
    abAppend(ab, INVERTED_COLORS, 4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines", E.filename ? E.filename : "[No Name]", E.num_rows);
    int rlen = snprintf(rstatus, sizeof(rstatus), "%d/%d", E.cursor_y + 1, E.num_rows);
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
    abAppend(ab, REMOVE_GRAPHICS, 3);
    abAppend(ab, NEW_LINE, 2);
}

void editorSetStatusMsg(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.status_msg, sizeof(E.status_msg), fmt, ap);
    va_end(ap);
    E.status_msg_time = time(NULL);
}

void editorDrawMsgBar(appendBuffer *ab) {
    abAppend(ab, CLEAR_LINE, 3);
    int msg_len = strlen(E.status_msg);
    if (msg_len > E.screen_cols)
        msg_len = E.screen_cols;
    if (msg_len && time(NULL) - E.status_msg_time < 5)
        abAppend(ab, E.status_msg, msg_len);
}

void initEditor() {
    E.cursor_x = 0;
    E.cursor_y = 0;
    E.render_x = 0;
    E.row_offset = 0;
    E.col_offset = 0;
    E.num_rows = 0;
    E.row = NULL;
    E.filename = NULL;
    E.status_msg[0] = '\0';
    E.status_msg_time = 0;

    if (getWindowSize(&E.screen_rows, &E.screen_cols) == -1) die("getWindowSize");
    E.screen_rows -= 2;
}

void abAppend(appendBuffer *ab, const char *str, int len) {
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL) return;

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

    FILE *fp = fopen(filename, "r");
    if (!fp) die("fopen");

    char *line = NULL;
    size_t line_cap = 0;
    ssize_t line_len;
    while ((line_len = getline(&line, &line_cap, fp)) != -1) {
        while (line_len > 0 && (line[line_len - 1] == '\n' || line[line_len - 1] == '\r'))
            line_len--;
        editorAppendRow(line, line_len);
    }
    free(line);
    fclose(fp);
}

char *editorRowsToString(int *buf_len) {
    int total_len = 0;
    for (int i = 0; i < E.num_rows; i++)
        total_len += E.row[i].size + 1;
    *buf_len = total_len;

    char *buf = malloc(total_len);
    char *ptr = buf;
    for (int j = 0; j < E.num_rows; j++) {
        memcpy(ptr, E.row[j].chars, E.row[j].size);
        ptr += E.row[j].size;
        *ptr = '\n';
        ptr++;
    }

    return buf;
}

void editorSave() {
    if (E.filename == NULL) return;

    int len;
    char *buf = editorRowsToString(&len);

    int fp = open(E.filename,
                  O_RDWR |     // read and write
                      O_CREAT, // create if doesnt exist
                  0644);       // permissions
    ftruncate(fp, len);
    write(fp, buf, len);
    close(fp);
    free(buf);
}

void editorAppendRow(char *str, size_t len) {
    E.row = realloc(E.row, sizeof(editorRow) * (E.num_rows + 1));

    int at = E.num_rows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, str, len);
    E.row[at].chars[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    editorUpdateRow(&E.row[at]);

    E.num_rows++;
}

void editorUpdateRow(editorRow *row) {
    int tabs = 0;
    for (int i = 0; i < row->size; i++)
        if (row->chars[i] == '\t')
            tabs++;
    free(row->render);
    row->render = malloc(row->size + tabs * (TAB_SIZE - 1) + 1);

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

void editorRowInsertChar(editorRow *row, int at, int c) {
    if (at < 0 || at > row->size)
        at = row->size;
    
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;
    editorUpdateRow(row);
}

void editorInsertChar(int c) {
    if (E.cursor_y == E.num_rows)
        editorAppendRow("", 0);
    editorRowInsertChar(&E.row[E.cursor_y], E.cursor_x, c);
    E.cursor_x++;
}