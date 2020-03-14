#include "config.h"

typedef unsigned char      u8;
typedef signed   char      s8;
typedef unsigned short     u16;
typedef signed   short     s16;
typedef unsigned int       u32;
typedef signed   int       s32;
typedef unsigned long long u64;
typedef signed   long long s64;

#define noreturn __attribute__((noreturn)) void

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
    asm("rdtsc" : "=q" (lo), "=d" (hi));
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

/* IDs used to keep separate timing operations separate */
enum timer {
    TIMER_UPDATE,
    TIMER_CLEAR,
    TIMER__LENGTH
};

u64 timers[TIMER__LENGTH] = {0};

/* Return true if at least ms milliseconds have elapsed since the last call
 * that returned true for this timer. When called on each iteration of the main
 * loop, has the effect of returning true once every ms milliseconds. */
bool interval(enum timer timer, u32 ms)
{
    u64 tf = rdtsc();
    if (tf - timers[timer] >= tpms * ms) {
        timers[timer] = tf;
        return true;
    } else return false;
}

/* Return true if at least ms milliseconds have elapsed since the first call
 * for this timer and reset the timer. */
bool wait(enum timer timer, u32 ms)
{
    if (timers[timer]) {
        if (rdtsc() - timers[timer] >= tpms * ms) {
            timers[timer] = 0;
            return true;
        } else return false;
    } else {
        timers[timer] = rdtsc();
        return false;
    }
}

/* Random */

/* Generate a random number from 0 inclusive to range exclusive from the number
 * of CPU ticks since boot. */
u32 rand(u32 range)
{
    return (u32) rdtsc() % range;
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

/* Formatting */

/* Format n in radix r (2-16) as a w length string. */
char *itoa(u32 n, u8 r, u8 w)
{
    static const char d[16] = "0123456789ABCDEF";
    static char s[34];
    s[33] = 0;
    u8 i = 33;
    do {
        i--;
        s[i] = d[n % r];
        n /= r;
    } while (i > 33 - w);
    return (char *) (s + i);
}

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

    puts(TITLE_X - 5,  TITLE_Y + 4, RED,           BLACK,   "PRESS SPACE TO START");
    puts(TITLE_X-10, TITLE_Y + 6, BRIGHT, BLACK, "Principios De Sistemas Operativos");
    puts(TITLE_X-10, TITLE_Y + 7, BRIGHT, BLACK, "Profesor: Ernesto Rivera Alvarado");
    puts(TITLE_X-5, TITLE_Y + 8, BRIGHT, BLACK, "Integantes");
    puts(TITLE_X, TITLE_Y + 9, BRIGHT, BLACK, "Daniel Blanco Vargas");
    puts(TITLE_X, TITLE_Y + 10, BRIGHT, BLACK, "Carlos");
    puts(TITLE_X, TITLE_Y + 11, BRIGHT, BLACK, "Gerardo");

    bool flag = true;
    while(flag){
        u8 key;
        if ((key = scan())) {
            if(key == KEY_SPACE){
                flag = false;
            }
        }
    }

}

u8 space[WELL_HEIGHT][WELL_WIDTH];

u32 score = 0, level = 1, speed = 20;

bool paused = false, game_over = false;

struct {
    s8 x, y;
} spaceship;

struct shot{
    s8 x, y;
    bool available;
};

struct enemy{
    s8 x, y;
};

struct enemy enemy1, enemy2, enemy3, enemy4, enemy5, enemy6;

bool shooting = false;
u64 counter = 100000;

#define WELL_Y (COLS / 2 - 10) //Numero de columna donde inica la primer pared
#define WELL_X (ROWS - ROWS + 2) //Numero de fila donde inicia la pantalla

/* Verifica si la nave se va a pasar de la pared
 * true si se pasa
 * false si no*/
bool collide(s8 x, s8 y)
{

    if (y >= WELL_Y + WELL_WIDTH -1 || y <= WELL_Y-1){
        return true;
    }
    return false;
}

