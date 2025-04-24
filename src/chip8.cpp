#include "excalibur/excalibur.h"

#define CHIP8_WIDTH 64
#define CHIP8_HEIGHT 32
#define CHIP8_SCALE 15

struct instruction_t {
    u16 opcode;
    u16 nnn;
    u8 nn;
    u8 n;
    u8 x;
    u8 y;
};

struct chip8_t {    
    u8 memory[KILOBYTES(4)];
    u8 v[16];
    
    u16 stack[12];
    u16 *stack_ptr;
    u16 pc;
    u16 i;

    b32 display[CHIP8_WIDTH*CHIP8_HEIGHT];
    b32 keypad[16];

    instruction_t inst;
    u8 delay_timer;
    u8 sound_timer;
    
    char *rom_name;
    size_t rom_size;
};

/////////////////////////////////
// NOTE(xkazu0x): chip8 functions
internal b32
chip8_initialize(chip8_t *chip8, char *rom_name) {
    chip8->rom_name = rom_name;
    chip8->pc = 0x200;
    chip8->stack_ptr = &chip8->stack[0];

    // NOTE(xkazu0x): load font
    u8 chip8_fontset[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0,
        0x20, 0x60, 0x20, 0x20, 0x70,
        0xF0, 0x10, 0xF0, 0x80, 0xF0,
        0xF0, 0x10, 0xF0, 0x10, 0xF0,
        0x90, 0x90, 0xF0, 0x10, 0x10,
        0xF0, 0x80, 0xF0, 0x10, 0xF0,
        0xF0, 0x80, 0xF0, 0x90, 0xF0,
        0xF0, 0x10, 0x20, 0x40, 0x40,
        0xF0, 0x90, 0xF0, 0x90, 0xF0,
        0xF0, 0x90, 0xF0, 0x10, 0xF0,
        0xF0, 0x90, 0xF0, 0x90, 0x90,
        0xE0, 0x90, 0xE0, 0x90, 0xE0,
        0xF0, 0x80, 0x80, 0x80, 0xF0,
        0xE0, 0x90, 0x90, 0x90, 0xE0,
        0xF0, 0x80, 0xF0, 0x80, 0xF0,
        0xF0, 0x80, 0xF0, 0x80, 0x80,
    };
    memcpy(&chip8->memory[0], chip8_fontset, sizeof(chip8_fontset));

    // NOTE(xkazu0x): load rom
    FILE *rom = fopen(chip8->rom_name, "rb");
    if (!rom) {
        printf("rom file name is invalid: %s\n", chip8->rom_name);
        return(false);
    }
    
    fseek(rom, 0, SEEK_END);
    size_t rom_size = ftell(rom);
    size_t max_size = sizeof(chip8->memory) - chip8->pc;
    rewind(rom);

    if (rom_size > max_size) {
        printf("rom file max size reached: %lld/%lld\n", rom_size, max_size);
        return(false);
    }
    
    if (fread(&chip8->memory[chip8->pc], rom_size, 1, rom) != 1) {
        printf("failed to read rom file: %s\n", chip8->rom_name);
        return(false);
    }
    fclose(rom);
    chip8->rom_size = rom_size;
    
    printf("rom file loaded: %s\n", chip8->rom_name);
    printf("rom size: %lld bytes\n", chip8->rom_size);

    return(true);
}

internal void
chip8_print(chip8_t *chip8) {
    printf("address: 0x%04X, opcode: 0x%04X, desc: ", chip8->pc-2, chip8->inst.opcode);
    switch ((chip8->inst.opcode >> 12) & 0x0F) {
        case 0x00: {
            if (chip8->inst.nn == 0xE0) {
                printf("clear screen\n");
            } else if (chip8->inst.nn == 0xEE) {
                printf("return from subroutine to address 0x%04X\n", *(chip8->stack_ptr - 1));
            } else {
                printf("unimplemented\n");
            }
        } break;
        case 0x01: {
            printf("jump to address NNN (0x%04X)\n", chip8->inst.nnn);
        } break;
        case 0x02: {
            printf("call subroutine at NNN (0x%04X)\n", chip8->inst.nnn);
        } break;
        case 0x06: {
            printf("V%X = NN (0x%02X)\n", chip8->inst.x, chip8->inst.nn);
        } break;
        case 0x07: {
            printf("V%X (0x%02X) += NN (0x%02X) => V%X (0x%02X)\n",
                   chip8->inst.x, chip8->v[chip8->inst.x], chip8->inst.nn,
                   chip8->inst.x, chip8->v[chip8->inst.x] + chip8->inst.nn);
        } break;
        case 0x0A: {
            printf("set I to NNN (0x%04X)\n", chip8->inst.nnn);
        } break;
        case 0x0D: {
            printf("from I (0x%04X) draw N (%u) at V%X (0x%02X) V%X (0x%02X)\n",
                   chip8->i, chip8->inst.n,
                   chip8->inst.x, chip8->v[chip8->inst.x],
                   chip8->inst.y, chip8->v[chip8->inst.y]);
        } break;
        default: {
            printf("unimplemented\n");
        } break;
    }
}

