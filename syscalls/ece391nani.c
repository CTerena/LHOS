/* ece391nani.c -- source code for text editor nani */
#include <stdint.h>

#include "ece391support.h"
#include "ece391syscall.h"
#include "ece391vt.h"

int32_t ece391_memcpy(void* dest, const void* src, int32_t n);
void ece391_memset(void* memory, char c, int n);
int32_t ece391_getline(char* buf, int32_t fd, int32_t n);

#define NULL 0
#define NUM_TERM_COLS    (80 - 1)
#define NUM_TERM_ROWS    (25 - 1)
#define MAX_COLS         124
#define MAX_ROWS         1000
#define NANI_STATIC_BUF_ADDR 0x08000000
#define NANI_STATUS_BAR_LINEINFO_START 60

static int8_t keycode_to_printable_char[2][128] =
{
    { // Unshifted version
        '\0',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
        'k','l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
        'u','v', 'w', 'x', 'y', 'z',
        '`', '-', '=', '[', ']', '\\', ';', '\'', ',', '.', '/', ' ',
        // Remaining keys are not used but will be mapped to '\0' anyway
    },
    { // Shifted version
        '0',
        ')', '!', '@', '#', '$', '%', '^', '&', '*', '(',
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
        'K','L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
        'U','V', 'W', 'X', 'Y', 'Z',
        '~', '_', '+', '{', '}', '|', ':', '"', '<', '>', '?', ' ',
        // Remaining keys are not used but will be mapped to '\0' anyway
    }
};

enum NANI_mode {
    NANI_NORMAL,
    NANI_INSERT,
    NANI_COMMAND,
};

typedef struct {
    int shift;
    int caps;
    int ctrl;
    int alt;
} keyboard_state_t;

typedef struct erow {
    int size;
    int rsize;
    char chars[MAX_COLS];
    char render[2 * MAX_COLS];
} erow_t;

typedef struct nani_state {
    int screen_x, screen_y;
    int rowoff;
    int coloff;
    enum NANI_mode mode;
    keyboard_state_t kbd;
    int numrows;
    erow_t *row;
} nani_state_t;

struct append_buf {
    char buf[2000];
    int len;
};

char itoa(int n) {
    return n + '0';
}

nani_state_t NANI;

struct append_buf abuf;

void erow_update_render(erow_t *row) {
    int i, j=0;
    for (i = 0; i < row->size; i++) {
        row->render[j] = row->chars[i];
        j++;
    }
    row->rsize = j;
}

void erow_append_string(erow_t *row, char *s, int32_t len) {
    int at = row->size;
    row->size += len;
    ece391_memcpy(&row->chars[at], s, len);
    erow_update_render(row);
}

void erow_insert_char(erow_t *row, int at, char c) {
    if (at >= MAX_COLS) return;
    if (at < 0 || at > row->size) at = row->size;
    int i;
    for (i = row->size; i > at; i--) {
        row->chars[i] = row->chars[i - 1];
    }
    row->chars[at] = c;
    row->size++;
    erow_update_render(row);
}


void erow_delete_char(erow_t *row, int at) {
    if (at < 0 || at >= row->size) return;
    int i;
    for (i = at; i < row->size - 1; i++) {
        row->chars[i] = row->chars[i + 1];
    }
    row->size--;
    erow_update_render(row);
}

void NANI_delete_row(int at) {
    if (at < 0 || at >= NANI.numrows) return;
    int i;
    for (i = at; i < NANI.numrows - 1; i++) {
        ece391_memcpy(&NANI.row[i], &NANI.row[i + 1], sizeof(erow_t));
    }
    NANI.numrows--;
}


void NANI_insert_row(int at, char *s, int32_t len) {
    if (at < 0 || at > NANI.numrows || at >= MAX_ROWS) return;
    int i;
    for (i = NANI.numrows; i > at; i--) {
        ece391_memcpy(&NANI.row[i], &NANI.row[i - 1], sizeof(erow_t));
    }
    NANI.row[at].size = len;
    ece391_memcpy(NANI.row[at].chars, s, len);
    NANI.row[at].rsize = 0;
    erow_update_render(&NANI.row[at]);

    NANI.numrows++;
}

