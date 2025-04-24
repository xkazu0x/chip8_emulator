/////////////////////////////////
// NOTE(xkazu0x): win32 functions

internal vec2i
win32_get_window_size(HWND window) {
    RECT client_rectangle;
    GetClientRect(window, &client_rectangle);
    vec2i result = _vec2i(client_rectangle.right - client_rectangle.left,
                                client_rectangle.bottom - client_rectangle.top);
    return(result);
}

internal void
win32_window_toggle_fullscreen(HWND window, WINDOWPLACEMENT *placement) {
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

/////////////////////////////////////////
// NOTE(xkazu0x): system window functions

internal b32
os_window_create(os_window_t *window, os_window_info_t info) {
    window->platform = VirtualAlloc(0, sizeof(win32_t), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    win32_t *platform = (win32_t *)window->platform;
    platform->window_instance = GetModuleHandleA(0);
    
    DEVMODE monitor_info;
    monitor_info.dmSize = sizeof(DEVMODE);
    EnumDisplaySettings(0, ENUM_CURRENT_SETTINGS, &monitor_info);
    
    vec2i monitor_size = _vec2i(monitor_info.dmPelsWidth, monitor_info.dmPelsHeight);
    s32 monitor_refresh_rate = monitor_info.dmDisplayFrequency;
    EXINFO("monitor size: %dx%d", monitor_size.x, monitor_size.y);
    EXINFO("monitor refresh rate: %dHz", monitor_refresh_rate);

    char *window_title = info.title;
    vec2i window_size = info.size;
    vec2i window_position = (monitor_size - window_size) / 2;
    EXINFO("window size: %dx%d", window_size.x, window_size.y);
    EXINFO("window position: x:%d y:%d", window_position.x, window_position.y);

    RECT window_rectangle = {};
    window_rectangle.left = 0;
    window_rectangle.right = window_size.x;
    window_rectangle.top = 0;
    window_rectangle.bottom = window_size.y;
    if (AdjustWindowRect(&window_rectangle, WS_OVERLAPPEDWINDOW, 0)) {
        window_size.x = window_rectangle.right - window_rectangle.left;
        window_size.y = window_rectangle.bottom - window_rectangle.top;
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
    window_class.lpszClassName = "EXCALIBUR_WINDOW";
    platform->window_atom = RegisterClassA(&window_class);
    if (!platform->window_atom) {
        EXFATAL("failed to register win32 window");
        return(EX_FALSE);
    }
    EXDEBUG("win32 window registered");

    u32 window_style = WS_OVERLAPPEDWINDOW;
    u32 window_style_ex = 0;

    platform->window_handle = CreateWindowExA(window_style_ex, MAKEINTATOM(platform->window_atom),
                                              window_title, window_style,
                                              window_position.x, window_position.y,
                                              window_size.x, window_size.y,
                                              0, 0, platform->window_instance, 0);
    if (!platform->window_handle) {
        EXFATAL("failed to create win32 window");
        return(EX_FALSE);
    }
    EXDEBUG("win32 window created");
    
    if (info.fullscreen) {
        win32_window_toggle_fullscreen(platform->window_handle, &platform->window_placement);
        window->fullscreen = info.fullscreen;
    }
    ShowWindow(platform->window_handle, SW_SHOW);
    
    RAWINPUTDEVICE raw_input_device = {};
    raw_input_device.usUsagePage = 0x01;
    raw_input_device.usUsage = 0x02;
    raw_input_device.hwndTarget = platform->window_handle;
    if (!RegisterRawInputDevices(&raw_input_device, 1, sizeof(raw_input_device))) {
        EXFATAL("failed to register mouse as raw input device");
        return(EX_FALSE);
    }
    EXDEBUG("mouse registered as raw input device");    
    return(EX_TRUE);
}

internal void
os_window_destroy(os_window_t *window) {
    win32_t *platform = (win32_t *)window->platform;
    UnregisterClassA(MAKEINTATOM(platform->window_atom), platform->window_instance);
    DestroyWindow(platform->window_handle);
    VirtualFree(platform, 0, MEM_RELEASE);
}

internal void
os_window_update(os_window_t *window) {
    win32_t *platform = (win32_t *)window->platform;
    input_t *input = &window->input;

    input->mouse.left.pressed = EX_FALSE;
    input->mouse.left.released = EX_FALSE;
    input->mouse.right.pressed = EX_FALSE;
    input->mouse.right.released = EX_FALSE;
    input->mouse.middle.pressed = EX_FALSE;
    input->mouse.middle.released = EX_FALSE;
    input->mouse.x1.pressed = EX_FALSE;
    input->mouse.x1.released = EX_FALSE;
    input->mouse.x2.pressed = EX_FALSE;
    input->mouse.x2.released = EX_FALSE;
    input->mouse.delta_wheel = 0;
    input->mouse.delta_position.x = 0;
    input->mouse.delta_position.y = 0;
    
    MSG message;
    while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
        switch (message.message) {
            case WM_QUIT: {
                window->quit = EX_TRUE;
            } break;
            case WM_INPUT: {
                UINT size;
                GetRawInputData((HRAWINPUT)message.lParam, RID_INPUT, 0, &size, sizeof(RAWINPUTHEADER));
                char *buffer = (char *)_alloca(size);
                if (GetRawInputData((HRAWINPUT)message.lParam, RID_INPUT, buffer, &size, sizeof(RAWINPUTHEADER)) == size) {
                    RAWINPUT *raw_input = (RAWINPUT *)buffer;
                    if (raw_input->header.dwType == RIM_TYPEMOUSE && raw_input->data.mouse.usFlags == MOUSE_MOVE_RELATIVE){
                        input->mouse.delta_position.x += raw_input->data.mouse.lLastX;
                        input->mouse.delta_position.y += raw_input->data.mouse.lLastY;

                        USHORT button_flags = raw_input->data.mouse.usButtonFlags;
                        b32 left_button_down = input->mouse.left.down;
                        if (button_flags & RI_MOUSE_LEFT_BUTTON_DOWN) left_button_down = EX_TRUE;
                        if (button_flags & RI_MOUSE_LEFT_BUTTON_UP) left_button_down = EX_FALSE;
                        _os_process_digital_button(&input->mouse.left, left_button_down);

                        b32 right_button_down = input->mouse.right.down;
                        if (button_flags & RI_MOUSE_RIGHT_BUTTON_DOWN) right_button_down = EX_TRUE;
                        if (button_flags & RI_MOUSE_RIGHT_BUTTON_UP) right_button_down = EX_FALSE;
                        _os_process_digital_button(&input->mouse.right, right_button_down);
                            
                        b32 middle_button_down = input->mouse.middle.down;
                        if (button_flags & RI_MOUSE_MIDDLE_BUTTON_DOWN) middle_button_down = EX_TRUE;
                        if (button_flags & RI_MOUSE_MIDDLE_BUTTON_UP) middle_button_down = EX_FALSE;
                        _os_process_digital_button(&input->mouse.middle, middle_button_down);

                        b32 x1_button_down = input->mouse.x1.down;
                        if (button_flags & RI_MOUSE_BUTTON_4_DOWN) x1_button_down = EX_TRUE;
                        if (button_flags & RI_MOUSE_BUTTON_4_UP) x1_button_down = EX_FALSE;
                        _os_process_digital_button(&input->mouse.x1, x1_button_down);
                            
                        b32 x2_button_down = input->mouse.x2.down;
                        if (button_flags & RI_MOUSE_BUTTON_5_DOWN) x2_button_down = EX_TRUE;
                        if (button_flags & RI_MOUSE_BUTTON_5_UP) x2_button_down = EX_FALSE;
                        _os_process_digital_button(&input->mouse.x2, x2_button_down);
                            
                        if (raw_input->data.mouse.usButtonFlags & RI_MOUSE_WHEEL) {
                            input->mouse.delta_wheel += ((SHORT)raw_input->data.mouse.usButtonData) / WHEEL_DELTA;
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
    
    BYTE keyboard_state[KEY_MAX];
    if (!GetKeyboardState(keyboard_state)) return;
    for (s32 key = 0; key < KEY_MAX; key++) {
        _os_process_digital_button(input->keyboard + key, keyboard_state[key] >> 7);
    }
    
    vec2i window_size = win32_get_window_size(platform->window_handle);
        
    input->mouse.wheel += input->mouse.delta_wheel;
    
    POINT mouse_position;
    GetCursorPos(&mouse_position);
    ScreenToClient(platform->window_handle, &mouse_position);
    
    if (mouse_position.x < 0) mouse_position.x = 0;
    if (mouse_position.y < 0) mouse_position.y = 0;
    if (mouse_position.x > window_size.x) mouse_position.x = window_size.x;
    if (mouse_position.y > window_size.y) mouse_position.y = window_size.y;

    input->mouse.position.x = mouse_position.x;
    input->mouse.position.y = mouse_position.y;
}

internal vec2i
os_window_get_size(os_window_t *window) {
    win32_t *platform = (win32_t *)window->platform;
    vec2i result = win32_get_window_size(platform->window_handle);
    return(result);
}

internal void
os_window_toggle_fullscreen(os_window_t *window) {
    win32_t *platform = (win32_t *)window->platform;
    win32_window_toggle_fullscreen(platform->window_handle, &platform->window_placement);
    window->fullscreen = !window->fullscreen;
}

///////////////////////////////////////////
// NOTE(xkazu0x): system renderer functions

internal os_renderer_t
os_renderer_create(os_window_t *window, vec2i size) {
    win32_t *platform = (win32_t *)window->platform;
    
    os_renderer_t renderer = {};
    renderer.size = size;
    renderer.bytes_per_pixel = 4;
    renderer.pitch = renderer.size.x * renderer.bytes_per_pixel;
    
    platform->bitmap_info.bmiHeader.biSize = sizeof(platform->bitmap_info.bmiHeader);
    platform->bitmap_info.bmiHeader.biWidth = renderer.size.x;
    platform->bitmap_info.bmiHeader.biHeight = -renderer.size.y;
    platform->bitmap_info.bmiHeader.biPlanes = 1;
    platform->bitmap_info.bmiHeader.biBitCount = 32;
    platform->bitmap_info.bmiHeader.biCompression = BI_RGB;

    s32 renderer_memory_size = (renderer.size.x * renderer.size.y) * renderer.bytes_per_pixel;
    renderer.memory = VirtualAlloc(0, renderer_memory_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    
    EXINFO("renderer size: %dx%d", renderer.size.x, renderer.size.y);
    return(renderer);
}

internal void
os_renderer_draw_pixel(os_renderer_t *renderer, f32 x, f32 y, vec3f color) {
    s32 xx = round_f32_to_s32(x);
    s32 yy = round_f32_to_s32(y);

    if (xx < 0) xx = 0;
    if (yy < 0) yy = 0;
    
    if (xx > renderer->size.x) xx = renderer->size.x;
    if (yy > renderer->size.y) y = renderer->size.y;

    u32 out_color = ((round_f32_to_u32(color.r * 255.0f) << 16) |
                     (round_f32_to_u32(color.g * 255.0f) << 8) |
                     (round_f32_to_u32(color.b * 255.0f) << 0));

    u8 *row = ((u8 *)renderer->memory +
                 (xx * renderer->bytes_per_pixel) +
                 (yy * renderer->pitch));
    u32 *pixel = (u32 *)row;
    *pixel = out_color;
}

internal void
os_renderer_clear(os_renderer_t *renderer, vec3f color) {
    vec4f color_unpacked = _vec4f(color*255.0f, 255.0f);
    u32 out_color = bgra_pack4x8(color_unpacked);

    u8 *row = (u8 *)renderer->memory;
    
    for (s32 y = 0; y < renderer->size.y; ++y) {
        u32 *pixel = (u32 *)row;
        for (s32 x = 0; x < renderer->size.x; ++x) {
            *pixel++ = out_color;
        }
        row += renderer->pitch;
    }
}

internal void
os_renderer_present(os_renderer_t *renderer, os_window_t *window) {
    win32_t *platform = (win32_t *)window->platform;
    vec2i window_size = win32_get_window_size(platform->window_handle);
    HDC window_device = GetDC(platform->window_handle);
    StretchDIBits(window_device,
                  0, 0, window_size.x, window_size.y,
                  0, 0, renderer->size.x, renderer->size.y,
                  renderer->memory,
                  &platform->bitmap_info,
                  DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(platform->window_handle, window_device);
}

///////////////////////////////////////
// NOTE(xkazu0x): system time functions

internal s64
os_get_ticks_per_frame() {
    LARGE_INTEGER large_integer;
    QueryPerformanceFrequency(&large_integer);
    return(large_integer.QuadPart);
}

internal s64
os_get_time() {
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return(result.QuadPart);
}

internal void
os_sleep_ms(u32 ms) {
    Sleep(ms);
}
