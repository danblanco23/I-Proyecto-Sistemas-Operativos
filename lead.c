//#include "config.h"

typedef unsigned char      u8;
typedef signed   char      s8;
typedef unsigned short     u16;
typedef signed   short     s16;
typedef unsigned int       u32;
typedef signed   int       s32;
typedef unsigned long long u64;
typedef signed   long long s64;

typedef enum bool {
    false,
    true
} bool;

/* Port I/O */

static inline u8 inb(u16 p)
{
    u8 r;
    asm("inb %1, %0" : "=a" (r) : "dN" (p));
    return r;
}

static inline void outb(u16 p, u8 d)
{
    asm("outb %1, %0" : : "dN" (p), "a" (d));
}

/* Timing */

/* Return the number of CPU ticks since boot. */
static inline u64 rdtsc(void)
{
    u32 hi, lo;
    asm("rdtsc" : "=a" (lo), "=d" (hi));
    return ((u64) lo) | (((u64) hi) << 32);
}

/* Return the current second field of the real-time-clock (RTC). Note that the
 * value may or may not be represented in such a way that it should be
 * formatted in hex to display the current second (i.e. 0x30 for the 30th
 * second). */
u8 rtcs(void)
{
    u8 last = 0, sec;
    do { /* until value is the same twice in a row */
        /* wait for update not in progress */
        do { outb(0x70, 0x0A); } while (inb(0x71) & 0x80);
        outb(0x70, 0x00);
        sec = inb(0x71);
    } while (sec != last && (last = sec));
    return sec;
}

/* The number of CPU ticks per millisecond */
u64 tpms;

/* Set tpms to the number of CPU ticks per millisecond based on the number of
 * ticks in the last second, if the RTC second has changed since the last call.
 * This gets called on every iteration of the main loop in order to provide
 * accurate timing. */
void tps(void)
{
    static u64 ti = 0;
    static u8 last_sec = 0xFF;
    u8 sec = rtcs();
    if (sec != last_sec) {
        last_sec = sec;
        u64 tf = rdtsc();
        tpms = (u32) ((tf - ti) >> 3) / 125; /* Less chance of truncation */
        ti = tf;
    }
}

/* Video Output */

/* Seven possible display colors. Bright variations can be used by bitwise OR
 * with BRIGHT (i.e. BRIGHT | BLUE). */
enum color {
    BLACK,
    BLUE,
    GREEN,
    CYAN,
    RED,
    MAGENTA,
    YELLOW,
    GRAY,
    BRIGHT
};

#define COLS (80)
#define ROWS (25)
u16 *const video = (u16*) 0xB8000;

/* Display a character at x, y in fg foreground color and bg background color.
 */
void putc(u8 x, u8 y, enum color fg, enum color bg, char c)
{
    u16 z = (bg << 12) | (fg << 8) | c;
    video[y * COLS + x] = z;
}

/* Display a string starting at x, y in fg foreground color and bg background
 * color. Characters in the string are not interpreted (e.g \n, \b, \t, etc.).
 * */
void puts(u8 x, u8 y, enum color fg, enum color bg, const char *s)
{
    for (; *s; s++, x++)
        putc(x, y, fg, bg, *s);
}

/* Clear the screen to bg backround color. */
void clear(enum color bg)
{
    u8 x, y;
    for (y = 0; y < ROWS; y++)
        for (x = 0; x < COLS; x++)
            putc(x, y, bg, bg, ' ');
}

/* Keyboard Input */

#define KEY_D     (0x20)
#define KEY_H     (0x23)
#define KEY_P     (0x19)
#define KEY_R     (0x13)
#define KEY_S     (0x1F)
#define KEY_UP    (0x48)
#define KEY_DOWN  (0x50)
#define KEY_LEFT  (0x4B)
#define KEY_RIGHT (0x4D)
#define KEY_ENTER (0x1C)
#define KEY_SPACE (0x39)

/* Return the scancode of the current up or down key if it has changed since
 * the last call, otherwise returns 0. When called on every iteration of the
 * main loop, returns non-zero on a key event. */