/* Clear the screen to bg backround color. */
void clear_space(enum color bg)
{
    u8 x, y;
    for (y = WELL_X+1; y < WELL_WIDTH; y++)
        for (x = WELL_Y+1; x < WELL_HEIGHT; x++)
            putc(x, y, bg, bg, ' ');
}


/*Posicion inicial de la nave*/
void spawn(void)
{
    spaceship.x = WELL_HEIGHT + 1;
    spaceship.y = WELL_Y + (WELL_WIDTH / 2);
}

/*actualiza las cordenadas de la nave cuando se va a mover
 * retorna false si choca con una pared
 * true si no*/
bool move(s8 dy, s8 dx)
{
    /*if (game_over)
        return false;*/

    if (collide(spaceship.x + dx, spaceship.y + dy)){
        return false;}
    spaceship.x += dx;
    spaceship.y += dy;
    return true;
}

void update_spaceship(){
    puts(spaceship.y, spaceship.x, GREEN, BLACK, "<^>");
}

struct shot shots[2] = {
        {0, 0, false},
        {0, 0, false},
        {0, 0, false}
};

struct shot current_shot;

/* shot */
/*Posicion inicial del disparo*/
void spawn_shot(){

    for(u32 i = 0; i < 2; i++){
        if (shots[i].available == false){
            current_shot = shots[i];
            shots[i].available = true;
            puts(0, 10, BLUE, BLACK, "<^>");
            //current_shot.available = true;
            break;
        }

    }

    current_shot.x = spaceship.x;
    current_shot.y = spaceship.y + 1;
}

/*verifica si el disparo llego al inicio de la pantalla
 * true si lo hace
 * false si no*/
bool collide_shot(s8 x, s8 y){
    if (current_shot.x < WELL_X){
        return true;
    }
    return false;
}

bool move_shot(s8 dx){
    /*if (game_over)
        return false;*/

    if (collide_shot(current_shot.x + dx, current_shot.y)){
        return false;
    }
    return true;
}


void update_shot(struct enemy *enemy){

    /*if(shooting){
         putc(shot1.y,shot1.x,BLACK,BLACK,' ');
         if(move_shot1(0, -1)){
             putc(shot1.y, shot2.x, BRIGHT, RED, ' ');
         }
     }
     if(move_shot1(0, -1) == false){
         shooting = false;
         spawn_shot1();
     }*/
    if(current_shot.available == false && move_shot(current_shot.x - 1)){
        if (current_shot.x == enemy->x && (current_shot.y == enemy->y || current_shot.y == enemy->y+1 || current_shot.y == enemy->y+2 || current_shot.y == enemy->y+3)){
            puts(enemy->y, enemy->x, BLACK, BLACK, "    ");
            putc(current_shot.y, current_shot.x, BLACK, BLACK, ' ');
            //spawn_shot();
            score += KILL_SCORE;
            if (score >= SCORE_PER_LEVEL) {
                level++;
                score -= SCORE_PER_LEVEL;
                main();
/*
                double speed_s = pow(0.8 - (level - 1) * 0.007, level - 1);
                speed = speed_s * 1000;*/
            }
            current_shot.x = 0;
            current_shot.y = 0;
            spawn_enemy(enemy);
            counter = 100000;
        }
        else{
            puts(0, 0, CYAN, BLACK, "<^>");
            putc(current_shot.y, current_shot.x, BLACK, BLACK, ' ');
            current_shot.x = current_shot.x - 1;
            putc(current_shot.y, current_shot.x, BLACK, RED, ' ');
        }
    }
    else{
        //puts(enemy1.y, enemy1.x, BLACK, BLACK, "    ");
        //spawn_enemy();
        //counter = 100000;
        putc(current_shot.y, current_shot.x, BLACK, BLACK, ' ');
    }
    /*
    if(current_shot.available == false) {
        puts(0, 0, CYAN, BLACK, "<^>");
        putc(current_shot.y, current_shot.x, BLACK, BLACK, ' ');
        current_shot.x = current_shot.x - 1;
        putc(current_shot.y, current_shot.x, BLACK, RED, ' ');
    }
    */
    /*
    if(current_shot.x < WELL_X){
        //putc(25,5,BLACK,GREEN,shotActive);
        //shooting=false;
        //spawn_shot();
        putc(current_shot.y, current_shot.x, BLACK, BLACK, ' ');
        //current_shot.available = true;
        //clear(BLACK);
    }*/
    //spawn()
}


