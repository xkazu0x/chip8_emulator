#include "base.h"
#include "math.h"
#include "os.h"

#include "base.cpp"
#include "math.cpp"
#include "os.cpp"

#include <time.h>

#define RENDER_WIDTH 64
#define RENDER_HEIGHT 32

#define WINDOW_SCALE 15

#define WINDOW_WIDTH WINDOW_SCALE*RENDER_WIDTH
#define WINDOW_HEIGHT WINDOW_SCALE*RENDER_HEIGHT

// TODO(xkazu0x): make a config flags for CHIP8 and SCHIP
// TODO(xkazu0x): delay timer
// TODO(xkazu0x): sound timer

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

    b32 display[RENDER_WIDTH*RENDER_HEIGHT];
    b32 keypad[16];

    instruction_t inst;
    u8 delay_timer;
    u8 sound_timer;
    
    char *rom_name;
    size_t rom_size;
};

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
        print("rom file name is invalid: %s\n", chip8->rom_name);
        return(false);
    }
    
    fseek(rom, 0, SEEK_END);
    size_t rom_size = ftell(rom);
    size_t max_size = sizeof(chip8->memory) - chip8->pc;
    rewind(rom);

    if (rom_size > max_size) {
        print("rom file max size reached: %lld/%lld\n", rom_size, max_size);
        return(false);
    }
    
    if (fread(&chip8->memory[chip8->pc], rom_size, 1, rom) != 1) {
        print("failed to read rom file: %s\n", chip8->rom_name);
        return(false);
    }
    fclose(rom);
    chip8->rom_size = rom_size;
    
    print("rom file: %s\n", chip8->rom_name);
    print("rom size: %lld bytes\n", chip8->rom_size);

    return(true);
}