void NANI_delete_char() {
    if (NANI.screen_y == NANI.numrows) return;
    if (NANI.screen_x == 0 && NANI.screen_y == 0) return;
    erow_t *row = &NANI.row[NANI.screen_y];
    if (NANI.screen_x > 0) {
        erow_delete_char(row, NANI.screen_x - 1);
        NANI.screen_x--;
    } else {
        if (NANI.row[NANI.screen_y - 1].size + NANI.row[NANI.screen_y].size > MAX_COLS) return;
        NANI.screen_x = NANI.row[NANI.screen_y - 1].size;
        erow_append_string(&NANI.row[NANI.screen_y - 1], row->chars, row->size);
        NANI_delete_row(NANI.screen_y);
        NANI.screen_y--;
    }
}

void NANI_insert_newline() {
    if (NANI.screen_x == 0) {
        NANI_insert_row(NANI.screen_y, "", 0);
    } else {
        erow_t *row = &NANI.row[NANI.screen_y];
        NANI_insert_row(NANI.screen_y + 1, &row->chars[NANI.screen_x], row->size - NANI.screen_x);
        row->size = NANI.screen_x;
        erow_update_render(row);
    }
    NANI.screen_y++;
    NANI.screen_x = 0;
}

void abuf_append(struct append_buf *ab, const char *s, int len) {
    ece391_memcpy(&ab->buf[ab->len], s, len);
    ab->len += len;
}

static void NANI_clear_screen() {
    ece391_write(1, "\x1b[2J", 4);
    ece391_write(1, "\x1b[H", 3);
}

static void NANI_scroll() {
    if (NANI.screen_y < NANI.rowoff) {
        NANI.rowoff = NANI.screen_y;
    }
    if (NANI.screen_y >= NANI.rowoff + NUM_TERM_ROWS) {
        NANI.rowoff = NANI.screen_y - NUM_TERM_ROWS + 1;
    }
    if (NANI.screen_x < NANI.coloff) {
        NANI.coloff = NANI.screen_x;
    }
    if (NANI.screen_x >= NANI.coloff + NUM_TERM_COLS) {
        NANI.coloff = NANI.screen_x - NUM_TERM_COLS + 1;
    }
}

static void NANI_draw_status_bar() {
    // abuf_append(&abuf, "\x1b[7m", 4);
    char status[79];
    ece391_memset(status, ' ', 79);
    if (NANI.mode == NANI_NORMAL) {
        ece391_memcpy(status, "-- NORMAL --", 12);
    } else if (NANI.mode == NANI_INSERT) {
        ece391_memcpy(status, "-- INSERT --", 12);
    }
    int i = NANI_STATUS_BAR_LINEINFO_START;
    status[i] = itoa((NANI.screen_y + 1) / 1000);
    status[i + 1] = itoa(((NANI.screen_y + 1) / 100) % 10);
    status[i + 2] = itoa(((NANI.screen_y + 1) / 10) % 10);
    status[i + 3] = itoa((NANI.screen_y + 1) % 10);
    status[i + 4] = ',';
    status[i + 5] = itoa((NANI.screen_x + 1) / 100);
    status[i + 6] = itoa(((NANI.screen_x + 1) / 10) % 10);
    status[i + 7] = itoa((NANI.screen_x + 1) % 10);
    abuf_append(&abuf, status, 79);
}

static void NANI_update_screen() {
    NANI_scroll();
    
    // Reset cursor to topleft
    abuf_append(&abuf, "\x1b[H", 3);

    // Draw lines
    int scr_y;
    for (scr_y = 0; scr_y < NUM_TERM_ROWS; scr_y++) {
        int filerow = scr_y + NANI.rowoff;
        if (filerow >= NANI.numrows) {
            if (NANI.numrows == 0 && scr_y == NUM_TERM_ROWS / 3) {
                char welcome[NUM_TERM_COLS] = "NANI -- a simple editor";
                int left_padding = (NUM_TERM_COLS - ece391_strlen((uint8_t *)welcome)) / 2; 
                while (left_padding--)
                    abuf_append(&abuf, "~", 1);
                abuf_append(&abuf, welcome, ece391_strlen((uint8_t *)welcome));
            } else {
                abuf_append(&abuf, "~", 1);
            }
        } else {
            int len = NANI.row[filerow].rsize - NANI.coloff;
            if (len < 0)
                len = 0;
            if (len > NUM_TERM_COLS)
                len = NUM_TERM_COLS;
            abuf_append(&abuf, &NANI.row[filerow].render[NANI.coloff], len);
        }
        
        abuf_append(&abuf, "\x1b[K", 3);
        abuf_append(&abuf, "\n", 1);
    }

    NANI_draw_status_bar();

    // Draw cursor
    char cursor_cmd[8] = "\x1b[00;00H";
    cursor_cmd[2] = itoa((NANI.screen_y - NANI.rowoff) / 10);
    cursor_cmd[3] = itoa((NANI.screen_y - NANI.rowoff) % 10);
    cursor_cmd[5] = itoa((NANI.screen_x - NANI.coloff) / 10);
    cursor_cmd[6] = itoa((NANI.screen_x - NANI.coloff) % 10);
    abuf_append(&abuf, cursor_cmd, 8);

    ece391_write(1, abuf.buf, abuf.len);

    ece391_memset(abuf.buf, 0, abuf.len);
    abuf.len = 0;
}