internal void
chip8_emulate(chip8_t *chip8) {
    chip8->inst.opcode = (chip8->memory[chip8->pc] << 8 | chip8->memory[chip8->pc + 1]);
    chip8->pc += 2;

    chip8->inst.nnn = chip8->inst.opcode & 0x0FFF;
    chip8->inst.nn  = chip8->inst.opcode & 0x0FF;
    chip8->inst.n   = chip8->inst.opcode & 0x0F;
    chip8->inst.x   = (chip8->inst.opcode >> 8) & 0x0F;
    chip8->inst.y   = (chip8->inst.opcode >> 4) & 0x0F;

    //printf("address: 0x%04X, opcode: 0x%04X, desc: ", chip8->pc-2, chip8->inst.opcode);
    switch ((chip8->inst.opcode >> 12) & 0x0F) {
        case 0x00: {
            if (chip8->inst.nn == 0xE0) {
                // NOTE(xkazu0x): 0x00E0
                //printf("clear screen\n");
                memset(&chip8->display[0], false, sizeof(chip8->display));
            } else if (chip8->inst.nn == 0xEE) {
                // NOTE(xkazu0x): 0x00EE
                //printf("return from subroutine to address 0x%04X\n", *(chip8->stack_ptr - 1));
                chip8->pc = *--chip8->stack_ptr;
            } else {
                //printf("unimplemented\n");
            }
        } break;
        case 0x01: {
            // NOTE(xkazu0x): 0x1NNN
            //printf("jump to address NNN (0x%04X)\n", chip8->inst.nnn);
            chip8->pc = chip8->inst.nnn;
        } break;
        case 0x02: {
            // NOTE(xkazu0x): 0x2NNN
            //printf("call subroutine at NNN (0x%04X)\n", chip8->inst.nnn);
            *chip8->stack_ptr++ = chip8->pc;
            chip8->pc = chip8->inst.nnn;
        } break;
        case 0x06: {
            // NOTE(xkazu0x): 0x6XNN
            //printf("V%X = NN (0x%02X)\n", chip8->inst.x, chip8->inst.nn);
            chip8->v[chip8->inst.x] = chip8->inst.nn;
        } break;
        case 0x07: {
            // NOTE(xkazu0x): 0x7XNN
            //printf("V%X (0x%02X) += NN (0x%02X) => V%X (0x%02X)\n",
            //chip8->inst.x, chip8->v[chip8->inst.x], chip8->inst.nn,
            //chip8->inst.x, chip8->v[chip8->inst.x] + chip8->inst.nn);
            chip8->v[chip8->inst.x] += chip8->inst.nn;
        } break;
        case 0x0A: {
            // NOTE(xkazu0x): 0xANNN
            //printf("set I to NNN (0x%04X)\n", chip8->inst.nnn);
            chip8->i = chip8->inst.nnn;
        } break;
        case 0x0D: {
            // NOTE(xkazu0x): 0xDXYN display
            //printf("from I (0x%04X) draw N (%u) at V%X (0x%02X) V%X (0x%02X)\n",
            //chip8->i, chip8->inst.n,
            //chip8->inst.x, chip8->v[chip8->inst.x],
            //chip8->inst.y, chip8->v[chip8->inst.y]);
                
            u8 coord_x = chip8->v[chip8->inst.x] % CHIP8_WIDTH;
            u8 coord_y = chip8->v[chip8->inst.y] % CHIP8_HEIGHT;

            u8 orig_x = coord_x;
                
            chip8->v[0xF] = 0; // initialize carry flag to 0

            // NOTE(xkazu0x): loop over all N rows of sprite
            for (u8 byte_index = 0;
                 byte_index < chip8->inst.n;
                 ++byte_index) {
                u8 sprite_byte = chip8->memory[chip8->i + byte_index];
                coord_x = orig_x;
                    
                for (s8 bit_index = 7;
                     bit_index >= 0;
                     --bit_index) {
                    // NOTE(xkazu0x) : if sprite pixel/bit is on and display pixel is on,
                    // set carry flag
                    b32 *pixel = &chip8->display[coord_y*CHIP8_WIDTH + coord_x];
                    b32 sprite_bit = (sprite_byte & (1 << bit_index));
                    if ((sprite_bit) && (*pixel)) {
                        chip8->v[0xF] = 1;
                    }

                    // NOTE(xkazu0x): XOR display pixel with sprite pixel/bit
                    // to set it on or off
                    *pixel ^= sprite_bit;

                    if (++coord_x >= CHIP8_WIDTH) break;
                }
                if (++coord_y >= CHIP8_HEIGHT) break;
            }
        } break;
        default: {
            //printf("unimplemented\n");
        } break;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: chip8 <rom_file_name>");
        return(1);
    }

    chip8_t chip8 = {};
    if (!chip8_initialize(&chip8, argv[1])) {
        return(1);
    }

    os_window_info_t window_info = {};
    window_info.fullscreen = EX_FALSE;
    window_info.size.x = CHIP8_SCALE*CHIP8_WIDTH;
    window_info.size.y = CHIP8_SCALE*CHIP8_HEIGHT;
    window_info.title = "CHIP8";
        
    os_window_t window = {};
    if (!os_window_create(&window, window_info)) {
        return(1);
    }
    
    os_renderer_t renderer = os_renderer_create(&window, {CHIP8_WIDTH, CHIP8_HEIGHT});
    
    b32 pause = EX_FALSE;
    while (!window.quit) {
        os_window_update(&window);
        
        if (window.input.keyboard[KEY_ESCAPE].pressed) {
            window.quit = EX_TRUE;
        }
        
        if (window.input.keyboard[KEY_SPACE].pressed) {
            pause = !pause;
            if (pause) {
                EXINFO("> --paused--");
            } else {
                EXINFO("> ++unpaused++");
            }
        }
        
        if (pause) continue;
        chip8_emulate(&chip8);

        for (s32 y = 0; y < CHIP8_HEIGHT; ++y) {
            for (s32 x = 0; x < CHIP8_WIDTH; ++x) {
                vec3f out_color = _vec3f(0.0f);
                if (chip8.display[y*CHIP8_WIDTH + x]) {
                    out_color = _vec3f(1.0f);
                }
                os_renderer_draw_pixel(&renderer, x, y, out_color);
            }
        }
        
        os_renderer_present(&renderer, &window);
    }
    
    os_window_destroy(&window);
    return(0);
}
