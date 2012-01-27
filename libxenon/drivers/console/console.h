#ifndef __console_console_h
#define __console_console_h

#ifdef __cplusplus
extern "C" {
#endif

unsigned int old_console_color[2];
    
#define CONSOLE_COLOR_RED 0x0000FF00
#define CONSOLE_COLOR_BLUE 0xD8444E00
#define CONSOLE_COLOR_GREEN 0x00800000
#define CONSOLE_COLOR_BLACK 0x00000000
#define CONSOLE_COLOR_WHITE 0xFFFFFF00
#define CONSOLE_COLOR_GREY 0xC0C0C000
#define CONSOLE_COLOR_BROWN 0x00339900
#define CONSOLE_COLOR_PURPLE 0xFF009900
#define CONSOLE_COLOR_YELLOW 0x00FFFF00
#define CONSOLE_COLOR_ORANGE 0x0066FF00
#define CONSOLE_COLOR_PINK 0xFF66FF00

#define CONSOLE_WARN CONSOLE_COLOR_YELLOW
#define CONSOLE_ERR CONSOLE_COLOR_ORANGE

#define PRINT_COL(bg, fg, s, ...) \
        old_console_color[0] = console_get_color(0); old_console_color[1] = console_get_color(1); \
        console_set_colors(bg,fg); \
        printf(s, ##__VA_ARGS__); \
        console_set_colors(old_console_color[0],old_console_color[1]); 

#define PRINT_WARN(s, ...) PRINT_COL(-1,CONSOLE_WARN,s, ##__VA_ARGS__)
#define PRINT_ERR(s, ...) PRINT_COL(-1,CONSOLE_ERR,s, ##__VA_ARGS__)

unsigned int console_get_color(int num);
void console_set_colors(unsigned int background, unsigned int foreground); // can be called before init
void console_get_dimensions(unsigned int * width,unsigned int * height);
void console_putch(const char c);
void console_clrscr();
void console_init(void);
void console_close(void);

#ifdef __cplusplus
};
#endif

#endif