static void NANI_move_cursor(char c) {
    erow_t *row = (NANI.screen_y >= NANI.numrows) ? NULL : &NANI.row[NANI.screen_y];

    switch (c) {
        case 'h':
            if (NANI.screen_x > 0)
                NANI.screen_x--;
            break;
        case 'j':
            if (NANI.screen_y < NANI.numrows)
                NANI.screen_y++;
            break;
        case 'k':
            if (NANI.screen_y > 0)
                NANI.screen_y--;
            break;
        case 'l':
            if (row && NANI.screen_x < row->size) {
                NANI.screen_x++;
            }
            break;
    }

    row = (NANI.screen_y >= NANI.numrows) ? NULL : &NANI.row[NANI.screen_y];
    int rowlen = row ? row->size : 0;
    if (NANI.screen_x > rowlen) {
        NANI.screen_x = rowlen;
    }
}

static void NANI_process_normal_key(char c) {
    if (c & 0x80) return; // ignore releases
    char printable_char;
    if (c >= KEY_A && c <= KEY_Z) {
        printable_char = keycode_to_printable_char[NANI.kbd.shift ^ NANI.kbd.caps][(int)c];
    } else {
        printable_char = keycode_to_printable_char[NANI.kbd.shift][(int)c];
    }
    switch (printable_char) {
        case 'x':
        case 'X':
            if (NANI.kbd.ctrl) {
                NANI_clear_screen();
                ece391_ioctl(1, 0); // Disable raw mode
                ece391_halt(0);
            }
            break;
        case 'b':
        case 'B':
            if (NANI.kbd.ctrl) {
                NANI.screen_y = NANI.rowoff;
                int i = NUM_TERM_ROWS;
                while (i--) {
                    NANI_move_cursor('k');
                }
            }
            break;
        case 'f':
        case 'F':
            if (NANI.kbd.ctrl) {
                NANI.screen_y = NANI.rowoff + NUM_TERM_COLS - 1;
                if (NANI.screen_y >= NANI.numrows)
                    NANI.screen_y = NANI.numrows;
                int i = NUM_TERM_ROWS;
                while (i--) {
                    NANI_move_cursor('j');
                }
            }
            break;
        case '$':
            if (NANI.screen_y < NANI.numrows) {
                NANI.screen_x = NANI.row[NANI.screen_y].size;
            }
            break;
        case 'h':
        case 'j':
        case 'k':
        case 'l':
            NANI_move_cursor(printable_char);
            break;
        case 'i':
            NANI.mode = NANI_INSERT;
            break;
        case ':':
            NANI.mode = NANI_COMMAND;
            break;
        default:
            break;
    }
}

static void NANI_process_insert_default(char c) {
    if (c & 0x80) return; // ignore releases
    char printable_char;
    if (c >= KEY_A && c <= KEY_Z) {
        printable_char = keycode_to_printable_char[NANI.kbd.shift ^ NANI.kbd.caps][(int)c];
    } else {
        printable_char = keycode_to_printable_char[NANI.kbd.shift][(int)c];
    }
    if (c == '\0') return;
    if (NANI.screen_y == NANI.numrows) {
        NANI_insert_row(NANI.numrows, "", 0);
    }
    erow_insert_char(&NANI.row[NANI.screen_y], NANI.screen_x, printable_char);
    NANI.screen_x++;
}

