internal void
os_process_digital_button(digital_button_t *button, b32 down) {
    b32 was_down = button->down;
    button->pressed = !was_down && down;
    button->released = was_down && !down;
    button->down = down;
}

#if defined(OS_WINDOWS)
#include "win32.h"
#include "win32.cpp"
#else
#error no implementation for the current operating system
#endif
