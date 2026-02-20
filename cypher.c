/*** Includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
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

#define CYPHER_VERSION      "1.3.0"
#define EMPTY_LINE_SYMBOL   "~"

#define CTRL_KEY(k)     ((k) & 0x1f)
#define is_cntrl(k)     ((k) < 32 || (k) == 127)
#define is_alnum(k)     (((k) >= 'a' && (k) <= 'z') || ((k) >= 'A' && (k) <= 'Z') || ((k) >= '0' && (k) <= '9'))

#define TAB_SIZE                4
#define QUIT_TIMES              2
#define SAVE_TIMES              2
#define UNDO_REDO_STACK_SIZE    100
#define UNDO_TIMEOUT_MS         1000
#define STATUS_LENGTH           80
#define SMALL_BUFFER_SIZE       32
#define BUFFER_SIZE             128
#define STATUS_MSG_TIMEOUT_SEC  5
#define MARGIN                  3
#define DEFAULT_FILE_PERMS      0644
#define DEFAULT_AB_CAPACITY     1024
#define DEFAULT_ROW_CAPACITY    32
#define MATCH_BUFFER_PADDING    50

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
} EditorKey;

typedef enum {
    BUFFER_ORIGINAL = 0,
    BUFFER_ADD = 1
} BufferSource;

typedef enum {
    CMD_INSERT,
    CMD_DELETE
} CommandType;

typedef struct {
    BufferSource source;
    size_t start;
    size_t length;
} Piece;

typedef struct {
    char *orig_buf;
    size_t orig_len;
    char *add_buf;
    size_t add_len;
    size_t add_capacity;
    Piece *pieces;
    size_t num_pieces;
    size_t piece_capacity;
    size_t logical_size;
} PieceTable;

typedef struct {
    int x;
    int y;
    int render_x;
    int preferred_x;
} EditorCursor;

typedef struct {
    int screen_rows;
    int screen_cols;
    int row_offset;
    int col_offset;
    volatile sig_atomic_t resized;
} EditorView;

typedef struct {
    PieceTable pt;
    size_t *line_offsets;
    int num_lines;
    int line_capacity;
    char *filename;
    bool dirty;
    int save_times;
    int quit_times;
} EditorBuffer;

typedef struct {
    bool active;
    int sx;
    int sy;
    int ex;
    int ey;
    char *clipboard;
    bool is_pasting;
    int paste_len;
} EditorSelection;

typedef struct {
    bool active;
    char *query;
    int *match_lines;
    int *match_cols;
    int num_matches;
    int current_idx;
} EditorFinder;

typedef struct {
    char status_msg[STATUS_LENGTH];
    time_t status_msg_time;
    struct termios orig_termios;
    int bracket_x;
    int bracket_y;
    bool has_bracket;
} EditorSystem;

typedef struct {
    EditorCursor cursor;
    EditorView view;
    EditorBuffer buf;
    EditorSelection sel;
    EditorFinder find;
    EditorSystem sys;
} EditorConfig;

typedef struct {
    char *b;
    int len;
    int capacity;
} AppendBuffer;

typedef struct {
    CommandType type;
    size_t offset;
    char *text;
    size_t len;
    EditorCursor cursor;
    int transaction_id;
} EditCommand;

typedef struct {
    EditCommand undo_stack[UNDO_REDO_STACK_SIZE];
    int undo_top;
    EditCommand redo_stack[UNDO_REDO_STACK_SIZE];
    int redo_top;
    long last_edit_time;
    int undo_in_progress;
    int current_transaction_id;
    bool in_transaction;
    int save_point;
} EditorUndoRedo;

/*** Global Data ***/

static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
EditorConfig E;
EditorUndoRedo history = {
    .undo_top = -1,
    .redo_top = -1,
    .last_edit_time = 0,
    .undo_in_progress = 0,
    .current_transaction_id = 0,
    .in_transaction = false,
    .save_point = -1,
};

/*** Function Prototypes ***/

// initialization
void editorInit();
void editorCleanup();
void editorQuit();
void die(const char *);

// terminal i/o
void enableRawMode();
void disableRawMode();
void clearTerminal();
void handleSigWinCh(int);
int getWindowSize(int *, int *);
int getCursorPosition(int *, int *);

// input parsing
int editorReadKey();
void editorProcessKeypress();
char *editorPrompt(char *, void (*)(const char *, int), char *);

// output rendering
void editorRefreshScreen();
void editorDrawRows(AppendBuffer *);
void editorSetStatusMsg(const char *);
void editorDrawStatusBar(AppendBuffer *);
void editorDrawMsgBar(AppendBuffer *);
void editorDrawWelcomeMessage(AppendBuffer *);
void editorManualScreen();
void editorScroll();

// cursor
void editorMoveCursor(int);
void editorMoveWordLeft();
void editorMoveWordRight();
void editorScrollPageUp(int);
void editorScrollPageDown(int);
void editorJump();
void editorJumpCallback(const char *, int);

// piece table
void ptInit(PieceTable *, const char *, size_t);
void ptFree(PieceTable *);
void ptInsert(PieceTable *, size_t, const char *, size_t);
void ptDelete(PieceTable *, size_t, size_t);
char *ptGetText(PieceTable *);
bool ptFindPiece(PieceTable *, size_t, size_t *, size_t *);
void ptReadLogical(PieceTable *, size_t, size_t, char *);
void editorUpdateLineOffsets(EditorBuffer *);
char *editorGetLine(EditorBuffer *, int, size_t *);
size_t editorGetLineLength(EditorBuffer *, int);
size_t editorGetLogicalOffset(EditorBuffer *, int, int);
void editorOffsetToRowCol(EditorBuffer *, size_t, int *, int *);

// line editing
void editorInsertChar(int);
void editorDeleteChar(int);
void editorInsertNewline();
void editorMoveRowUp();
void editorMoveRowDown();
void editorCopyRowUp();
void editorCopyRowDown();

// clipboard
void editorSelectText(int);
void editorSelectAll();
char *editorGetSelectedText(int *);
void editorDeleteSelectedText();
void editorCopySelection();
void editorCutSelection();
void editorCutLine();
void clipboardCopyToSystem(const char *, int);

// find & replace
void editorFind();
void editorFindCallback(const char *, int);
void editorScanLineMatches(int, const char *);
void editorReplace();
void editorReplaceCallback(const char *, int);
int editorReplaceAll(const char *);
void editorReplaceJumpToCurrent();
bool editorReplaceCurrent(const char *, const char *);

// undo-redo
void freeEditCommand(EditCommand *);
void editorBeginMacro();
void editorEndMacro();
void recordCommand(CommandType, size_t, const char *, size_t, EditorCursor);
void executeInsert(size_t, const char *, size_t);
void executeDelete(size_t, size_t);
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

// file i/o
void editorOpen(const char *);
void editorSave();
void panicSave(int);
char *editorRowsToString(size_t *);

// utility
bool isWordChar(int);
long currentMillis();
char getClosingChar(char);
void clampCursorPosition();
void humanReadableSize(size_t, char *, size_t);
void base64_encode(const char *, int, char *);
int editorLineCxToRx(const char *, int, int);
int editorLineRxToCx(const char *, int, int);

// memory
void *safeMalloc(size_t);
void *safeRealloc(void *, size_t);
char *safeStrdup(const char *);

// append buffer
void abAppend(AppendBuffer *, const char *, int);
void abFree(AppendBuffer *);

/*** Main ***/

int main(int argc, char *argv[]) {
    enableRawMode();

    struct sigaction sa;
    sa.sa_handler = handleSigWinCh;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGWINCH, &sa, NULL);

    signal(SIGSEGV, panicSave); // Catch Segfaults
    signal(SIGABRT, panicSave); // Catch Aborts

    editorInit();
    if (argc >= 2)
        editorOpen(argv[1]);
    else {
        ptInit(&E.buf.pt, "", 0);
        editorUpdateLineOffsets(&E.buf);
    }

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