internal void
chip8_emulate(chip8_t *chip8) {
    chip8->inst.opcode = (chip8->memory[chip8->pc] << 8 | chip8->memory[chip8->pc + 1]);
    
    print("addr: 0x%04X, opcode: 0x%04X, desc: ", chip8->pc, chip8->inst.opcode);
    chip8->pc += 2;

    chip8->inst.nnn = chip8->inst.opcode & 0x0FFF;
    chip8->inst.nn  = chip8->inst.opcode & 0x0FF;
    chip8->inst.n   = chip8->inst.opcode & 0x0F;
    chip8->inst.x   = (chip8->inst.opcode >> 8) & 0x0F;
    chip8->inst.y   = (chip8->inst.opcode >> 4) & 0x0F;

    switch ((chip8->inst.opcode >> 12) & 0x0F) {
        case 0x00: {
            // NOTE(xkazu0x): 0x00E0
            if (chip8->inst.nn == 0xE0) {
                print("clear screen\n");
                memset(&chip8->display[0], false, sizeof(chip8->display));
            }
            // NOTE(xkazu0x): 0x00EE
            else if (chip8->inst.nn == 0xEE) {
                print("return from subroutine to address 0x%04X\n", *(chip8->stack_ptr - 1));
                chip8->pc = *--chip8->stack_ptr;
            }
            // NOTE(xkazu0x): 0x0NNN
            else {
                // TODO(xkazu0x): calls machine code routine at address NNN
            }
        } break;
        
        // NOTE(xkazu0x): 0x1NNN
        case 0x01: {
            print("[jump] PC = NNN (0x%04X)\n", chip8->inst.nnn);
            chip8->pc = chip8->inst.nnn;
        } break;
        
        // NOTE(xkazu0x): 0x2NNN
        case 0x02: {
            print("call subroutine at NNN (0x%04X)\n", chip8->inst.nnn);
            *chip8->stack_ptr++ = chip8->pc;
            chip8->pc = chip8->inst.nnn;
        } break;
        
        // NOTE(xkazu0x): 0x3XNN
        case 0x03: {
            print("if V%X (0x%02X) == NN (0x%02X), skip the next instrunction\n",
                  chip8->inst.x, chip8->v[chip8->inst.x], chip8->inst.nn);
            if (chip8->v[chip8->inst.x] == chip8->inst.nn) {
                chip8->pc += 2;
            }
        } break;
        
        // NOTE(xkazu0x): 0x4XNN
        case 0x04: {
            print("if V%X (0x%02X) != NN (0x%02X), skip the next instrunction\n",
                  chip8->inst.x, chip8->v[chip8->inst.x], chip8->inst.nn);
            if (chip8->v[chip8->inst.x] != chip8->inst.nn) {
                chip8->pc += 2;
            }
        } break;
        
        // NOTE(xkazu0x): 0x5XY0
        case 0x05: {
            if (chip8->inst.n != 0) break; // NOTE(xkazu0x): wrong opcode
            
            print("if V%X (0x%02X) == V%X (0x%02X), skip the next instrunction\n",
                  chip8->inst.x, chip8->v[chip8->inst.x],
                  chip8->inst.y, chip8->v[chip8->inst.y]);
            if (chip8->v[chip8->inst.x] == chip8->v[chip8->inst.y]) {
                chip8->pc += 2;
            }
        } break;
        
        // NOTE(xkazu0x): 0x6XNN
        case 0x06: {
            print("V%X = NN (0x%02X)\n", chip8->inst.x, chip8->inst.nn);
            chip8->v[chip8->inst.x] = chip8->inst.nn;
        } break;
        
        // NOTE(xkazu0x): 0x7XNN
        case 0x07: {
            print("V%X (0x%02X) += NN (0x%02X) => V%X (0x%02X)\n",
                  chip8->inst.x, chip8->v[chip8->inst.x], chip8->inst.nn,
                  chip8->inst.x, chip8->v[chip8->inst.x] + chip8->inst.nn);
            chip8->v[chip8->inst.x] += chip8->inst.nn;
        } break;

        // NOTE(xkzu0x): 0x8XYN
        case 0x08: {
            switch (chip8->inst.n) {
                // NOTE(xkzu0x): 0x8XY0
                case 0: {
                    print("V%X = V%X (0x%02X)\n",
                          chip8->inst.x, chip8->inst.y, chip8->v[chip8->inst.y]);
                    chip8->v[chip8->inst.x] = chip8->v[chip8->inst.y];
                } break;

                // NOTE(xkzu0x): 0x8XY1
                case 1: {
                    print("V%X (0x%02X) |= V%X (0x%02X); result: 0x%02X\n",
                          chip8->inst.x, chip8->v[chip8->inst.x],
                          chip8->inst.y, chip8->v[chip8->inst.y],
                          chip8->v[chip8->inst.x] | chip8->v[chip8->inst.y]);
                    chip8->v[chip8->inst.x] |= chip8->v[chip8->inst.y];
                } break;
                
                // NOTE(xkzu0x): 0x8XY2
                case 2: {
                    print("V%X (0x%02X) &= V%X (0x%02X); result: 0x%02X\n",
                          chip8->inst.x, chip8->v[chip8->inst.x],
                          chip8->inst.y, chip8->v[chip8->inst.y],
                          chip8->v[chip8->inst.x] & chip8->v[chip8->inst.y]);
                    chip8->v[chip8->inst.x] &= chip8->v[chip8->inst.y];
                } break;
                
                // NOTE(xkzu0x): 0x8XY3
                case 3: {
                    print("V%X (0x%02X) ^= V%X (0x%02X); result: 0x%02X\n",
                          chip8->inst.x, chip8->v[chip8->inst.x],
                          chip8->inst.y, chip8->v[chip8->inst.y],
                          chip8->v[chip8->inst.x] ^ chip8->v[chip8->inst.y]);
                    chip8->v[chip8->inst.x] ^= chip8->v[chip8->inst.y];
                } break;
                
                // NOTE(xkzu0x): 0x8XY4
                case 4: {
                    print("V%X (0x%02X) += V%X (0x%02X), VF = 1 if overflow; result: 0x%02X",
                          chip8->inst.x, chip8->v[chip8->inst.x],
                          chip8->inst.y, chip8->v[chip8->inst.y],
                          chip8->v[chip8->inst.x] + chip8->v[chip8->inst.y])
                        
                        if ((u16)(chip8->v[chip8->inst.x] + chip8->v[chip8->inst.y]) > u8_max) {
                            chip8->v[0xF] = 1;
                        } else {
                            chip8->v[0xF] = 0;
                        }
                    chip8->v[chip8->inst.x] += chip8->v[chip8->inst.y];
                    
                    print(", VF = %X\n", chip8->v[0xF]);
                } break;
                
                // NOTE(xkzu0x): 0x8XY5
                case 5: {
                    print("V%X (0x%02X) -= V%X (0x%02X), VF = 0 if underflow; result: 0x%02X",
                          chip8->inst.x, chip8->v[chip8->inst.y],
                          chip8->inst.y, chip8->v[chip8->inst.y],
                          chip8->v[chip8->inst.x] - chip8->v[chip8->inst.y]);
                    
                    if (chip8->v[chip8->inst.x] >= chip8->v[chip8->inst.y]) {
                        chip8->v[0xF] = 1;
                    } else {
                        chip8->v[0xF] = 0;
                    }
                    chip8->v[chip8->inst.x] -= chip8->v[chip8->inst.y];

                    print(", VF = %X\n", chip8->v[0xF]);
                } break;
                
                // NOTE(xkzu0x): 0x8XY6
                case 6: {
                    print("set V%X (0x%02X) >>= 1, VF = shifted off bit (%X); result: 0x%02X\n",
                          chip8->inst.x, chip8->v[chip8->inst.x],
                          chip8->v[chip8->inst.x] & 1,
                          chip8->v[chip8->inst.x] >> 1);
                    
                    chip8->v[0xF] = chip8->v[chip8->inst.x] & 1;
                    chip8->v[chip8->inst.x] >>= 1;
                } break;
                
                // NOTE(xkzu0x): 0x8XY7
                case 7: {
                    print("set V%X = V%X (0x%02X) - V%X (0x%02X), VF = 0 if underflow; result: 0x%02X",
                          chip8->inst.x,
                          chip8->inst.y, chip8->v[chip8->inst.y],
                          chip8->inst.x, chip8->v[chip8->inst.x],
                          chip8->v[chip8->inst.y] - chip8->v[chip8->inst.x]);
                        
                    if (chip8->v[chip8->inst.y] >= chip8->v[chip8->inst.x])
                    {
                        chip8->v[0xF] = 1;
                    } else {
                        chip8->v[0xF] = 0;
                    }
                    chip8->v[chip8->inst.x] = chip8->v[chip8->inst.y] - chip8->v[chip8->inst.x];

                    print(", VF = %X\n", chip8->v[0xF]);
                } break;

                // NOTE(xkzu0x): 0x8XYE
                case 0xE: {
                    print("set V%X (0x%02X) <<= 1, VF = shifted off bit (%X); result: 0x%02X\n",
                          chip8->inst.x, chip8->v[chip8->inst.x],
                          (chip8->v[chip8->inst.x] & 0x80) >> 7,
                          chip8->v[chip8->inst.x] << 1);
                    
                    chip8->v[0xF] = (chip8->v[chip8->inst.x] & 0x80) >> 7;
                    chip8->v[chip8->inst.x] <<= 1;
                } break;
                
                default: {
                    // NOTE(xkazu0x): unimplemented/wrong
                } break;
            }
        } break;

        // NOTE(xkazu0x): 0x9XY0
        case 0x09: {
            if (chip8->inst.n != 0) break; // NOTE(xkazu0x): wrong opcode

            print("if V%X (0x%02X) != V%X (0x%02X), skip the next instruction\n",
                  chip8->inst.x, chip8->v[chip8->inst.x],
                  chip8->inst.y, chip8->v[chip8->inst.y]);
            if (chip8->v[chip8->inst.x] != chip8->v[chip8->inst.y]) {
                chip8->pc += 2;
            }
        } break;
        
        // NOTE(xkazu0x): 0xANNN
        case 0x0A: {
            print("I = NNN (0x%04X)\n", chip8->inst.nnn);
            chip8->i = chip8->inst.nnn;
        } break;

        case 0x0B: {
            print("[jump] PC = NNN (0x%04X) + V0 (0x%04X); result: (0x%04X)\n",
                  chip8->inst.nnn, chip8->v[0],
                  chip8->v[0] + chip8->inst.nnn);
            chip8->pc = chip8->v[0] + chip8->inst.nnn;
        } break;

        // NOTE(xkazu0x): 0xCXNN
        case 0x0C: {
            print("V%X = (rand() % 256) & NN (0x%02X)\n", chip8->inst.x, chip8->inst.nn);
            chip8->v[chip8->inst.x] = (rand() % 256) & chip8->inst.nn;
        } break;
        
        // NOTE(xkazu0x): 0xDXYN
        case 0x0D: {
            print("from I (0x%04X) draw N (%u) at V%X (0x%02X) V%X (0x%02X)\n",
                  chip8->i, chip8->inst.n,
                  chip8->inst.x, chip8->v[chip8->inst.x],
                  chip8->inst.y, chip8->v[chip8->inst.y]);
            
            u8 coord_x = chip8->v[chip8->inst.x] % RENDER_WIDTH;
            u8 coord_y = chip8->v[chip8->inst.y] % RENDER_HEIGHT;

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
                    b32 *pixel = &chip8->display[coord_y*RENDER_WIDTH + coord_x];
                    b32 sprite_bit = (sprite_byte & (1 << bit_index));
                    if ((sprite_bit) && (*pixel)) {
                        chip8->v[0xF] = 1;
                    }
                    
                    // NOTE(xkazu0x): XOR display pixel with sprite pixel/bit
                    // to set it on or off
                    *pixel ^= sprite_bit;
                    
                    if (++coord_x >= RENDER_WIDTH) break;
                }
                if (++coord_y >= RENDER_HEIGHT) break;
            }
        } break;
        
        case 0x0E: {
            // NOTE(xkazu0x): 0xEX9E
            if (chip8->inst.nn == 0x9E) {
                print("if key (%d) in V%X (0x%02X) is pressed, skip to the next instruction\n",
                      chip8->keypad[chip8->v[chip8->inst.x]],
                      chip8->inst.x, chip8->v[chip8->inst.x]);
                if (chip8->keypad[chip8->v[chip8->inst.x]]) {
                    chip8->pc += 2;
                }
            }
            // NOTE(xkazu0x): 0xEXA1
            else if (chip8->inst.nn == 0xA1) {
                print("if key (%d) in V%X (0x%02X) is not pressed, skip to the next instruction\n",
                      chip8->keypad[chip8->v[chip8->inst.x]],
                      chip8->inst.x, chip8->v[chip8->inst.x]);
                if (!chip8->keypad[chip8->v[chip8->inst.x]]) {
                    chip8->pc += 2;
                }
            }
        } break;
            
        case 0x0F: {
            switch (chip8->inst.nn) {
                // NOTE(xkazu0x): 0xFX07
                case 0x07: {
                    print("V%X = delay timer\n", chip8->inst.x, chip8->delay_timer);
                    chip8->v[chip8->inst.x] = chip8->delay_timer;
                } break;
                
                // NOTE(xkazu0x): 0xFX0A
                case 0x0A: {
                    print("await until a key is pressed; store key in V%X\n", chip8->inst.x);
                    b32 key_pressed = false;
                    while (!key_pressed) {
                        for (u32 key_index = 0;
                             key_index < ARRAY_COUNT(chip8->keypad);
                             ++key_index) {
                            if (chip8->keypad[key_index]) {
                                chip8->v[chip8->inst.x] = chip8->keypad[key_index];
                                key_pressed = true;
                                break;
                            }
                        }

                        // TODO(xkazu0x):
                        // if (!key_pressed) chip8->pc -= 2;
                    }
                } break;
                    
                // NOTE(xkazu0x): 0xFX15
                case 0x15: {
                    print("delay timer = V%X (0x%02X)\n",
                          chip8->inst.x, chip8->v[chip8->inst.x]);
                    chip8->delay_timer = chip8->v[chip8->inst.x];
                } break;
                    
                // NOTE(xkazu0x): 0xFX18
                case 0x18: {
                    print("sound timer = V%X (0x%02X)\n",
                          chip8->inst.x, chip8->v[chip8->inst.x]);
                    chip8->sound_timer = chip8->v[chip8->inst.x];
                } break;
                    
                // NOTE(xkazu0x): 0xFX1E
                case 0x1E: {
                    print("I (0x%04X) += V%X (0x%02X); result: (0x%04X)\n",
                          chip8->i, chip8->v[chip8->inst.x],
                          chip8->i + chip8->v[chip8->inst.x]);
                    chip8->i += chip8->v[chip8->inst.x];
                 } break;
                    
                // NOTE(xkazu0x): 0xFX29
                case 0x29: {
                    print("set I to sprite location in memory for character in V%X (0x%02X); result(V%X) = (0x%02X)\n",
                          chip8->inst.x, chip8->v[chip8->inst.x],
                          chip8->inst.x, chip8->v[chip8->inst.x]*5);
                    chip8->i = chip8->v[chip8->inst.x]*5;
                } break;
                    
                // NOTE(xkazu0x): 0xFX33
                case 0x33: {
                    // NOTE(xkazu0x): store BCD representation of VX at memory offset from I
                    // I = hundred's place, I+1 = ten's place, I+2 = one's place
                    print("store BCD of V%X (0x%02X) at memory from I (0x%04X)\n",
                          chip8->inst.x, chip8->v[chip8->inst.x], chip8->i);
                    u8 bcd = chip8->v[chip8->inst.x];
                    chip8->memory[chip8->i + 2] = (bcd % 10);
                    bcd /= 10;
                    chip8->memory[chip8->i + 1] = (bcd % 10);
                    bcd /= 10;
                    chip8->memory[chip8->i] = bcd;
                } break;
                    
                // NOTE(xkazu0x): 0xFX55
                case 0x55: {
                    // NOTE(xkazu0x): register dum V0-VX inclusive to memory offset from I;
                    // SCHIP does not increment I, CHIP8 does increment I
                    print("register dump V0-V%X (0x%02X) inclusive at memory from I (0x%04X)\n",
                          chip8->inst.x, chip8->v[chip8->inst.x], chip8->i);
                    for (u8 register_index = 0;
                         register_index <= chip8->inst.x;
                         ++register_index) {
                        chip8->memory[chip8->i + register_index] = chip8->v[register_index];
                    }
                } break;
                    
                // NOTE(xkazu0x): 0xFX65
                case 0x65: {
                    print("register load V0-V%X (0x%02X) inclusive at memory from I (0x%04X)\n",
                          chip8->inst.x, chip8->v[chip8->inst.x], chip8->i);
                    for (u8 register_index = 0;
                         register_index <= chip8->inst.x;
                         ++register_index) {
                        chip8->v[register_index] = chip8->memory[chip8->i + register_index];
                    }
                } break;
                    
                default: {
                    print("unimplemented/wrong opcode\n");
                } break;
            }
        } break;
            
        default: {
            print("unimplemented opcode\n");
        }
    }
}

