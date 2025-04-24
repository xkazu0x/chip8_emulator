#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef s32 b32;

#define KILOBYTES(x) ((x)*1024LL)

#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32

struct chip8_t {
    u8 memory[KILOBYTES(4)];
    bool display[DISPLAY_WIDTH*DISPLAY_HEIGHT];
    u16 stack[12];
    u16 *stack_ptr;
    u8 v[16];
    u16 pc;
    u16 i;
    u8 delay_timer;
    u8 sound_timer;
    bool keypad[16];
    char *rom_name;
};

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: chip8 <rom_file_name>");
        return(1);
    }
    
    chip8_t chip8 = {};
    chip8.rom_name = argv[1];
    chip8.pc = 0x200;
    chip8.stack_ptr = &chip8.stack[0];
    
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
    memcpy(&chip8.memory[0], chip8_fontset, sizeof(chip8_fontset));

    // NOTE(xkazu0x): load rom
    FILE *rom = fopen(chip8.rom_name, "rb");
    if (!rom) {
        printf("rom file name is invalid: %s\n", chip8.rom_name);
        return(1);
    }
    
    fseek(rom, 0, SEEK_END);
    size_t rom_size = ftell(rom);
    size_t max_size = sizeof(chip8.memory) - chip8.pc;
    rewind(rom);

    if (rom_size > max_size) {
        printf("rom file max size reached: %lld/%lld\n", rom_size, max_size);
        return(1);
    }

    if (fread(&chip8.memory[chip8.pc], rom_size, 1, rom) != 1) {
        printf("failed to read rom file: %s\n", chip8.rom_name);
        return(1);
    }
    fclose(rom);
    
    printf("rom file loaded: %s\n", chip8.rom_name);
    printf("rom size: %lld bytes\n", rom_size);
    
    //b32 quit = false;
    //while (!quit) {
    for (u8 byte = 0; byte < rom_size; ++byte) {
        u16 opcode = (chip8.memory[chip8.pc] << 8 | chip8.memory[chip8.pc + 1]);
        chip8.pc += 2;

        u16 nnn = opcode & 0x0FFF;
        u8 nn = opcode & 0x0FF;
        u8 n = opcode & 0x0F;
        u8 x = (opcode >> 8) & 0x0F;
        u8 y = (opcode >> 4) & 0x0F;

        printf("address: 0x%04X, opcode: 0x%04X, desc: ", chip8.pc-2, opcode);
        switch ((opcode >> 12) & 0x0F) {
            case 0x00: {
                if (nn == 0xE0) {
                    // NOTE(xkazu0x): 0x00E0
                    printf("clear screen\n");
                    memset(&chip8.display[0], false, sizeof(chip8.display));
                } else if (nn == 0xEE) {
                    // NOTE(xkazu0x): 0x00EE
                    printf("return from subroutine to address 0x%04X\n", *(chip8.stack_ptr - 1));
                    chip8.pc = *--chip8.stack_ptr;
                } else {
                    printf("unimplemented\n");
                }
            } break;
            case 0x01: {
                // NOTE(xkazu0x): 0x1NNN
                printf("jump to address NNN (0x%04X)\n", nnn);
                chip8.pc = nnn;
            } break;
            case 0x02: {
                // NOTE(xkazu0x): 0x2NNN
                printf("call subroutine at NNN (0x%04X)\n", nnn);
                *chip8.stack_ptr++ = chip8.pc;
                chip8.pc = nnn;
            } break;
            case 0x06: {
                // NOTE(xkazu0x): 0x6XNN
                printf("V%X = NN (0x%02X)\n", x, nn);
                chip8.v[x] = nn;
            } break;
            case 0x07: {
                // NOTE(xkazu0x): 0x7XNN
                printf("V%X (0x%02X) += NN (0x%02X) => V%X (0x%02X)\n", x, chip8.v[x], nn, x, chip8.v[x] + nn);
                chip8.v[x] += nn;
            } break;
            case 0x0A: {
                // NOTE(xkazu0x): 0xANNN
                printf("set I to NNN (0x%04X)\n", nnn);
                chip8.i = nnn;
            } break;
            case 0x0D: {
                // NOTE(xkazu0x): 0xDXYN display
                printf("from I (0x%04X) draw N (%u) at V%X (0x%02X) V%X (0x%02X)\n",
                       chip8.i, n, x, chip8.v[x], y, chip8.v[y]);
                
                u8 coord_x = chip8.v[x] % DISPLAY_WIDTH;
                u8 coord_y = chip8.v[y] % DISPLAY_HEIGHT;

                u8 orig_x = coord_x;
                
                chip8.v[0xF] = 0; // initialize carry flag to 0

                // NOTE(xkazu0x): loop over all N rows of sprite
                for (u8 byte_index = 0;
                     byte_index < n;
                     ++byte_index) {
                    u8 sprite_byte = chip8.memory[chip8.i + byte_index];
                    coord_x = orig_x;
                    
                    for (s8 bit_index = 7;
                         bit_index >= 0;
                         --bit_index) {
                        // NOTE(xkazu0x) : if sprite pixel/bit is on and display pixel is on,
                        // set carry flag
                        bool *pixel = &chip8.display[coord_y*DISPLAY_WIDTH + coord_x];
                        bool sprite_bit = (sprite_byte & (1 << bit_index));
                        if ((sprite_bit) && (*pixel)) {
                            chip8.v[0xF] = 1;
                        }

                        // NOTE(xkazu0x): XOR display pixel with sprite pixel/bit
                        // to set it on or off
                        *pixel ^= sprite_bit;

                        if (++coord_x >= DISPLAY_WIDTH) break;
                    }
                    if (++coord_y >= DISPLAY_HEIGHT) break;
                }
            } break;
            default: {
                printf("unimplemented\n");
            } break;
        }
    }

    for (u32 i = 0; i < DISPLAY_WIDTH*DISPLAY_HEIGHT; ++i) {
        if (i % DISPLAY_WIDTH == 0) printf("\n");
        printf("%d", chip8.display[i]);
    }
    
    return(0);
}