void editorInit() {
    E.cursor.x = 0;
    E.cursor.y = 0;
    E.cursor.render_x = 0;
    E.cursor.preferred_x = 0;
    E.view.row_offset = 0;
    E.view.col_offset = 0;
    E.view.resized = 0;
    E.buf.line_offsets = NULL;
    E.buf.num_lines = 0;
    E.buf.line_capacity = 0;
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
    ptFree(&E.buf.pt);
    free(E.buf.line_offsets);
    E.buf.line_offsets = NULL;
    E.buf.num_lines = 0;

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

void clearTerminal() {
    if (write(STDOUT_FILENO, CLEAR_SCREEN  CURSOR_RESET, sizeof(CLEAR_SCREEN  CURSOR_RESET)) == -1) {}
}

void handleSigWinCh(int unused) {
    (void)unused;
    E.view.resized = 1;
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
                    int motion = (b & 32);
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

                        int button = cb - ' ';
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
            if (E.sel.active)
                editorCutSelection();
            else
                editorCutLine();
            updateMatchBracket();
            break;

        case PASTE_START:       // paste
            E.sel.is_pasting = true;
            E.sel.paste_len = 0;
            editorBeginMacro();
            break;
        case PASTE_END:
            E.sel.is_pasting = false;
            editorEndMacro();
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
            editorDeleteSelectedText();
            editorInsertNewline();
            updateMatchBracket();
            break;

        case '\t':              // tab
            if (E.sel.is_pasting) E.sel.paste_len++;
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
            if (E.cursor.y < E.buf.num_lines) {
                E.cursor.x = editorGetLineLength(&E.buf, E.cursor.y);
                E.cursor.preferred_x = E.cursor.x;
            }
            updateMatchBracket();
            break;

        case DEL_KEY:
            editorDeleteChar(0);
            updateMatchBracket();
            break;
        case BACKSPACE:
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
            if (E.cursor.y < E.buf.num_lines)
                E.cursor.x = editorGetLineLength(&E.buf, E.cursor.y);
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
            editorMoveRowUp();
            updateMatchBracket();
            break;
        case ALT_ARROW_DOWN:
            editorMoveRowDown();
            updateMatchBracket();
            break;

        case ALT_SHIFT_ARROW_UP:
            editorCopyRowUp();
            updateMatchBracket();
            break;
        case ALT_SHIFT_ARROW_DOWN:
            editorCopyRowDown();
            updateMatchBracket();
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

void editorRefreshScreen() {
    editorScroll();

    AppendBuffer ab = {
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

void editorDrawRows(AppendBuffer *ab) {
    if (E.buf.num_lines == 0) {
        editorDrawWelcomeMessage(ab);
        return;
    }

    for (int y = 0; y < E.view.screen_rows; y++) {
        int file_row = y + E.view.row_offset;
        if (file_row >= E.buf.num_lines)
            abAppend(ab, EMPTY_LINE_SYMBOL, sizeof(EMPTY_LINE_SYMBOL) - 1);
        else {
            size_t line_len;
            char *line_text = editorGetLine(&E.buf, file_row, &line_len);

            if (line_text) {
                int tabs = 0;
                for (size_t i = 0; i < line_len; i++) if (line_text[i] == '\t') tabs++;

                char *render_text = safeMalloc(line_len + tabs * (TAB_SIZE - 1) + 1);
                int rsize = 0;
                for (size_t i = 0; i < line_len; i++) {
                    if (line_text[i] == '\t') {
                        render_text[rsize++] = ' ';
                        while (rsize % TAB_SIZE != 0) render_text[rsize++] = ' ';
                    }
                    else if (is_cntrl(line_text[i]))
                        render_text[rsize++] = '?';
                    else
                        render_text[rsize++] = line_text[i];
                }
                render_text[rsize] = '\0';

                int len = rsize - E.view.col_offset;
                if (len < 0) len = 0;
                if (len > E.view.screen_cols) len = E.view.screen_cols;

                for (int j = 0; j < len; j++) {
                    int cx = editorLineRxToCx(line_text, line_len, j + E.view.col_offset);
                    int is_sel = 0;
                    if (E.sel.active) {
                        int y1 = E.sel.sy, x1 = E.sel.sx;
                        int y2 = E.sel.ey, x2 = E.sel.ex;
                        if (y1 > y2 || (y1 == y2 && x1 > x2)) {
                            int tmpy = y1, tmpx = x1;
                            y1 = y2; x1 = x2; y2 = tmpy; x2 = tmpx;
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
                                    if (m == E.find.current_idx) is_current_match = 1;
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
                            start_row = end_row; start_col = end_col;
                            end_row = tmp_r; end_col = tmp_c;
                        }

                        if (file_row >= start_row && file_row <= end_row) {
                            int highlight_start_col = 0;
                            int highlight_end_col = line_len;

                            if (file_row == start_row) highlight_start_col = start_col;
                            if (file_row == end_row) highlight_end_col = end_col + 1;

                            if (cx >= highlight_start_col && cx < highlight_end_col) {
                                abAppend(ab, DARK_GRAY_BG_COLOR, sizeof(DARK_GRAY_BG_COLOR) - 1);
                                abAppend(ab, &render_text[j + E.view.col_offset], 1);
                                abAppend(ab, REMOVE_GRAPHICS, sizeof(REMOVE_GRAPHICS) - 1);
                            }
                            else
                                abAppend(ab, &render_text[j + E.view.col_offset], 1);
                        }
                        else
                            abAppend(ab, &render_text[j + E.view.col_offset], 1);
                    }
                    else
                        abAppend(ab, &render_text[j + E.view.col_offset], 1);

                    if (is_sel || is_find || is_current_match) abAppend(ab, REMOVE_GRAPHICS, sizeof(REMOVE_GRAPHICS) - 1);
                }

                free(render_text);
                free(line_text);
            }
        }
        abAppend(ab, CLEAR_LINE NEW_LINE, sizeof(CLEAR_LINE NEW_LINE) - 1);
    }
}

void editorSetStatusMsg(const char *msg) {
    snprintf(E.sys.status_msg, sizeof(E.sys.status_msg), "%s", msg);
    E.sys.status_msg_time = time(NULL);
}

void editorDrawStatusBar(AppendBuffer *ab) {
    abAppend(ab, INVERTED_COLORS, sizeof(INVERTED_COLORS) - 1);
    char status[STATUS_LENGTH], rstatus[STATUS_LENGTH];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s", E.buf.filename ? E.buf.filename : "[No Name]", E.buf.num_lines, E.buf.dirty ? "(modified)" : "");
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

void editorDrawMsgBar(AppendBuffer *ab) {
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

void editorDrawWelcomeMessage(AppendBuffer *ab) {
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

    while (editorReadKey() == 0);
    write(STDOUT_FILENO, CLEAR_SCREEN CURSOR_RESET SHOW_CURSOR, sizeof(CLEAR_SCREEN CURSOR_RESET SHOW_CURSOR) - 1);
}

void editorScroll() {
    E.cursor.render_x = 0;
    if (E.cursor.y < E.buf.num_lines) {
        size_t line_len;
        char *line_text = editorGetLine(&E.buf, E.cursor.y, &line_len);
        if (line_text) {
            E.cursor.render_x = editorLineCxToRx(line_text, line_len, E.cursor.x);
            free(line_text);
        }
    }

    if (E.cursor.y < E.view.row_offset)
        E.view.row_offset = E.cursor.y;
    if (E.cursor.y >= E.view.row_offset + E.view.screen_rows)
        E.view.row_offset = E.cursor.y - E.view.screen_rows + 1;
    if (E.cursor.render_x < E.view.col_offset)
        E.view.col_offset = E.cursor.render_x;
    if (E.cursor.render_x >= E.view.col_offset + E.view.screen_cols)
        E.view.col_offset = E.cursor.render_x - E.view.screen_cols + 1;
}

void editorMoveCursor(int key) {
    if (E.buf.num_lines == 0)
        return;

    int current_line_len = editorGetLineLength(&E.buf, E.cursor.y);
    switch (key) {
        case ARROW_LEFT:
            if (E.cursor.x != 0) {
                E.cursor.x--;
            } else if (E.cursor.y > 0) {
                E.cursor.y--;
                E.cursor.x = editorGetLineLength(&E.buf, E.cursor.y);
            }
            E.cursor.preferred_x = E.cursor.x;
            break;

        case ARROW_RIGHT:
            if (E.cursor.x < current_line_len) {
                E.cursor.x++;
            } else if (E.cursor.x == current_line_len && E.cursor.y < E.buf.num_lines - 1) {
                E.cursor.y++;
                E.cursor.x = 0;
            }
            E.cursor.preferred_x = E.cursor.x;
            break;

        case ARROW_DOWN:
            if (E.cursor.y < E.buf.num_lines - 1) {
                E.cursor.y++;
                int next_line_len = editorGetLineLength(&E.buf, E.cursor.y);
                if (E.cursor.preferred_x < 0) E.cursor.preferred_x = E.cursor.x;
                E.cursor.x = next_line_len > E.cursor.preferred_x ? E.cursor.preferred_x : next_line_len;
            } else {
                E.cursor.x = current_line_len;
                E.cursor.preferred_x = current_line_len;
            }
            break;

        case ARROW_UP:
            if (E.cursor.y > 0) {
                E.cursor.y--;
                int prev_line_len = editorGetLineLength(&E.buf, E.cursor.y);
                if (E.cursor.preferred_x < 0) E.cursor.preferred_x = E.cursor.x;
                E.cursor.x = prev_line_len > E.cursor.preferred_x ? E.cursor.preferred_x : prev_line_len;
            } else {
                E.cursor.x = 0;
                E.cursor.preferred_x = 0;
            }
            break;
    }

    current_line_len = editorGetLineLength(&E.buf, E.cursor.y);
    if (E.cursor.x > current_line_len)
        E.cursor.x = current_line_len;
}

void editorMoveWordLeft() {
    if (E.cursor.y >= E.buf.num_lines) return;
    size_t line_len;
    char *line_text = editorGetLine(&E.buf, E.cursor.y, &line_len);
    if (!line_text) return;

    while (E.cursor.x > 0 && !isWordChar(line_text[E.cursor.x - 1])) E.cursor.x--;
    while (E.cursor.x > 0 && isWordChar(line_text[E.cursor.x - 1])) E.cursor.x--;
    E.cursor.preferred_x = E.cursor.x;
    free(line_text);
}

void editorMoveWordRight() {
    if (E.cursor.y >= E.buf.num_lines) return;
    size_t line_len;
    char *line_text = editorGetLine(&E.buf, E.cursor.y, &line_len);
    if (!line_text) return;

    while ((size_t)E.cursor.x < line_len && !isWordChar(line_text[E.cursor.x])) E.cursor.x++;
    while ((size_t)E.cursor.x < line_len && isWordChar(line_text[E.cursor.x])) E.cursor.x++;
    E.cursor.preferred_x = E.cursor.x;
    free(line_text);
}

void editorScrollPageUp(int scroll_amount) {
    if (E.buf.num_lines == 0) return;

    if (E.view.row_offset > 0) {
        if (scroll_amount > E.view.row_offset) scroll_amount = E.view.row_offset;
        E.view.row_offset -= scroll_amount;
        E.cursor.y -= scroll_amount;

        if (E.cursor.y < 0) E.cursor.y = 0;
        if (E.cursor.y < E.buf.num_lines) {
            int row_len = editorGetLineLength(&E.buf, E.cursor.y);
            if (E.cursor.preferred_x > row_len) E.cursor.x = row_len;
            else E.cursor.x = E.cursor.preferred_x;
        }
    } else {
        E.cursor.y = 0;
        E.cursor.x = 0;
        E.cursor.preferred_x = 0;
    }
}

void editorScrollPageDown(int scroll_amount) {
    if (E.buf.num_lines == 0) return;

    if (E.view.row_offset < E.buf.num_lines - E.view.screen_rows) {
        E.view.row_offset += scroll_amount;
        E.cursor.y += scroll_amount;

        if (E.cursor.y >= E.buf.num_lines) E.cursor.y = E.buf.num_lines - 1;
        if (E.cursor.y < E.buf.num_lines) {
            int row_len = editorGetLineLength(&E.buf, E.cursor.y);
            if (E.cursor.preferred_x > row_len) E.cursor.x = row_len;
            else E.cursor.x = E.cursor.preferred_x;
        }
    } else {
        E.cursor.y = E.buf.num_lines - 1;
        E.cursor.x = editorGetLineLength(&E.buf, E.cursor.y);
        E.cursor.preferred_x = E.cursor.x;
    }
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
    if (row >= E.buf.num_lines) row = E.buf.num_lines - 1;
    if (row < 0) row = 0;
    if (col < 0) col = 0;
    if (row >= 0 && row < E.buf.num_lines && (size_t)col > editorGetLineLength(&E.buf, row)) 
        col = editorGetLineLength(&E.buf, row);

    E.cursor.x = col;
    E.cursor.y = row;
    E.cursor.preferred_x = col;
    editorScroll();
}

void ptInit(PieceTable *pt, const char *file_content, size_t content_len) {
    pt->orig_buf = file_content ? safeStrdup(file_content) : safeStrdup("");
    pt->orig_len = content_len;
    pt->add_capacity = 1024;
    pt->add_buf = safeMalloc(pt->add_capacity);
    pt->add_len = 0;
    pt->piece_capacity = 128;
    pt->pieces = safeMalloc(sizeof(Piece) * pt->piece_capacity);

    if (content_len > 0) {
        pt->pieces[0] = (Piece){BUFFER_ORIGINAL, 0, content_len};
        pt->num_pieces = 1;
    } else {
        pt->num_pieces = 0;
    }
    pt->logical_size = content_len;
}

void ptFree(PieceTable *pt) {
    free(pt->orig_buf);
    free(pt->add_buf);
    free(pt->pieces);
}

void ptInsert(PieceTable *pt, size_t offset, const char *text, size_t text_len) {
    if (text_len == 0 || offset > pt->logical_size) return;

    if (pt->add_len + text_len > pt->add_capacity) {
        while (pt->add_len + text_len > pt->add_capacity) pt->add_capacity *= 2;
        pt->add_buf = safeRealloc(pt->add_buf, pt->add_capacity);
    }
    size_t new_piece_start = pt->add_len;
    memcpy(pt->add_buf + pt->add_len, text, text_len);
    pt->add_len += text_len;

    Piece new_piece = {
        .source = BUFFER_ADD,
        .start = new_piece_start,
        .length = text_len
    };

    if (pt->num_pieces == 0) {
        pt->pieces[0] = new_piece;
        pt->num_pieces = 1;
        pt->logical_size += text_len;
        return;
    }

    size_t piece_idx = 0, piece_offset = 0;
    ptFindPiece(pt, offset, &piece_idx, &piece_offset);
    Piece target = pt->pieces[piece_idx];

    if (piece_offset == 0) {
        if (pt->num_pieces + 1 > pt->piece_capacity) {
            pt->piece_capacity *= 2;
            pt->pieces = safeRealloc(pt->pieces, sizeof(Piece) * pt->piece_capacity);
        }
        memmove(&pt->pieces[piece_idx + 1], &pt->pieces[piece_idx], sizeof(Piece) * (pt->num_pieces - piece_idx));
        pt->pieces[piece_idx] = new_piece;
        pt->num_pieces++;
    } else if (piece_offset == target.length) {
        if (pt->num_pieces + 1 > pt->piece_capacity) {
            pt->piece_capacity *= 2;
            pt->pieces = safeRealloc(pt->pieces, sizeof(Piece) * pt->piece_capacity);
        }
        memmove(&pt->pieces[piece_idx + 2], &pt->pieces[piece_idx + 1], sizeof(Piece) * (pt->num_pieces - piece_idx - 1));
        pt->pieces[piece_idx + 1] = new_piece;
        pt->num_pieces++;
    } else {
        Piece left = {target.source, target.start, piece_offset};
        Piece right = {target.source, target.start + piece_offset, target.length - piece_offset};
        if (pt->num_pieces + 2 > pt->piece_capacity) {
            pt->piece_capacity *= 2;
            pt->pieces = safeRealloc(pt->pieces, sizeof(Piece) * pt->piece_capacity);
        }

        memmove(&pt->pieces[piece_idx + 3], &pt->pieces[piece_idx + 1], sizeof(Piece) * (pt->num_pieces - piece_idx - 1));
        pt->pieces[piece_idx] = left;
        pt->pieces[piece_idx + 1] = new_piece;
        pt->pieces[piece_idx + 2] = right;
        pt->num_pieces += 2;
    }
    pt->logical_size += text_len;
}

void ptDelete(PieceTable *pt, size_t offset, size_t len) {
    if (len == 0 || offset >= pt->logical_size) return;
    if (offset + len > pt->logical_size) len = pt->logical_size - offset;

    size_t start_idx, start_offset;
    size_t end_idx, end_offset;
    ptFindPiece(pt, offset, &start_idx, &start_offset);
    ptFindPiece(pt, offset + len, &end_idx, &end_offset);

    if (start_idx == end_idx) {
        Piece target = pt->pieces[start_idx];
        if (start_offset == 0 && end_offset == target.length) {
            memmove(&pt->pieces[start_idx], &pt->pieces[start_idx + 1], sizeof(Piece) * (pt->num_pieces - start_idx - 1));
            pt->num_pieces--;
        } else if (start_offset == 0) {
            pt->pieces[start_idx].start += len;
            pt->pieces[start_idx].length -= len;
        } else if (end_offset == target.length) {
            pt->pieces[start_idx].length -= len;
        } else {
            Piece left = {target.source, target.start, start_offset};
            Piece right = {target.source, target.start + end_offset, target.length - end_offset};
            if (pt->num_pieces + 1 > pt->piece_capacity) {
                pt->piece_capacity *= 2;
                pt->pieces = safeRealloc(pt->pieces, sizeof(Piece) * pt->piece_capacity);
            }
            memmove(&pt->pieces[start_idx + 2], &pt->pieces[start_idx + 1], sizeof(Piece) * (pt->num_pieces - start_idx - 1));
            pt->pieces[start_idx] = left;
            pt->pieces[start_idx + 1] = right;
            pt->num_pieces++;
        }
    } else {
        pt->pieces[start_idx].length = start_offset;
        pt->pieces[end_idx].start += end_offset;
        pt->pieces[end_idx].length -= end_offset;

        size_t pieces_to_remove = (end_idx - start_idx) - 1;
        if (pt->pieces[end_idx].length == 0) pieces_to_remove++;
        if (pt->pieces[start_idx].length == 0) {
            pieces_to_remove++;
            start_idx--;
        }

        if (pieces_to_remove > 0) {
            memmove(&pt->pieces[start_idx + 1], &pt->pieces[start_idx + pieces_to_remove + 1], sizeof(Piece) * (pt->num_pieces - (start_idx + pieces_to_remove + 1)));
            pt->num_pieces -= pieces_to_remove;
        }
    }
    pt->logical_size -= len;
}

char *ptGetText(PieceTable *pt) {
    char *buf = safeMalloc(pt->logical_size + 1);
    size_t current_pos = 0;
    for (size_t i = 0; i < pt->num_pieces; i++) {
        Piece p = pt->pieces[i];
        if (p.length == 0) continue;

        char *source_buf = (p.source == BUFFER_ORIGINAL) ? pt->orig_buf : pt->add_buf;
        memcpy(buf + current_pos, source_buf + p.start, p.length);
        current_pos += p.length;
    }
    buf[pt->logical_size] = '\0';
    return buf;
}

bool ptFindPiece(PieceTable *pt, size_t offset, size_t *piece_idx, size_t *piece_offset) {
    if (offset > pt->logical_size) return false;

    size_t current_offset = 0;
    for (size_t i = 0; i < pt->num_pieces; i++) {
        if (current_offset + pt->pieces[i].length > offset) {
            *piece_idx = i;
            *piece_offset = offset - current_offset;
            return true;
        }
        current_offset += pt->pieces[i].length;
    }

    if (offset == pt->logical_size && pt->num_pieces > 0) {
        *piece_idx = pt->num_pieces - 1;
        *piece_offset = pt->pieces[*piece_idx].length;
        return true;
    }
    return false;
}

void ptReadLogical(PieceTable *pt, size_t offset, size_t length, char *out_buf) {
    size_t bytes_read = 0;
    size_t piece_idx, piece_offset;
    if (!ptFindPiece(pt, offset, &piece_idx, &piece_offset)) return;

    while (bytes_read < length && piece_idx < pt->num_pieces) {
        Piece p = pt->pieces[piece_idx];
        char *source = (p.source == BUFFER_ORIGINAL) ? pt->orig_buf : pt->add_buf;

        size_t available_in_piece = p.length - piece_offset;
        size_t bytes_to_copy = (length - bytes_read < available_in_piece) ? (length - bytes_read) : available_in_piece;

        memcpy(out_buf + bytes_read, source + p.start + piece_offset, bytes_to_copy);
        bytes_read += bytes_to_copy;
        piece_idx++;
        piece_offset = 0;
    }
    out_buf[bytes_read] = '\0';
}

void editorUpdateLineOffsets(EditorBuffer *buf) {
    if (buf->line_capacity == 0) {
        buf->line_capacity = 1024;
        buf->line_offsets = safeMalloc(sizeof(size_t) * buf->line_capacity);
    }

    buf->line_offsets[0] = 0;
    buf->num_lines = 1;

    size_t current_logical_offset = 0;
    for (size_t i = 0; i < buf->pt.num_pieces; i++) {
        Piece p = buf->pt.pieces[i];
        if (p.length == 0) continue;

        char *source_buf = (p.source == BUFFER_ORIGINAL) ? buf->pt.orig_buf : buf->pt.add_buf;
        for (size_t j = 0; j < p.length; j++) {
            if (source_buf[p.start + j] == '\n') {
                if (buf->num_lines >= buf->line_capacity) {
                    buf->line_capacity *= 2;
                    buf->line_offsets = safeRealloc(buf->line_offsets, sizeof(size_t) * buf->line_capacity);
                }
                buf->line_offsets[buf->num_lines++] = current_logical_offset + j + 1;
            }
        }
        current_logical_offset += p.length;
    }
}

char *editorGetLine(EditorBuffer *buf, int line_idx, size_t *line_len) {
    if (line_idx < 0 || line_idx >= buf->num_lines) return NULL;

    size_t start_offset = buf->line_offsets[line_idx];
    size_t end_offset;
    if (line_idx == buf->num_lines - 1)
        end_offset = buf->pt.logical_size;
    else
        end_offset = buf->line_offsets[line_idx + 1] - 1;

    *line_len = end_offset - start_offset;
    char *line_text = safeMalloc(*line_len + 1);

    ptReadLogical(&buf->pt, start_offset, *line_len, line_text);
    return line_text;
}

size_t editorGetLineLength(EditorBuffer *buf, int line_idx) {
    if (line_idx < 0 || line_idx >= buf->num_lines) return 0;

    size_t start_offset = buf->line_offsets[line_idx];
    size_t end_offset;
    if (line_idx == buf->num_lines - 1)
        end_offset = buf->pt.logical_size;
    else
        end_offset = buf->line_offsets[line_idx + 1] - 1;
    return end_offset - start_offset;
}

size_t editorGetLogicalOffset(EditorBuffer *buf, int cursor_y, int cursor_x) {
    if (buf->num_lines == 0) return 0;

    if (cursor_y >= buf->num_lines) cursor_y = buf->num_lines - 1;
    if (cursor_y < 0) cursor_y = 0;

    size_t line_start = buf->line_offsets[cursor_y];
    size_t line_len = editorGetLineLength(buf, cursor_y);

    if ((size_t)cursor_x > line_len) cursor_x = line_len;
    if (cursor_x < 0) cursor_x = 0;
    return line_start + cursor_x;
}

void editorOffsetToRowCol(EditorBuffer *buf, size_t offset, int *row, int *col) {
    if (buf->num_lines == 0) {
        *row = 0; *col = 0;
        return;
    }

    for (int i = 0; i < buf->num_lines; i++) {
        size_t start = buf->line_offsets[i];
        size_t end = (i == buf->num_lines - 1) ? buf->pt.logical_size : buf->line_offsets[i + 1];
        if (offset >= start && (offset < end || (offset == end && i == buf->num_lines - 1))) {
            *row = i;
            *col = offset - start;
            return;
        }
    }

    *row = buf->num_lines - 1;
    *col = offset - buf->line_offsets[*row];
}

void editorInsertChar(int ch) {
    if (E.sel.active) {
        editorDeleteSelectedText();
        E.sel.active = false;
    }

    size_t offset = editorGetLogicalOffset(&E.buf, E.cursor.y, E.cursor.x);
    char c = ch;
    executeInsert(offset, &c, 1);
    E.cursor.x++;

    char closing_char = getClosingChar(ch);
    if (!E.sel.is_pasting && closing_char)
        executeInsert(offset + 1, &closing_char, 1);

    E.cursor.preferred_x = E.cursor.x;
    E.buf.dirty = true;
    editorUpdateLineOffsets(&E.buf);
}

void editorDeleteChar(int is_backspace) {
    if (E.sel.active) {
        editorDeleteSelectedText();
        return;
    }

    if (E.buf.num_lines == 0) return;
    if (E.cursor.x == 0 && E.cursor.y == 0 && is_backspace) return;

    size_t offset = editorGetLogicalOffset(&E.buf, E.cursor.y, E.cursor.x);
    if (is_backspace) {
        char *full_doc = ptGetText(&E.buf.pt);
        char prev_char = (offset > 0) ? full_doc[offset - 1] : '\0';
        char next_char = (offset < E.buf.pt.logical_size) ? full_doc[offset] : '\0';
        char closing_char = getClosingChar(prev_char);
        if (closing_char && next_char == closing_char)
            executeDelete(offset - 1, 2); 
        else
            executeDelete(offset - 1, 1);

        free(full_doc);
        if (E.cursor.x == 0) {
            E.cursor.y--;
            E.cursor.x = editorGetLineLength(&E.buf, E.cursor.y);
        }
        else
            E.cursor.x--;
    }
    else if (offset < E.buf.pt.logical_size)
        executeDelete(offset, 1);

    E.cursor.preferred_x = E.cursor.x;
    E.buf.dirty = true;
    editorUpdateLineOffsets(&E.buf);
}

void editorInsertNewline() {
    size_t offset = editorGetLogicalOffset(&E.buf, E.cursor.y, E.cursor.x);
    size_t line_len;
    char *line_text = editorGetLine(&E.buf, E.cursor.y, &line_len);

    int indent_len = 0;
    if (line_text) {
        while ((size_t)indent_len < line_len && (line_text[indent_len] == ' ' || line_text[indent_len] == '\t')) indent_len++;
        if (indent_len > E.cursor.x) indent_len = E.cursor.x;
        if (E.sel.is_pasting) indent_len = 0;
    }

    char *insert_str = safeMalloc(indent_len + 2);
    insert_str[0] = '\n';
    if (indent_len > 0)
        memcpy(insert_str + 1, line_text, indent_len);
    insert_str[indent_len + 1] = '\0';

    executeInsert(offset, insert_str, indent_len + 1);
    if (E.cursor.x > 0 && line_text && line_len > (size_t)E.cursor.x && (line_text[E.cursor.x - 1] == '{' || line_text[E.cursor.x - 1] == '[' || line_text[E.cursor.x - 1] == '(')) {
        if (line_text[E.cursor.x] == getClosingChar(line_text[E.cursor.x - 1])) {
            int new_indent_len = indent_len + TAB_SIZE;
            char *block_indent = safeMalloc(new_indent_len + 2);
            block_indent[0] = '\n';
            memset(block_indent + 1, ' ', new_indent_len);
            block_indent[new_indent_len + 1] = '\0';

            executeInsert(offset + indent_len + 1, block_indent, new_indent_len + 1);
            free(block_indent);

            E.cursor.y++;
            E.cursor.x = new_indent_len;
            E.cursor.preferred_x = E.cursor.x;

            free(insert_str);
            free(line_text);
            editorUpdateLineOffsets(&E.buf);
            return;
        }
    }

    E.cursor.y++;
    E.cursor.x = indent_len;
    E.cursor.preferred_x = E.cursor.x;
    E.buf.dirty = true;

    free(insert_str);
    if (line_text) free(line_text);
    editorUpdateLineOffsets(&E.buf);
}

void editorMoveRowUp() {
    if (E.cursor.y <= 0) return;
    E.sel.active = false;

    size_t prev_start = E.buf.line_offsets[E.cursor.y - 1];
    size_t current_start = E.buf.line_offsets[E.cursor.y];
    size_t current_end = (E.cursor.y == E.buf.num_lines - 1) ? E.buf.pt.logical_size : E.buf.line_offsets[E.cursor.y + 1];

    size_t prev_len = current_start - prev_start;
    size_t current_len = current_end - current_start;

    char *prev_text = safeMalloc(prev_len + 1);
    ptReadLogical(&E.buf.pt, prev_start, prev_len, prev_text);
    char *current_text = safeMalloc(current_len + 1);
    ptReadLogical(&E.buf.pt, current_start, current_len, current_text);

    bool prev_has_nl = (prev_len > 0 && prev_text[prev_len - 1] == '\n');
    bool curr_has_nl = (current_len > 0 && current_text[current_len - 1] == '\n');
    if (prev_has_nl && !curr_has_nl) {
        prev_text[prev_len - 1] = '\0';
        prev_len--;

        current_text = safeRealloc(current_text, current_len + 2);
        current_text[current_len] = '\n';
        current_text[current_len + 1] = '\0';
        current_len++;
    }

    executeDelete(prev_start, prev_len + current_len);
    executeInsert(prev_start, current_text, current_len);
    executeInsert(prev_start + current_len, prev_text, prev_len);

    free(prev_text);
    free(current_text);
    E.cursor.y--;
    E.buf.dirty = true;
    editorUpdateLineOffsets(&E.buf);
}

void editorMoveRowDown() {
    if (E.cursor.y >= E.buf.num_lines - 1) return;
    E.sel.active = false;

    size_t current_start = E.buf.line_offsets[E.cursor.y];
    size_t next_start = E.buf.line_offsets[E.cursor.y + 1];
    size_t next_end = (E.cursor.y + 1 == E.buf.num_lines - 1) ? E.buf.pt.logical_size : E.buf.line_offsets[E.cursor.y + 2];

    size_t current_len = next_start - current_start;
    size_t next_len = next_end - next_start;

    char *current_text = safeMalloc(current_len + 1);
    ptReadLogical(&E.buf.pt, current_start, current_len, current_text);

    char *next_text = safeMalloc(next_len + 1);
    ptReadLogical(&E.buf.pt, next_start, next_len, next_text);

    bool curr_has_nl = (current_len > 0 && current_text[current_len - 1] == '\n');
    bool next_has_nl = (next_len > 0 && next_text[next_len - 1] == '\n');
    if (curr_has_nl && !next_has_nl) {
        current_text[current_len - 1] = '\0';
        current_len--;

        next_text = safeRealloc(next_text, next_len + 2);
        next_text[next_len] = '\n';
        next_text[next_len + 1] = '\0';
        next_len++;
    }

    executeDelete(current_start, current_len + next_len);
    executeInsert(current_start, next_text, next_len);
    executeInsert(current_start + next_len, current_text, current_len);

    free(current_text);
    free(next_text);
    E.cursor.y++;
    E.buf.dirty = true;
    editorUpdateLineOffsets(&E.buf);
}

void editorCopyRowUp() {
    E.sel.active = false;
    size_t current_start = E.buf.line_offsets[E.cursor.y];
    size_t current_end = (E.cursor.y == E.buf.num_lines - 1) ? E.buf.pt.logical_size : E.buf.line_offsets[E.cursor.y + 1];
    size_t current_len = current_end - current_start;

    char *current_text = safeMalloc(current_len + 2);
    ptReadLogical(&E.buf.pt, current_start, current_len, current_text);

    bool curr_has_nl = (current_len > 0 && current_text[current_len - 1] == '\n');
    if (!curr_has_nl) {
        current_text[current_len] = '\n';
        current_text[current_len + 1] = '\0';
        current_len++;
    }

    executeInsert(current_start, current_text, current_len);
    free(current_text);
    E.cursor.y++;
    E.buf.dirty = true;
    editorUpdateLineOffsets(&E.buf);
}

void editorCopyRowDown() {
    E.sel.active = false;
    size_t current_start = E.buf.line_offsets[E.cursor.y];
    size_t current_end = (E.cursor.y == E.buf.num_lines - 1) ? E.buf.pt.logical_size : E.buf.line_offsets[E.cursor.y + 1];
    size_t current_len = current_end - current_start;
    char *current_text = safeMalloc(current_len + 2);
    ptReadLogical(&E.buf.pt, current_start, current_len, current_text);

    bool curr_has_nl = (current_len > 0 && current_text[current_len - 1] == '\n');
    if (!curr_has_nl) {
        char *insert_text = safeMalloc(current_len + 2);
        insert_text[0] = '\n';
        memcpy(insert_text + 1, current_text, current_len);
        insert_text[current_len + 1] = '\0';

        executeInsert(current_end, insert_text, current_len + 1);
        free(insert_text);
    } else {
        executeInsert(current_end, current_text, current_len);
    }

    free(current_text);
    E.buf.dirty = true;
    editorUpdateLineOffsets(&E.buf);
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
    if (E.buf.num_lines > 0) {
        E.sel.active = true;
        E.sel.sx = 0;
        E.sel.sy = 0;
        E.sel.ey = E.buf.num_lines - 1;
        E.sel.ex = editorGetLineLength(&E.buf, E.sel.ey);
        E.cursor.x = E.sel.ex;
        E.cursor.y = E.sel.ey;

        char msg[STATUS_LENGTH];
        snprintf(msg, sizeof(msg), "Selected all %d lines", E.buf.num_lines);
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
        x1 = x2; y1 = y2;
        x2 = tmpx; y2 = tmpy;
    }

    size_t start_offset = editorGetLogicalOffset(&E.buf, y1, x1);
    size_t end_offset = editorGetLogicalOffset(&E.buf, y2, x2);
    size_t total_len = end_offset - start_offset;
    if (len_out) *len_out = total_len;
    if (total_len == 0) return NULL;

    char *buf = safeMalloc(total_len + 1);
    ptReadLogical(&E.buf.pt, start_offset, total_len, buf);

    return buf;
}

void editorDeleteSelectedText() {
    if (!E.sel.active) return;

    int y1 = E.sel.sy, x1 = E.sel.sx;
    int y2 = E.sel.ey, x2 = E.sel.ex;
    if (y1 > y2 || (y1 == y2 && x1 > x2)) {
        int tmpx = x1, tmpy = y1;
        x1 = x2; y1 = y2;
        x2 = tmpx; y2 = tmpy;
    }

    size_t start_offset = editorGetLogicalOffset(&E.buf, y1, x1);
    size_t end_offset = editorGetLogicalOffset(&E.buf, y2, x2);
    size_t length_to_delete = end_offset - start_offset;
    if (length_to_delete > 0)
        executeDelete(start_offset, length_to_delete);

    E.cursor.x = x1;
    E.cursor.y = y1;
    E.cursor.preferred_x = E.cursor.x;
    E.sel.active = false;
    E.buf.dirty = true;
    editorUpdateLineOffsets(&E.buf);
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
    if (E.cursor.y >= E.buf.num_lines) return;

    size_t start_offset = editorGetLogicalOffset(&E.buf, E.cursor.y, 0);
    size_t line_len = editorGetLineLength(&E.buf, E.cursor.y);

    size_t delete_len = line_len;
    if (E.cursor.y < E.buf.num_lines - 1) delete_len += 1;

    char *line_content = safeMalloc(delete_len + 1);
    ptReadLogical(&E.buf.pt, start_offset, delete_len, line_content);

    free(E.sel.clipboard);
    E.sel.clipboard = line_content;
    clipboardCopyToSystem(E.sel.clipboard, delete_len);

    executeDelete(start_offset, delete_len);
    if (E.cursor.y >= E.buf.num_lines && E.buf.num_lines > 0)
        E.cursor.y = E.buf.num_lines - 1;

    int current_len = editorGetLineLength(&E.buf, E.cursor.y);
    E.cursor.x = E.cursor.x > current_len ? current_len : E.cursor.x;
    E.cursor.preferred_x = E.cursor.x;

    char sizebuf[SMALL_BUFFER_SIZE];
    humanReadableSize(delete_len, sizebuf, sizeof(sizebuf));
    char msg[STATUS_LENGTH];
    snprintf(msg, sizeof(msg), "Cut %s", sizebuf);
    editorSetStatusMsg(msg);
    
    E.buf.dirty = true;
    editorUpdateLineOffsets(&E.buf);
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

        char *full_text = ptGetText(&E.buf.pt);
        char *ptr = full_text;
        int query_len = strlen(query);
        while ((ptr = strstr(ptr, query)) != NULL) {
            E.find.match_lines = safeRealloc(E.find.match_lines, sizeof(int) * (E.find.num_matches + 1));
            E.find.match_cols = safeRealloc(E.find.match_cols, sizeof(int) * (E.find.num_matches + 1));

            size_t match_offset = ptr - full_text;
            int match_row, match_col;
            editorOffsetToRowCol(&E.buf, match_offset, &match_row, &match_col);

            E.find.match_lines[E.find.num_matches] = match_row;
            E.find.match_cols[E.find.num_matches] = match_col;
            E.find.num_matches++;
            ptr += query_len;
        }
        free(full_text);

        if (E.find.num_matches > 0) {
            E.find.current_idx = 0;
            int row = E.find.match_lines[0];
            int col = E.find.match_cols[0];
            E.cursor.x = col;
            E.cursor.y = row;
            E.view.row_offset = E.buf.num_lines;
            E.cursor.preferred_x = E.cursor.x;

            if (row < E.view.row_offset)
                E.view.row_offset = row;
            else if (row >= E.view.row_offset + E.view.screen_rows)
                E.view.row_offset = row - E.view.screen_rows + MARGIN;

            size_t render_line_len;
            char *render_line_text = editorGetLine(&E.buf, row, &render_line_len);
            int render_pos = editorLineCxToRx(render_line_text, render_line_len, E.cursor.x);
            if (render_line_text)
                free(render_line_text);

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

            size_t render_line_len;
            char *render_line_text = editorGetLine(&E.buf, row, &render_line_len);
            int render_pos = editorLineCxToRx(render_line_text, render_line_len, E.cursor.x);
            if (render_line_text)
                free(render_line_text);

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
    (void)line;
    free(E.find.match_lines);
    free(E.find.match_cols);
    E.find.match_lines = NULL;
    E.find.match_cols = NULL;
    E.find.num_matches = 0;
    if (!query) return;

    char *full_text = ptGetText(&E.buf.pt);
    char *ptr = full_text;
    int query_len = strlen(query);
    while ((ptr = strstr(ptr, query)) != NULL) {
        E.find.match_lines = safeRealloc(E.find.match_lines, sizeof(int) * (E.find.num_matches + 1));
        E.find.match_cols = safeRealloc(E.find.match_cols, sizeof(int) * (E.find.num_matches + 1));

        size_t match_offset = ptr - full_text;
        int match_row, match_col;
        editorOffsetToRowCol(&E.buf, match_offset, &match_row, &match_col);

        E.find.match_lines[E.find.num_matches] = match_row;
        E.find.match_cols[E.find.num_matches] = match_col;
        E.find.num_matches++;
        ptr += query_len;
    }
    free(full_text);
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
                idx = (idx + E.find.num_matches - 1) % E.find.num_matches;
                E.find.current_idx = idx;
                editorReplaceJumpToCurrent();
                break;
            case 'a':
            case 'A':
                replaced += editorReplaceAll(replace_query);
                done = true;
                break;
            case '\r':
            case '\n':
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

        char *full_text = ptGetText(&E.buf.pt);
        char *ptr = full_text;
        int query_len = strlen(query);
        while ((ptr = strstr(ptr, query)) != NULL) {
            E.find.match_lines = safeRealloc(E.find.match_lines, sizeof(int) * (E.find.num_matches + 1));
            E.find.match_cols = safeRealloc(E.find.match_cols, sizeof(int) * (E.find.num_matches + 1));

            size_t match_offset = ptr - full_text;
            int match_row, match_col;
            editorOffsetToRowCol(&E.buf, match_offset, &match_row, &match_col);

            E.find.match_lines[E.find.num_matches] = match_row;
            E.find.match_cols[E.find.num_matches] = match_col;
            E.find.num_matches++;
            ptr += query_len;
        }
        free(full_text);

        if (E.find.num_matches > 0) {
            E.find.current_idx = 0;
            int row = E.find.match_lines[0];
            int col = E.find.match_cols[0];
            E.cursor.y = row;
            E.cursor.x = col;
            E.view.row_offset = (row >= E.view.screen_rows) ? row - E.view.screen_rows + MARGIN : 0;
            size_t render_line_len;
            char *render_line_text = editorGetLine(&E.buf, row, &render_line_len);
            int render_pos = editorLineCxToRx(render_line_text, render_line_len, E.cursor.x);
            if (render_line_text)
                free(render_line_text);

            int margin = query_len + MARGIN;
            E.view.col_offset = (render_pos > margin) ? render_pos - (E.view.screen_cols - margin) : 0;
            if (E.view.col_offset < 0)
                E.view.col_offset = 0;
        }
        E.find.active = true;
    }
}

int editorReplaceAll(const char *replace_str) {
    if (!E.find.query || !replace_str) return 0;
    int replaced_count = 0;

    editorBeginMacro();
    int find_len = strlen(E.find.query);
    int replace_len = strlen(replace_str);
    for (int i = E.find.num_matches - 1; i >= 0; i--) {
        int row = E.find.match_lines[i];
        int col = E.find.match_cols[i];
        size_t offset = editorGetLogicalOffset(&E.buf, row, col);

        executeDelete(offset, find_len);
        executeInsert(offset, replace_str, replace_len);
        replaced_count++;
    }

    editorEndMacro();
    if (replaced_count > 0) {
        E.buf.dirty = true;
        editorUpdateLineOffsets(&E.buf);
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

        size_t render_line_len;
        char *render_line_text = editorGetLine(&E.buf, row, &render_line_len);
        int render_pos = editorLineCxToRx(render_line_text, render_line_len, E.cursor.x);
        if (render_line_text)
            free(render_line_text);

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
    size_t offset = editorGetLogicalOffset(&E.buf, row, col);

    int find_len = strlen(find_str);
    int replace_len = strlen(replace_str);

    editorBeginMacro();
    executeDelete(offset, find_len);
    executeInsert(offset, replace_str, replace_len);
    editorEndMacro();

    E.buf.dirty = true;
    editorUpdateLineOffsets(&E.buf);

    E.cursor.y = row;
    E.cursor.x = col + replace_len;
    E.cursor.preferred_x = E.cursor.x;
    return true;
}

void freeEditCommand(EditCommand *cmd) {
    if (!cmd->text) return;
    free(cmd->text);
    cmd->text = NULL;
}

void editorBeginMacro() {
    history.in_transaction = true;
    history.current_transaction_id++;
}

void editorEndMacro() {
    history.in_transaction = false;
}

void recordCommand(CommandType type, size_t offset, const char *text, size_t len, EditorCursor cursor) {
    long now = currentMillis();
    for (int i = 0; i <= history.redo_top; i++)
        freeEditCommand(&history.redo_stack[i]);
    history.redo_top = -1;

    if (history.save_point > history.undo_top)
        history.save_point = -2;

    bool merged = false;
    if (history.undo_top >= 0 && !history.in_transaction) {
        EditCommand *last_cmd = &history.undo_stack[history.undo_top];
        long time_elapsed = now - history.last_edit_time;

        if (time_elapsed < UNDO_TIMEOUT_MS && last_cmd->type == type && last_cmd->transaction_id == 0) {
            if (type == CMD_INSERT && offset == last_cmd->offset + last_cmd->len) {
                last_cmd->text = safeRealloc(last_cmd->text, last_cmd->len + len + 1);
                memcpy(last_cmd->text + last_cmd->len, text, len);
                last_cmd->len += len;
                last_cmd->text[last_cmd->len] = '\0';
                merged = true;
            } 
            else if (type == CMD_DELETE) {
                if (offset + len == last_cmd->offset) {
                    char *new_text = safeMalloc(last_cmd->len + len + 1);
                    memcpy(new_text, text, len);
                    memcpy(new_text + len, last_cmd->text, last_cmd->len);
                    new_text[last_cmd->len + len] = '\0';
                    free(last_cmd->text);
                    last_cmd->text = new_text;
                    last_cmd->offset = offset;
                    last_cmd->len += len;
                    merged = true;
                } else if (offset == last_cmd->offset) {
                    last_cmd->text = safeRealloc(last_cmd->text, last_cmd->len + len + 1);
                    memcpy(last_cmd->text + last_cmd->len, text, len);
                    last_cmd->len += len;
                    last_cmd->text[last_cmd->len] = '\0';
                    merged = true;
                }
            }
        }
    }

    if (!merged) {
        if (history.undo_top >= UNDO_REDO_STACK_SIZE - 1) {
            freeEditCommand(&history.undo_stack[0]);
            memmove(&history.undo_stack[0], &history.undo_stack[1], sizeof(EditCommand) * (UNDO_REDO_STACK_SIZE - 1));
            history.undo_top--;

            if (history.save_point >= 0)
                history.save_point--;
            else if (history.save_point == -1)
                history.save_point = -2;
        }

        history.undo_top++;
        EditCommand *cmd = &history.undo_stack[history.undo_top];
        cmd->type = type;
        cmd->offset = offset;
        cmd->len = len;
        cmd->text = safeMalloc(len + 1);
        memcpy(cmd->text, text, len);
        cmd->text[len] = '\0';
        cmd->cursor = cursor;
        cmd->transaction_id = history.in_transaction ? history.current_transaction_id : 0; 
    }
    history.last_edit_time = now;
}

void executeInsert(size_t offset, const char *text, size_t len) {
    if (len == 0) return;

    recordCommand(CMD_INSERT, offset, text, len, E.cursor);

    ptInsert(&E.buf.pt, offset, text, len);
    E.buf.dirty = true;
    editorUpdateLineOffsets(&E.buf);
}

void executeDelete(size_t offset, size_t len) {
    if (len == 0) return;

    char *deleted_text = safeMalloc(len + 1);
    ptReadLogical(&E.buf.pt, offset, len, deleted_text);

    recordCommand(CMD_DELETE, offset, deleted_text, len, E.cursor);
    free(deleted_text);

    ptDelete(&E.buf.pt, offset, len);
    E.buf.dirty = true;
    editorUpdateLineOffsets(&E.buf);
}

void editorUndo() {
    if (history.undo_top < 0) {
        editorSetStatusMsg("Nothing to undo");
        return;
    }

    int target_transaction = history.undo_stack[history.undo_top].transaction_id;
    bool is_macro = (target_transaction != 0);

    do {
        if (history.undo_top < 0) break;
        EditCommand *cmd = &history.undo_stack[history.undo_top];
        if (is_macro && cmd->transaction_id != target_transaction) break;

        history.redo_top++;
        history.redo_stack[history.redo_top] = *cmd;
        history.redo_stack[history.redo_top].text = safeStrdup(cmd->text);

        if (cmd->type == CMD_INSERT)
            ptDelete(&E.buf.pt, cmd->offset, cmd->len);
        else if (cmd->type == CMD_DELETE)
            ptInsert(&E.buf.pt, cmd->offset, cmd->text, cmd->len);

        E.cursor = cmd->cursor;
        freeEditCommand(cmd);
        history.undo_top--;
    } while (is_macro);

    editorUpdateLineOffsets(&E.buf);
    E.buf.dirty = (history.undo_top != history.save_point);
    editorSetStatusMsg("Undid");
}

void editorRedo() {
    if (history.redo_top < 0) {
        editorSetStatusMsg("Nothing to redo");
        return;
    }

    int target_transaction = history.redo_stack[history.redo_top].transaction_id;
    bool is_macro = (target_transaction != 0);

    do {
        if (history.redo_top < 0) break;
        EditCommand *cmd = &history.redo_stack[history.redo_top];
        if (is_macro && cmd->transaction_id != target_transaction) break;

        history.undo_top++;
        history.undo_stack[history.undo_top] = *cmd;
        history.undo_stack[history.undo_top].text = safeStrdup(cmd->text);

        if (cmd->type == CMD_INSERT)
            ptInsert(&E.buf.pt, cmd->offset, cmd->text, cmd->len);
        else if (cmd->type == CMD_DELETE)
            ptDelete(&E.buf.pt, cmd->offset, cmd->len);

        freeEditCommand(cmd);
        history.redo_top--;
    } while (is_macro);

    editorUpdateLineOffsets(&E.buf);
    E.buf.dirty = (history.undo_top != history.save_point);
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
    if (cursor_y >= E.buf.num_lines) return false;
    
    size_t line_len;
    char *line_text = editorGetLine(&E.buf, cursor_y, &line_len);
    if (!line_text || (size_t)cursor_x >= line_len) {
        if (line_text) free(line_text);
        return false;
    }
    char bracket = line_text[cursor_x];
    free(line_text);

    if (!(bracket == '(' || bracket == ')' || bracket == '{' || bracket == '}' || bracket == '[' || bracket == ']'))
        return false;

    char match = getMatchingBracket(bracket);
    int direction = (bracket == '(' || bracket == '{' || bracket == '[') ? 1 : -1;
    int count = 1;

    char *full_text = ptGetText(&E.buf.pt);
    size_t current_offset = editorGetLogicalOffset(&E.buf, cursor_y, cursor_x);
    while (1) {
        if (direction == 1) {
            current_offset++;
            if (current_offset >= E.buf.pt.logical_size) break;
        } else {
            if (current_offset == 0) break;
            current_offset--;
        }

        char ch = full_text[current_offset];
        if (ch == bracket) count++;
        else if (ch == match) count--;

        if (count == 0) {
            editorOffsetToRowCol(&E.buf, current_offset, match_y, match_x);
            free(full_text);
            return true;
        }
    }
    free(full_text);
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

void editorOpen(const char *filename) {
    free(E.buf.filename);
    E.buf.filename = safeStrdup(filename);

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        if (errno == ENOENT) {
            ptInit(&E.buf.pt, "", 0);
            editorUpdateLineOffsets(&E.buf);
            E.buf.dirty = false;
            return;
        }
        die("fopen");
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size > 0) {
        char *buffer = safeMalloc(file_size + 1);
        size_t read_size = fread(buffer, 1, file_size, fp);
        buffer[read_size] = '\0';
        ptInit(&E.buf.pt, buffer, read_size);
        free(buffer);
    }
    else
        ptInit(&E.buf.pt, "", 0);

    fclose(fp);
    editorUpdateLineOffsets(&E.buf);
    E.buf.dirty = false;
    history.save_point = history.undo_top;
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

    mode_t file_mode = DEFAULT_FILE_PERMS;
    struct stat st;
    if (stat(E.buf.filename, &st) == 0)
        file_mode = st.st_mode; // keep existing permissions
    int fd = open(tmp_filename,
                  O_RDWR  |     // read and write
                  O_CREAT |     // create if doesn't exist
                  O_TRUNC,      // clear the file
                  file_mode);   // permissions
    if (fd == -1) {
        free(tmp_filename);
        char msg[STATUS_LENGTH];
        snprintf(msg, sizeof(msg), "Can't save! I/O error: %s", strerror(errno));
        editorSetStatusMsg(msg);
        return;
    }

    int total_bytes = E.buf.pt.logical_size;
    bool success = true;
    if (total_bytes > 0) {
        char *full_text = ptGetText(&E.buf.pt);
        if (write(fd, full_text, total_bytes) != total_bytes)
            success = false;
        free(full_text);
    }

    if (success)
        if (fsync(fd) == -1)
            success = false;
    close(fd);

    if (success) {
        E.buf.dirty = false;
        E.buf.quit_times = QUIT_TIMES;
        history.save_point = history.undo_top;
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

void panicSave(int signum) {
    if (E.buf.filename && E.buf.pt.pieces) {
        char path[BUFFER_SIZE];
        snprintf(path, sizeof(path), "%s.crash", E.buf.filename);

        int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, DEFAULT_FILE_PERMS);
        if (fd != -1) {
            for (size_t i = 0; i < E.buf.pt.num_pieces; i++) {
                Piece p = E.buf.pt.pieces[i];
                if (p.length == 0) continue;

                char *source = (p.source == BUFFER_ORIGINAL) ? E.buf.pt.orig_buf : E.buf.pt.add_buf;
                write(fd, source + p.start, p.length);
            }
            close(fd);
        }
    }
    signal(signum, SIG_DFL);
    raise(signum);
}

char *editorRowsToString(size_t *buf_len) {
    *buf_len = E.buf.pt.logical_size;
    return ptGetText(&E.buf.pt);
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
    if (E.buf.num_lines == 0) {
        E.cursor.x = 0;
        E.cursor.y = 0;
        return;
    }

    if (E.cursor.y < 0)
        E.cursor.y = 0;
    else if (E.cursor.y >= E.buf.num_lines) {
        E.cursor.y = E.buf.num_lines - 1;
        E.cursor.x = editorGetLineLength(&E.buf, E.cursor.y);
        return;
    }

    int current_line_len = editorGetLineLength(&E.buf, E.cursor.y);
    if (E.cursor.x < 0)
        E.cursor.x = 0;
    else if (E.cursor.x > current_line_len)
        E.cursor.x = current_line_len;
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

int editorLineCxToRx(const char *chars, int size, int cursor_x) {
    int render_x = 0;
    for (int i = 0; i < cursor_x && i < size; i++) {
        if (chars[i] == '\t')
            render_x += (TAB_SIZE - 1) - (render_x % TAB_SIZE);
        render_x++;
    }
    return render_x;
}

int editorLineRxToCx(const char *chars, int size, int render_x) {
    int cur_render_x = 0;
    int cursor_x;
    for (cursor_x = 0; cursor_x < size; cursor_x++) {
        if (chars[cursor_x] == '\t')
            cur_render_x += (TAB_SIZE - 1) - (cur_render_x % TAB_SIZE);
        cur_render_x++;
        if (cur_render_x > render_x) return cursor_x;
    }
    return cursor_x;
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

void abAppend(AppendBuffer *ab, const char *str, int len) {
    if (ab->len + len >= ab->capacity) {
        int new_capacity = ab->capacity == 0 ? DEFAULT_AB_CAPACITY : ab->capacity * 2;
        while (new_capacity < ab->len + len)
            new_capacity *= 2;

        ab->b = safeRealloc(ab->b, new_capacity);
        ab->capacity = new_capacity;
    }
    memcpy(&ab->b[ab->len], str, len);
    ab->len += len;
}

void abFree(AppendBuffer *ab) {
    free(ab->b);
}