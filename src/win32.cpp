internal window_size_t
win32_get_window_size(HWND window) {
    RECT client_rectangle;
    GetClientRect(window, &client_rectangle);
    
    window_size_t result;
    result.width = client_rectangle.right - client_rectangle.left;
    result.height = client_rectangle.bottom - client_rectangle.top;

    return(result);
}

internal void
win32_toggle_window_fullscreen(HWND window, WINDOWPLACEMENT *placement) {
    DWORD window_style = GetWindowLong(window, GWL_STYLE);
    if (window_style & WS_OVERLAPPEDWINDOW) {
        MONITORINFO monitor_info = {};
        monitor_info.cbSize = sizeof(monitor_info);
        if (GetWindowPlacement(window, placement) &&
            GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &monitor_info)) {
            SetWindowLong(window, GWL_STYLE, (window_style & ~(WS_OVERLAPPEDWINDOW)) | WS_POPUP);
            SetWindowPos(window, HWND_TOP,
                         monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
                         monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
                         monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    } else {
        SetWindowLongPtr(window, GWL_STYLE, (window_style & ~(WS_POPUP)) | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(window, placement);
        SetWindowPos(window, 0, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

internal LRESULT CALLBACK
win32_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    LRESULT result = 0;
    switch (message) {
        case WM_CLOSE:
        case WM_DESTROY: {
            PostQuitMessage(0);
        } break;
        default: {
            result = DefWindowProcA(window, message, wparam, lparam);
        }
    }
    return(result);
}

internal window_t
create_window(char *title, s32 width, s32 height) {
    window_t window = {};
    window.platform = VirtualAlloc(0, sizeof(window_win32_t), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    window_win32_t *platform = (window_win32_t *)window.platform;
    platform->window_instance = GetModuleHandleA(0);
    
    DEVMODE monitor_info;
    monitor_info.dmSize = sizeof(DEVMODE);
    EnumDisplaySettings(0, ENUM_CURRENT_SETTINGS, &monitor_info);
    
    s32 monitor_width = monitor_info.dmPelsWidth;
    s32 monitor_height = monitor_info.dmPelsHeight;
    s32 monitor_refresh_rate = monitor_info.dmDisplayFrequency;

    char *window_title = title;
    s32 window_width = width;
    s32 window_height = height;
    s32 window_x = (monitor_width - window_width)/2;
    s32 window_y = (monitor_height - window_height)/2;

    u32 window_style = WS_OVERLAPPEDWINDOW;
    u32 window_style_ex = 0;
    
    RECT window_rectangle = {};
    window_rectangle.left = 0;
    window_rectangle.right = window_width;
    window_rectangle.top = 0;
    window_rectangle.bottom = window_height;
    if (AdjustWindowRect(&window_rectangle, window_style, 0)) {
        window_width = window_rectangle.right - window_rectangle.left;
        window_height = window_rectangle.bottom - window_rectangle.top;
    }
    
    WNDCLASSA window_class = {};
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = win32_window_proc;
    window_class.cbClsExtra = 0;
    window_class.cbWndExtra = 0;
    window_class.hInstance = platform->window_instance;
    window_class.hIcon = LoadIcon(0, IDI_APPLICATION);
    window_class.hCursor = LoadCursor(0, IDC_ARROW);
    window_class.hbrBackground = 0;
    window_class.lpszMenuName = 0;
    window_class.lpszClassName = "chip8_window_class";
    platform->window_atom = RegisterClassA(&window_class);
    if (platform->window_atom) {
        platform->window_handle = CreateWindowExA(window_style_ex, MAKEINTATOM(platform->window_atom), window_title,
                                                  window_style, window_x, window_y, window_width, window_height,
                                                  0, 0, platform->window_instance, 0);
        if (platform->window_handle) {
            ShowWindow(platform->window_handle, SW_SHOW);
        }
    }
    
    return(window);
}

internal void
destroy_window(window_t *window) {
    window_win32_t *platform = (window_win32_t *)window->platform;
    UnregisterClassA(MAKEINTATOM(platform->window_atom), platform->window_instance);
    DestroyWindow(platform->window_handle);
    VirtualFree(platform, 0, MEM_RELEASE);
}

internal void
update_window_events(window_t *window) {
    MSG message;
    while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
        switch (message.message) {
            case WM_QUIT: {
                window->should_quit = true;
            } break;
            default: {
                TranslateMessage(&message);
                DispatchMessageA(&message);
            }
        }
    }    
}

internal void
toggle_window_fullscreen(window_t *window) {
    window_win32_t *platform = (window_win32_t *)window->platform;
    win32_toggle_window_fullscreen(platform->window_handle, &platform->window_placement);
}

internal window_size_t
get_window_size(window_t *window) {
    window_win32_t *platform = (window_win32_t *)window->platform;
    window_size_t result = win32_get_window_size(platform->window_handle);
    return(result);
}

internal input_t
create_input(window_t *window) {
    window_win32_t *platform = (window_win32_t *)window->platform;
    input_t input = {};
    
    RAWINPUTDEVICE raw_input_device = {};
    raw_input_device.usUsagePage = 0x01;
    raw_input_device.usUsage = 0x02;
    raw_input_device.hwndTarget = platform->window_handle;
    // TODO(xkazu0x): find another way to handle error
    if (!RegisterRawInputDevices(&raw_input_device, 1, sizeof(raw_input_device))) {
        print("failed to register mouse as raw input device\n");
    }

    return(input);
}

internal void
input_process_digital_button(digital_button_t *button, b32 down) {
    b32 was_down = button->down;
    button->pressed = !was_down && down;
    button->released = was_down && !down;
    button->down = down;
}

internal void
update_window_events(window_t *window, input_t *input) {
    window_win32_t *platform = (window_win32_t *)window->platform;
    
    for (u32 i = 0; i < KEY_MAX; i++) {
        input->keyboard[i].pressed = false;
        input->keyboard[i].released = false;
    }
    
    input->mouse.left.pressed = false;
    input->mouse.left.released = false;
    input->mouse.right.pressed = false;
    input->mouse.right.released = false;
    input->mouse.middle.pressed = false;
    input->mouse.middle.released = false;
    input->mouse.x1.pressed = false;
    input->mouse.x1.released = false;
    input->mouse.x2.pressed = false;
    input->mouse.x2.released = false;
    input->mouse.dwheel = 0;
    input->mouse.dx = 0;
    input->mouse.dy = 0;
    
    MSG message;
    while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
        switch (message.message) {
            case WM_QUIT: {
                window->should_quit = true;
            } break;
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP: {
                u32 key_code = (u32)message.wParam;
                b32 down = ((message.lParam & (1 << 31)) == 0);
                input_process_digital_button(&input->keyboard[key_code], down);
                TranslateMessage(&message);
                DispatchMessageA(&message);
            } break;
            case WM_INPUT: {
                UINT size;
                GetRawInputData((HRAWINPUT)message.lParam, RID_INPUT, 0, &size, sizeof(RAWINPUTHEADER));
                char *buffer = (char *)_alloca(size);
                if (GetRawInputData((HRAWINPUT)message.lParam, RID_INPUT, buffer, &size, sizeof(RAWINPUTHEADER)) == size) {
                    RAWINPUT *raw_input = (RAWINPUT *)buffer;
                    if (raw_input->header.dwType == RIM_TYPEMOUSE && raw_input->data.mouse.usFlags == MOUSE_MOVE_RELATIVE){
                        input->mouse.dx += raw_input->data.mouse.lLastX;
                        input->mouse.dy += raw_input->data.mouse.lLastY;

                        USHORT button_flags = raw_input->data.mouse.usButtonFlags;
                        b32 left_button_down = input->mouse.left.down;
                        if (button_flags & RI_MOUSE_LEFT_BUTTON_DOWN) left_button_down = true;
                        if (button_flags & RI_MOUSE_LEFT_BUTTON_UP) left_button_down = false;
                        input_process_digital_button(&input->mouse.left, left_button_down);

                        b32 right_button_down = input->mouse.right.down;
                        if (button_flags & RI_MOUSE_RIGHT_BUTTON_DOWN) right_button_down = true;
                        if (button_flags & RI_MOUSE_RIGHT_BUTTON_UP) right_button_down = false;
                        input_process_digital_button(&input->mouse.right, right_button_down);
                            
                        b32 middle_button_down = input->mouse.middle.down;
                        if (button_flags & RI_MOUSE_MIDDLE_BUTTON_DOWN) middle_button_down = true;
                        if (button_flags & RI_MOUSE_MIDDLE_BUTTON_UP) middle_button_down = false;
                        input_process_digital_button(&input->mouse.middle, middle_button_down);

                        b32 x1_button_down = input->mouse.x1.down;
                        if (button_flags & RI_MOUSE_BUTTON_4_DOWN) x1_button_down = true;
                        if (button_flags & RI_MOUSE_BUTTON_4_UP) x1_button_down = false;
                        input_process_digital_button(&input->mouse.x1, x1_button_down);
                            
                        b32 x2_button_down = input->mouse.x2.down;
                        if (button_flags & RI_MOUSE_BUTTON_5_DOWN) x2_button_down = true;
                        if (button_flags & RI_MOUSE_BUTTON_5_UP) x2_button_down = false;
                        input_process_digital_button(&input->mouse.x2, x2_button_down);
                            
                        if (raw_input->data.mouse.usButtonFlags & RI_MOUSE_WHEEL) {
                            input->mouse.dwheel += ((SHORT)raw_input->data.mouse.usButtonData) / WHEEL_DELTA;
                        }
                    }
                }
                TranslateMessage(&message);
                DispatchMessageA(&message);
            } break;
            default: {
                TranslateMessage(&message);
                DispatchMessageA(&message);
            }
        }
    }
    
    // BYTE keyboard_state[KEY_MAX];
    // if (GetKeyboardState(keyboard_state)) {
    //     for (s32 key_code = 0; key_code < KEY_MAX; key_code++) {
    //         input_process_digital_button(input->keyboard + key_code, keyboard_state[key_code] >> 7);
    //     }
    // }
        
    input->mouse.wheel += input->mouse.dwheel;
    
    POINT mouse_position;
    GetCursorPos(&mouse_position);
    ScreenToClient(platform->window_handle, &mouse_position);
    
    if (mouse_position.x < 0) mouse_position.x = 0;
    if (mouse_position.y < 0) mouse_position.y = 0;

    window_size_t window_size = win32_get_window_size(platform->window_handle);
    if (mouse_position.x > window_size.width) mouse_position.x = window_size.width;
    if (mouse_position.y > window_size.height) mouse_position.y = window_size.height;

    input->mouse.x = mouse_position.x;
    input->mouse.y = mouse_position.y;
}

internal renderer_t
create_renderer(window_t *window, s32 width, s32 height) {
    window_win32_t *platform = (window_win32_t *)window->platform;
    
    renderer_t renderer = {};
    renderer.width = width;
    renderer.height = height;
    renderer.bytes_per_pixel = 4;
    renderer.pitch = renderer.width*renderer.bytes_per_pixel;
    
    platform->bitmap_info.bmiHeader.biSize = sizeof(platform->bitmap_info.bmiHeader);
    platform->bitmap_info.bmiHeader.biWidth = renderer.width;
    platform->bitmap_info.bmiHeader.biHeight = -renderer.height;
    platform->bitmap_info.bmiHeader.biPlanes = 1;
    platform->bitmap_info.bmiHeader.biBitCount = 32;
    platform->bitmap_info.bmiHeader.biCompression = BI_RGB;

    s32 renderer_memory_size = (renderer.width*renderer.height)*renderer.bytes_per_pixel;
    renderer.memory = VirtualAlloc(0, renderer_memory_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    return(renderer);
}

internal void
renderer_clear(renderer_t *renderer, vec3 color) {
    u32 out_color = ((round_f32_to_u32(color.r * 255.0f) << 16) |
                     (round_f32_to_u32(color.g * 255.0f) << 8) |
                     (round_f32_to_u32(color.b * 255.0f) << 0));
    
    u8 *row = (u8 *)renderer->memory;
    for (s32 y = 0; y < renderer->height; ++y) {
        u32 *pixel = (u32 *)row;
        for (s32 x = 0; x < renderer->width; ++x) {
            *pixel++ = out_color;
        }
        row += renderer->pitch;
    }
}

internal void
renderer_present(renderer_t *renderer, window_t *window) {
    window_win32_t *platform = (window_win32_t *)window->platform;
    
    window_size_t window_size = win32_get_window_size(platform->window_handle);
    HDC window_device = GetDC(platform->window_handle);
    
    StretchDIBits(window_device,
                  0, 0, window_size.width, window_size.height,
                  0, 0, renderer->width, renderer->height,
                  renderer->memory,
                  &platform->bitmap_info,
                  DIB_RGB_COLORS, SRCCOPY);
    
    ReleaseDC(platform->window_handle, window_device);
}

internal void
renderer_draw_pixel(renderer_t *renderer, f32 x, f32 y, vec3 color) {
    s32 offset_x = round_f32_to_s32(x);
    s32 offset_y = round_f32_to_s32(y);

    if (offset_x < 0) offset_x = 0;
    if (offset_y < 0) offset_y = 0;
    
    if (offset_x > renderer->width) offset_x = renderer->width;
    if (offset_y > renderer->width) offset_y = renderer->height;

    u32 out_color = ((round_f32_to_u32(color.r * 255.0f) << 16) |
                     (round_f32_to_u32(color.g * 255.0f) << 8) |
                     (round_f32_to_u32(color.b * 255.0f) << 0));

    u8 *row = ((u8 *)renderer->memory +
               (offset_x * renderer->bytes_per_pixel) +
               (offset_y * renderer->pitch));
    u32 *pixel = (u32 *)row;
    *pixel = out_color;
}

internal void
renderer_draw_pixel(renderer_t *renderer, vec2 position, vec3 color) {
    renderer_draw_pixel(renderer, position.x, position.y, color);
}

internal s64
get_performance_frequency() {
    LARGE_INTEGER large_integer;
    QueryPerformanceFrequency(&large_integer);
    return(large_integer.QuadPart);
}

internal s64
get_performance_counter() {
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return(result.QuadPart);
}

internal void
sleep(u32 ms) {
    Sleep(ms);
}
