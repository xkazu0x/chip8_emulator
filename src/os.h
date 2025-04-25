#ifndef OS_H
#define OS_H

enum keycode_t {
    KEY_ESCAPE = 0x1B,

    KEY_SPACE  = 0x20,
    
    KEY_LEFT   = 0x25,
    KEY_UP     = 0x26,
    KEY_RIGHT  = 0x27,
    KEY_DOWN   = 0x28,
    
    KEY_F1     = 0x70,
    KEY_F2     = 0x71,
    KEY_F3     = 0x72,
    KEY_F4     = 0x73,
    KEY_F5     = 0x74,
    KEY_F6     = 0x75,
    KEY_F7     = 0x76,
    KEY_F8     = 0x77,
    KEY_F9     = 0x78,
    KEY_F10    = 0x79,
    KEY_F11    = 0x7A,
    KEY_F12    = 0x7B,
    KEY_F13    = 0x7C,
    KEY_F14    = 0x7D,
    KEY_F15    = 0x7E,
    KEY_F16    = 0x7F,
    KEY_F17    = 0x80,
    KEY_F18    = 0x81,
    KEY_F19    = 0x82,
    KEY_F20    = 0x83,
    KEY_F21    = 0x84,
    KEY_F22    = 0x85,
    KEY_F23    = 0x86,
    KEY_F24    = 0x87,
    
    KEY_MAX = 256,
};

struct digital_button_t {
    b32 down;
    b32 pressed;
    b32 released;
};

struct mouse_t {
    digital_button_t left;
    digital_button_t right;
    digital_button_t middle;
    digital_button_t x1;
    digital_button_t x2;
    s32 wheel;
    s32 dwheel;
    s32 x;
    s32 y;
    s32 dx;
    s32 dy;
};

struct input_t {
    mouse_t mouse;
    digital_button_t keyboard[KEY_MAX];
};

struct os_window_t {
    b32 quit;
    input_t input;
    void *platform;
};

struct os_window_size_t {
    s32 x;
    s32 y;
};

struct os_renderer_t {
    s32 width;
    s32 height;
    s32 bytes_per_pixel;
    s32 pitch;
    void *memory;
};

internal os_window_t os_window_create(char *title, s32 width, s32 height);
internal void os_window_destroy(os_window_t *window);
internal void os_window_update(os_window_t *window);
internal os_window_size_t os_window_get_size(os_window_t *window);
internal void os_window_toggle_fullscreen(os_window_t *window);

internal os_renderer_t os_renderer_create(os_window_t *window, s32 width, s32 height);
internal void os_renderer_draw_pixel(os_renderer_t *renderer, f32 x, f32 y, vec3 color);
internal void os_renderer_clear(os_renderer_t *renderer, vec3 color);
internal void os_renderer_present(os_renderer_t *renderer, os_window_t *window);

internal s64 os_get_ticks_per_frame();
internal s64 os_get_time();
internal void os_sleep_ms(u32 ms);

internal void os_process_digital_button(digital_button_t *button, b32 down);

#endif // OS_H