u8 scan(void)
{
    static u8 key = 0;
    u8 scan = inb(0x60);
    if (scan != key)
        return key = scan;
    else return 0;
}


struct {
    s8 x, y; /* Coordinates */
} current;

#define TITLE_X (COLS / 2 - 9)
#define TITLE_Y (ROWS / 5)

/* Draw about information in the centre. */
void start_menu(){
    clear(BLACK);
    puts(TITLE_X,      TITLE_Y,     BRIGHT | RED,     RED,     "---");
    puts(TITLE_X + 3,  TITLE_Y,     BRIGHT,           MAGENTA, "---");
    puts(TITLE_X + 6,  TITLE_Y,     BRIGHT,           BLUE,    "---");
    puts(TITLE_X + 9,  TITLE_Y,     BRIGHT,           GREEN,   "---");
    puts(TITLE_X + 9,  TITLE_Y,     BRIGHT,           GREEN,   "---");
    puts(TITLE_X,      TITLE_Y + 1, BRIGHT | RED,     RED,     " L ");
    puts(TITLE_X + 3,  TITLE_Y + 1, BRIGHT | MAGENTA, MAGENTA, " E ");
    puts(TITLE_X + 6,  TITLE_Y + 1, BRIGHT | BLUE,    BLUE,    " A ");
    puts(TITLE_X + 9,  TITLE_Y + 1, BRIGHT | GREEN,   GREEN,   " D ");
    puts(TITLE_X,      TITLE_Y + 2, BRIGHT,           RED,     "---");
    puts(TITLE_X + 3,  TITLE_Y + 2, BRIGHT,           MAGENTA, "---");
    puts(TITLE_X + 6,  TITLE_Y + 2, BRIGHT,           BLUE,    "---");
    puts(TITLE_X + 9,  TITLE_Y + 2, BRIGHT,           GREEN,   "---");

    puts(TITLE_X-10, TITLE_Y + 5, BRIGHT, BLACK, "Principios De Sistemas Operativos");
    puts(TITLE_X-10, TITLE_Y + 6, BRIGHT, BLACK, "Profesor: Ernesto Rivera Alvarado");
    puts(TITLE_X-5, TITLE_Y + 7, BRIGHT, BLACK, "Integantes");
    puts(TITLE_X, TITLE_Y + 8, BRIGHT, BLACK, "Daniel Blanco Vargas");
    puts(TITLE_X, TITLE_Y + 9, BRIGHT, BLACK, "Carlos");
    puts(TITLE_X, TITLE_Y + 10, BRIGHT, BLACK, "Gerardo");

}

#define WELL_WIDTH  (17)
#define WELL_HEIGHT (13)

#define WELL_Y (COLS / 2 - 10)
#define WELL_X (ROWS / 5)
/* Draw the well, current tetrimino, its ghost, the preview tetrimino, the
 * status, score and level indicators. Each well/tetrimino cell is drawn one
 * screen-row high and two screen-columns wide. The top two rows of the well
 * are hidden. Rows in the cleared_rows array are drawn as white rather than
 * their actual colors. */
void draw_first_stage(void) {
    u8 x, y;

    /* Border */
    for (x = 0; x < WELL_HEIGHT; x++) {
        putc(WELL_Y, x + WELL_X , BLACK, RED, ' ');
        putc(WELL_Y + 18, x + WELL_X, BLACK, GREEN, ' ');
        x++;
    }
    /*
    for (y = 0; y < WELL_WIDTH; y++) {
        putc(y + WELL_Y, WELL_HEIGHT, BLACK, GRAY, ' ');
        y++;
    }*/
}


void main(void){
 start_menu();
 clear(BLACK);
 puts(TITLE_X, TITLE_Y + 10, BRIGHT, BLACK, "El otro mae");

 while (true){
    //tps();
    u8 key;
    bool flag = false;
    if ((key = scan())) {
       if(key == KEY_SPACE){
             flag = true;
         }
    }

    if(flag){
        clear(BLACK);
        draw_first_stage();
    }
    //goto loop;
    }
}