/*
void update(void)
{
    //update_spaceship();
    update_shot();
    /*double speed_s = pow(0.8 - (level - 1) * 0.007, level - 1);
    speed = speed_s * 1000;

}
*/


/* shot 2*/
/*Posicion inicial del disparo*/
/*void spawn_shot2(){
    puts(0, 10, MAGENTA, BLACK, "<^>");
    shot1.x = spaceship.x ;
    shot1.y = spaceship.y + 1;
}

/*verifica si el disparo llego al inicio de la pantalla
 * true si lo hace
 * false si no*/
/*bool collide_shot2()
{
    if (shot2.x <= WELL_X){
        return true;
    }
    return false;
}

bool move_shot2(s8 dy, s8 dx)
{
    /*if (game_over)
        return false;*/

    /*if (collide_shot2(shot.x + dx, shot.y + dy)){
        return false;}*/
  /*  shot2.x += dx;
    shot2.y += dy;
    return true;
}


void update_shot2()
{
    if(shooting){
        putc(shot2.y,shot2.x,BLACK,BLACK,' ');
        shot2.x = shot2.x-1;
        putc(shot2.y,shot2.x,BLACK,RED,' ');
    }
    if(shot2.x < WELL_X){
        //putc(25,5,BLACK,GREEN,shotActive);
        shooting=false;
        spawn_shot2();
        clear(BLACK);
    }
}*/

/*Se genera un random que va a ser la columna donde va aparecer el enemigo*/
void spawn_enemy(struct enemy *enemy){
    enemy->x = 1;
    u32 random = 0;
    random = rand(26);
    random = random + 31;
    enemy->y = random;
}

/*verifica si el enemigo llega al final(a la parte inferior) de la pantalla*/
bool collide_enemy(s8 x){
    if(x > WELL_HEIGHT){
        return true;
    }
    return false;
}

/*Dibuja el enemigo en pantalla*/
void move_enemy(struct enemy *enemy){
    if(collide_enemy(enemy->x)){
        game_over = true;
    } else{
        puts(enemy->y,enemy->x,BLACK,BLACK,"    ");
        enemy->x = enemy->x + 1;
        puts(enemy->y,enemy->x,BLUE,BLACK,"{**}");
    }
}

/*dibuja un disparo, recibe un struct correspondiente al disparo a dibujar*/
void draw_shot(struct shot shotD){

    putc(shotD.y, shotD.x, BRIGHT, RED, ' ');
}

/*Pinta los bordes de negro*/
void clear_border(){
    for (u8 x = 0; x < WELL_HEIGHT; x++) {
        putc(WELL_Y, x + WELL_X , BLACK, BLACK, ' ');
        putc(WELL_Y + WELL_WIDTH, x + WELL_X, BLACK, BLACK, ' ');
        x++;
    }

    for (u8 x = 1; x < WELL_HEIGHT; x++) {
        putc(WELL_Y, x + WELL_X , BLACK, BLACK, ' ');
        putc(WELL_Y + WELL_WIDTH, x + WELL_X, BLACK, BLACK, ' ');
        x++;
    }
}
#define STATUS_X (COLS * 3/4)
#define STATUS_Y (ROWS / 2 - 4)