static void NANI_process_insert_key(char c) {
    switch (c) {
        case KEY_ESC:
            NANI.mode = NANI_NORMAL;
            break;
        case KEY_BACKSPACE:
            NANI_delete_char();
            break;
        case KEY_ENTER:
            NANI_insert_newline();
            break;
        default:
            NANI_process_insert_default(c);
    }
}

static void NANI_process_command_key(char c) {
    switch (c) {
        case KEY_ESC:
            NANI.mode = NANI_NORMAL;
            break;
    }
}


static void NANI_process_key() {
    unsigned char c;

    int nbytes_read = ece391_read(0, &c, 1);
    if (nbytes_read == 0) return;

    switch (c) {
        // Process modifier keys
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT:
            NANI.kbd.shift = 1;
            break;
        case KEY_LEFTCTRL:
            NANI.kbd.ctrl = 1;
            break;
        case KEY_LEFTALT:
            NANI.kbd.alt = 1;
            break;
        case KEY_LEFTSHIFT | 0x80:
        case KEY_RIGHTSHIFT | 0x80:
            NANI.kbd.shift = 0;
            break;
        case KEY_LEFTCTRL | 0x80:
            NANI.kbd.ctrl = 0;
            break;
        case KEY_LEFTALT | 0x80:
            NANI.kbd.alt = 0;
            break;
        case KEY_CAPSLOCK:
            NANI.kbd.caps = !NANI.kbd.caps;
            break;
        // Process other keys
        default:
            switch (NANI.mode) {
                case NANI_NORMAL:
                        NANI_process_normal_key(c);
                    break;
                case NANI_INSERT:
                        NANI_process_insert_key(c);
                    break;
                case NANI_COMMAND:
                        NANI_process_command_key(c);
                    break;
            }
    }
}

static void NANI_init() {
    NANI.screen_x = 0;
    NANI.screen_y = 0;
    NANI.rowoff = 0;
    NANI.coloff = 0;
    NANI.mode = NANI_NORMAL;
    NANI.kbd.shift = 0;
    NANI.kbd.caps = 0;
    NANI.kbd.ctrl = 0;
    NANI.kbd.alt = 0;
    NANI.numrows = 0;
    NANI.row = (erow_t *)NANI_STATIC_BUF_ADDR;
}

static char line_buf[MAX_COLS + 1];

static int32_t NANI_open(int32_t fd) {
    int linelen;
    while (-1 != (linelen = ece391_getline(line_buf, fd, MAX_COLS + 1))) {
        if (linelen == -2) {
            ece391_fdputs(1, (uint8_t*)"file too long\n");
            return -1;
        }
        while (linelen > 0 && line_buf[linelen - 1] == '\n')
        linelen--;
        NANI_insert_row(NANI.numrows, line_buf, linelen);
    }
    return 0;
}

int main()
{
    int32_t fd;
    uint8_t buf[1024];
    NANI_init();
    if (ece391_getargs (buf, 1024) != -1) {
        if (-1 == (fd = ece391_open (buf))) {
            ece391_fdputs (1, (uint8_t*)"file not found\n");
            return 2;
        }
        if (-1 == NANI_open(fd)) {
            return 1;
        }
    }
    ece391_ioctl(1, 1); // Enable raw mode
    while (1) {
        NANI_update_screen();
        NANI_process_key();
    }
    ece391_ioctl(1, 0); // Disable raw mode
    return 0;
}


int32_t ece391_memcpy(void* dest, const void* src, int32_t n)
{
    int32_t i;
    char* d = (char*)dest;
    char* s = (char*)src;
    for(i=0; i<n; i++) {
        d[i] = s[i];
    }

    return 0;
}

int32_t ece391_getline(char* buf, int32_t fd, int32_t n) {
    int32_t i;
    for (i = 0; i < n; i++) {
        char c;
        if (1 != ece391_read(fd, &c, 1)) {
            break;
        }
        if (c == '\n') {
            break;
        }
        buf[i] = c;
    }
    if (i == n && buf[i - 1] != '\n')
        i = -2;
    if (i == 0)
        i = -1;
    return i;
}

void ece391_memset(void* memory, char c, int n)
{
    char* mem = (char*)memory;
    int i;
    for(i=0; i<n; i++) {
        mem[i] = c;
    }
}