internal void
chip8_update_keypad(chip8_t *chip8, input_t *input) {
    // NOTE(xkazu0x):
    // chip8 keypad - QWERT
    // 1 2 3 C        1 2 3 4
    // 4 5 6 D        Q W E R
    // 7 8 9 E        A S D F
    // A 0 B F        Z X C V

    chip8->keypad[0x1] = input->keyboard[KEY_1].pressed;
    chip8->keypad[0x2] = input->keyboard[KEY_2].pressed;
    chip8->keypad[0x3] = input->keyboard[KEY_3].pressed;
    chip8->keypad[0xC] = input->keyboard[KEY_4].pressed;
    
    chip8->keypad[0x4] = input->keyboard[KEY_Q].pressed;
    chip8->keypad[0x5] = input->keyboard[KEY_W].pressed;
    chip8->keypad[0x6] = input->keyboard[KEY_E].pressed;
    chip8->keypad[0xD] = input->keyboard[KEY_R].pressed;
    
    chip8->keypad[0x7] = input->keyboard[KEY_A].pressed;
    chip8->keypad[0x8] = input->keyboard[KEY_S].pressed;
    chip8->keypad[0x9] = input->keyboard[KEY_D].pressed;
    chip8->keypad[0xE] = input->keyboard[KEY_F].pressed;
    
    chip8->keypad[0xA] = input->keyboard[KEY_Z].pressed;
    chip8->keypad[0x0] = input->keyboard[KEY_X].pressed;
    chip8->keypad[0xB] = input->keyboard[KEY_C].pressed;
    chip8->keypad[0xF] = input->keyboard[KEY_V].pressed;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: chip8 <rom_file_name>");
        exit(EXIT_FAILURE);
    }

    chip8_t chip8 = {};
    if (!chip8_initialize(&chip8, argv[1])) {
        exit(EXIT_FAILURE);
    }
        
    window_t window = create_window("chip8 emulator", WINDOW_WIDTH, WINDOW_HEIGHT);
    renderer_t renderer = create_renderer(&window, RENDER_WIDTH, RENDER_HEIGHT);
    input_t input = create_input(&window);

    srand(time(0));
    
    b32 pause = false;
    while (!window.should_quit) {
        update_window_events(&window, &input);
        
        if (input.keyboard[KEY_ESCAPE].pressed) {
            window.should_quit = true;
            break;
        }
        
        if (input.keyboard[KEY_F11].pressed) {
            toggle_window_fullscreen(&window);
        }

        if (input.keyboard[KEY_SPACE].pressed) {
            pause = !pause;
            if (pause) {
                print("[paused]\n");
            } else {
                print("[unpaused]\n");
            }
        }
        
        if (pause) continue;
        chip8_update_keypad(&chip8, &input);
        chip8_emulate(&chip8);
        
        for (s32 y = 0; y < renderer.height; ++y) {
            for (s32 x = 0; x < renderer.width; ++x) {
                vec3 out_color = make_vec3(0.0f);
                if (chip8.display[y*renderer.width + x]) {
                    out_color = make_vec3(1.0f);
                }
                renderer_draw_pixel(&renderer, x, y, out_color);
            }
        }
        
        renderer_present(&renderer, &window);
    }

    destroy_window(&window);
    return(0);
}