#define SCORE_X
#define SCORE_Y (ROWS / 2 - 1)
/* Dibuja las parades del primer nivel,
 * la nave y las balas
 * Usa WELL_Y que vale 30 y WELL_X que vale 5
 * */
void draw_first_stage(){
    u8 x, y;

    /* Border */
    for (x = 0; x < WELL_HEIGHT; x++) {
        putc(WELL_Y, x + WELL_X , BLACK, RED, ' '); //primer pared, empiza en la culumna #30
        putc(WELL_Y + WELL_WIDTH, x + WELL_X, BLACK, RED, ' '); //segunda pared, empieza en la columna #48
        x++;
    }

    for (x = 1; x < WELL_HEIGHT; x++) {
        putc(WELL_Y, x + WELL_X , BLACK, MAGENTA, ' '); //primer pared, empiza en la culumna #30
        putc(WELL_Y + WELL_WIDTH, x + WELL_X, BLACK, MAGENTA, ' '); //segunda pared, empieza en la columna #48
        x++;
    }

    /* Score */
    puts(44, 23, BLUE, BLACK, "SCORE");
    puts(41, 24, BRIGHT | BLUE, BLACK, itoa(score, 10, 10));

    /* Spaceship */
    //update_spaceship();
    //puts(spaceship.y, spaceship.x, GREEN, BLACK, "<^>");

    /*shot*/
    //draw_shot(shotd);
}


noreturn first_stage() {
    {
        clear(BLACK);
        start_menu();

        /* Wait a full second to calibrate timing. */
        u32 itpms;
        tps();
        itpms = tpms; while (tpms == itpms) tps();
        itpms = tpms; while (tpms == itpms) tps();

        /* Initialize game state. Shuffle bag of tetriminos until first tetrimino
         * is not S or Z. */
        //do { shuffle(bag, BAG_SIZE); } while (bag[0] == 4 || bag[0] == 6);
        //spawn();
        //ghost();
        spawn();
        //spawn_shot();
        //spawn_shot2();
        spawn_enemy(&enemy1);
        spawn_enemy(&enemy2);
        spawn_enemy(&enemy3);
        spawn_enemy(&enemy4);
        spawn_enemy(&enemy5);
        spawn_enemy(&enemy6);

        clear(BLACK);
        draw_first_stage();

        bool debug = false, help = true, statistics = false;
        s8 shoot = false;
        s8 shot_b = 0;

        loop:
        /*if(counter == 100000){
            spawn_enemy();
        }*/

        if(counter > 0){
            counter--;
        }
        else{
            move_enemy(&enemy1);
            move_enemy(&enemy2);
            move_enemy(&enemy3);
            move_enemy(&enemy4);
            move_enemy(&enemy5);
            move_enemy(&enemy6);

            counter = 100000;
        }

        tps();

        bool updated = false;

        u8 key;
        if ((key = scan())) {
            switch(key) {
                case KEY_LEFT:
                    move(-1, 0);
                    putc(spaceship.y + 3, spaceship.x, BLACK, BLACK, ' ');
                    break;
                case KEY_RIGHT:
                    move(1, 0);
                    putc(spaceship.y - 1, spaceship.x, BLACK, BLACK, ' ');
                    break;
                case KEY_SPACE:
                    spawn_shot();
                    shooting=true;
                    break;
                case KEY_P:
                    if (game_over)
                        break;
                    clear(BLACK);
                    paused = !paused;
                    break;
            }
            updated = true;
        }

        if (!paused && !game_over && interval(TIMER_UPDATE, speed)) {
            update_shot(&enemy1);
            update_shot(&enemy2);
            update_shot(&enemy3);
            update_shot(&enemy4);
            update_shot(&enemy5);
            update_shot(&enemy6);
            updated = true;
        }
        if(game_over){
            main();
        }

        if (updated){
            update_spaceship();
            //updated = false;
        }

        draw_first_stage();
        clear_border();

        goto loop;
    }
}


noreturn main(){
    first_stage();
}