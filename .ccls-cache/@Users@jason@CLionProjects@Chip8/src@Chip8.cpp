#include "Chip8.h"

#include <iostream>

unsigned char chip8_fontset[80] =
{
        0xF0, 0x90, 0x90, 0x90, 0xF0, //0
        0x20, 0x60, 0x20, 0x20, 0x70, //1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, //2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, //3
        0x90, 0x90, 0xF0, 0x10, 0x10, //4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, //5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, //6
        0xF0, 0x10, 0x20, 0x40, 0x40, //7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, //8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, //9
        0xF0, 0x90, 0xF0, 0x90, 0x90, //A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, //B
        0xF0, 0x80, 0x80, 0x80, 0xF0, //C
        0xE0, 0x90, 0x90, 0x90, 0xE0, //D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, //E
        0xF0, 0x80, 0xF0, 0x80, 0x80  //F
};

Chip8::Chip8()
{
    Init();
}

void Chip8::Init()
{
    pc = &memory[0x200];
    I = 0;
    sp = stack;

    for (int i = 0; i < 2048; i++)
    {
        display[i] = 0;
    }

    for (int i = 0; i < 16; i++)
    {
        stack[i] = 0;
        key[i] = 0;
        registers[i] = 0;
    }

    for (int i = 0; i < 80; i++)
    {
        memory[i] = chip8_fontset[i];
    }

    delayTimer = 0;
    soundTimer = 0;
}

bool Chip8::LoadRom(const char* path)
{
    printf("Loading ROM: %s\n", path);

    FILE* rom = fopen(path, "rb");
    if (rom == nullptr)
    {
        std::cerr << "Failed to open ROM!" << std::endl;
        return false;
    }

    // Get the file size
    fseek(rom, 0, SEEK_END);
    long size = ftell(rom);
    rewind(rom);

    uint8_t romBuffer[size];
    size_t result = fread(romBuffer, sizeof(uint8_t), (size_t)size, rom);
    if (result != size)
    {
        std::cerr << "Failed to read rom into buffer" << std::endl;
        return false;
    }

    if (4096-512 < size)
    {
        std::cerr << "ROM is too large" << std::endl;
        return false;
    }

    for (int i = 0; i < size; i++)
    {
        memory[i + 0x200] = romBuffer[i];
    }

    fclose(rom);
    return true;
}

void Chip8::Update()
{
    uint16_t opcode = (*pc << 8) | *(pc + 1);

    switch ((opcode & 0xF000) >> 12)
    {
        case 0x0:
        {
            switch (opcode & 0xFF)
            {
                // 00E0: Clear the display
                case 0xE0:
                {
                    for (int i = 0; i < 2048; i++) display[i] = 0;
                    drawFlag = true;
                    pc += 2;
                    printf("0x%X: Clearing the screen\n");
                    break;
                }

                // 00EE: Return pc to the value at the top of the stack; decrement sp
                case 0xEE:
                {
                    pc = *(--sp);
                    printf("0x%X: Returning to %x from subroutine\n", opcode, pc - memory);
                    pc += 2;
                    break;
                }

                default:
                {
                    printf("Unknown opcode: %X\n", opcode);
                    pause();
                    break;
                }
            }

            break;
        }

        // 1nnn: Jump to location nnn
        case 0x1:
        {
            uint16_t val = opcode & 0xFFF;
            pc = memory + val;
            printf("0x%X: Setting pc to %X\n", pc - memory);
            break;
        }

        /*
         * 2nnn: Call subroutine at nnn
         *
         * Increments stack pointer and puts pc at the top of the stack. PC is then set to nnn.
         */
        case 0x2:
        {
            uint16_t val = opcode & 0xFFF;
            *sp = pc;
            sp++;
            pc = memory + val;
            printf("0x%X: Going to subroutine at 0x%X, 0x%X on stack\n", opcode, pc - memory, *(sp - 1));
            break;
        }

        // 3xkk: Skips next instruction if Vx == kk
        case 0x3:
        {
            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t val = opcode & 0xFF;
            if (registers[x] == val)
            {
                pc += 4;
                printf("0x%X: Skipping next instruction; V[%X] == %X\n", opcode, x, val);
            } else {
                pc += 2;
                printf("0x%X: Not skipping next instruction; V[%X] != %X\n", opcode, x, val);
            }
            break;
        }

        // 4xkk: Skip next instruction if Vx != kk.
        case 0x4:
        {
            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t val = opcode & 0xFF;

            if (registers[x] != val)
            {
                pc += 4;
                printf("0x%X: Skipping next instruction; V[%X] != %X\n", opcode, x, val);
            } else {
                pc += 2;
                printf("0x%X: Not skipping next instruction; V[%X] == %X\n", opcode, x, val);
            }
            break;
        }

        // 5XY0: Skip next instruction if Vx = Vy
        case 0x5:
        {
            uint8_t x = (opcode & 0xF00) >> 8;
            uint8_t y = (opcode & 0xF0) >> 4;
            if (registers[x] == registers[y])
            {
                pc += 4;
                printf("0x%X: Skipping next instruction; V[%X] == V[%X]", opcode, x, y);
            } else {
                pc += 2;
                printf("0x%X: Not skipping next instruction; V[%X] != V[%X]", opcode, x, y);
            }
            break;
        }

        // 6xkk: set Vx = kk
        case 0x6:
        {
            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t val = opcode & 0xFF;
            registers[x] = val;
            printf("0x%X: Setting V[%X] to %X\n", opcode, x, val);
            pc += 2;
            break;
        }

        // 7xkk: Vx += kk
        case 0x7:
        {
            uint8_t x = (opcode & 0xF00) >> 8;
            uint8_t val = opcode & 0xFF;
            registers[x] += val;
            printf("0x%X: Incrementing V[%X] by %X\n", opcode, x, val);
            pc += 2;
            break;
        }

        case 0x8:
        {
            switch (opcode & 0xF)
            {
                // 8xy0: set Vx = Vy
                case 0x0:
                {
                    uint8_t x = (opcode & 0xF00) >> 8;
                    uint8_t y = (opcode & 0xF0) >> 4;
                    registers[x] = registers[y];
                    printf("0x%X: V[%X] = V[%X]\n", opcode, x, y);
                    pc += 2;
                    break;
                }

                // 8XY1: Set Vx = Vx OR Vy
                case 0x1:
                {
                    uint8_t x = (opcode & 0xF00) >> 8;
                    uint8_t y = (opcode & 0xF0) >> 4;
                    registers[x] |= registers[y];
                    pc += 2;
                    printf("0x%X: V[%X] |= V[%X]; V[%X] = %X\n", opcode, x, y, x, registers[x]);
                    break;
                }

                // 8xy2: Set Vx = Vx & Vy
                case 0x2:
                {
                    uint8_t x = (opcode & 0xF00) >> 8;
                    uint8_t y = (opcode & 0xF0) >> 4;
                    registers[x] = registers[x] & registers[y];
                    printf("0x%X: V[%X] &= V[%X]; V[%X] = %X\n", opcode, x, y, x, registers[x]);
                    pc += 2;
                    break;
                }

                // 8xy3: Set Vx = Vx XOR Vy
                case 0x3:
                {
                    uint8_t x = (opcode & 0xF00) >> 8;
                    uint8_t y = (opcode & 0xF0) >> 4;
                    registers[x] ^= registers[y];
                    printf("0x%X: V[%X] ^= V[%X]; V[%X] = %X\n", opcode, x, y, x, registers[x]);
                    pc += 2;
                    break;
                }

                // TODO: Check overflow math on 8xy4 and 8xy5
                // 8xy4: Vx += Vy; V[F] = 1 if carry
                case 0x4:
                {
                    uint8_t x = (opcode & 0xF00) >> 8;
                    uint8_t y = (opcode & 0xF0) >> 4;

                    if ((int)registers[x] + (int)registers[y] < 256) registers[0xF] = 0;
                    else registers[0xF] = 1;
                    registers[x] += registers[y];
                    pc += 2;
                    printf("0x%X: V[%X] += V[%X]; V[%X] = %X\n", opcode, x, y, x, registers[x]);
                    break;
                }

                // 8xy5: Set Vx = Vx - Vy, set VF = NOT borrow
                case 0x5:
                {
                    uint8_t x = (opcode & 0xF00) >> 8;
                    uint8_t y = (opcode & 0xF0) >> 4;

                    if (registers[x] < registers[y]) registers[0xF] = 0;
                    else registers[0xF] = 1;
                    registers[x] -= registers[y];

                    pc += 2;
                    printf("0x%X: V[%X] -= V[%X]; V[%X] = %X\n", opcode, x, y, x, registers[x]);
                    break;
                }

                // 8X_6: Shifts VX right by one. VF is set to the value of the least significant bit of VX before the shift
                case 0x6:
                {
                    uint8_t x = (opcode & 0xF00) >> 8;
                    registers[0xF] = registers[x] & 0x1;
                    registers[x] = registers[x] >> 1;
                    pc += 2;
                    printf("0x%X: Right shift V[%X] by one; V[F] = %X, V[%X] = %X\n", opcode, x, registers[0xF], x, registers[x]);
                    break;
                }

                // 8XY7: Set Vx = Vy - Vx, set VF = not borrow
                case 0x7:
                {
                    uint8_t x = (opcode & 0xF00) >> 8;
                    uint8_t y = (opcode & 0xF0) >> 4;

                    if (registers[y] < registers[x]) registers[0xF] = 0;
                    else registers[0xF] = 1;
                    registers[x] = registers[y] - registers[x];

                    pc += 2;
                    printf("0x%X: V[%X] = V[%X] - V[%X]; V[%X] = %X\n", opcode, x, y, x, x, registers[x]);
                    break;
                }

                // 8X_E: Shifts VX left by one. VF is set to the value of the most significant bit of VX before the shift
                case 0xE:
                {
                    uint8_t x = (opcode & 0xF00) >> 8;
                    registers[0xF] = registers[x] >> 7;
                    registers[x] = registers[x] << 1;
                    pc += 2;
                    printf("0x%X: Right shift V[%X] by one; V[F] = %X, V[%X] = %X\n", opcode, x, registers[0xF], x, registers[x]);
                    break;
                }

                default:
                {
                    printf("Unknown opcode: 0x%X\n", opcode);
                    pause();
                    break;
                }
            }

            break;
        }

        // 9XY0: Skip next instruction if Vx != Vy
        case 0x9:
        {
            uint8_t x = (opcode & 0xF00) >> 8;
            uint8_t y = (opcode & 0xF0) >> 4;

            if (registers[x] != registers[y])
            {
                pc += 4;
                printf("0x%X: Skipping next instruction; V[%X] != V[%X]\n", opcode, x, y);
            } else {
                pc += 2;
                printf("0x%X: Not skipping next instruction; V[%X] != V[%X]\n", opcode, x, y);
            }
            break;
        }

        // Annn: Sets I = nnn
        case 0xA:
        {
            uint16_t val = opcode & 0xFFF;
            I = val;
            printf("0x%X: Setting I to %X\n", opcode, val);
            pc += 2;
            break;
        }

        //Bnnn: Sets pc to nnn + V0
        case 0xB:
        {
            uint16_t val = opcode & 0xFFF;
            pc = &memory[val + registers[0]];
            printf("0x%X: Setting pc to %X", pc - memory);
            break;
        }

        // Cxkk: Set Vx to a random byte AND kk
        case 0xC:
        {
            uint8_t x = (opcode & 0xF00) >> 8;
            uint8_t val = opcode & 0xFF;
            registers[x] = (rand() % (0xFF + 1)) & val;
            printf("0x%X: Setting V[%X] to a random byte anded with %X; val = %X\n", opcode, x, val, registers[x]);
            pc += 2;
            break;
        }

        /*
         * Dxyn - DRW Vx, Vy, nibble
         * Displays n-byte sprite starting at memory location I at (Vx, Vy), set Vf true if collision occured
         *
         * Each row of 8 pixels is read as bit-coded starting from memory location I;
         * I value doesn't change after the execution of this instruction.
         * VF is set to 1 if any screen pixels are flipped from set to unset
         * when the sprite is drawn, and to 0 if that doesn't happen.
         */
        case 0xD:
        {
            uint8_t x = (opcode & 0xF00) >> 8;
            uint8_t y = (opcode & 0xF0) >> 4;
            uint8_t n = opcode & 0xF;

            uint8_t xPos = registers[x];
            uint8_t yPos = registers[y];
            uint8_t lineSprite;

            registers[0xF] = 0;
            for (uint8_t yLine = 0; yLine < n; yLine++)
            {
                lineSprite = memory[I + yLine];
                for (uint8_t xLine = 0; xLine < 8; xLine++)
                {
                    if ((lineSprite & (0x80 >> xLine)) != 0)
                    {
                        if (display[((yPos + yLine) * 64) + (xPos + xLine)] == 1)
                        {
                            registers[0xF] = 1;
                        }

                        display[((yPos + yLine) * 64) + (xPos + xLine)] ^= 1;
                    }
                }
            }
            drawFlag = true;
            pc += 2;
            printf("0x%X: Drawing sprite at (%u, %u) from I\n", opcode, xPos, yPos);
            break;
        }

        case 0xE:
        {
            switch (opcode & 0xFF)
            {
                // EX9E: Skip next instruction if key with the value of Vx is pressed
                case 0x9E:
                {
                    uint8_t x = (opcode & 0x0F00) >> 8;

                    if (key[registers[x]] != 1)
                    {
                        pc += 2;
                        printf("0x%X: Not skipping next instruction; %X was not pressed\n", opcode, x, registers[x]);
                    } else {
                        pc += 4;
                        printf("0x%X: Skipping next instruction; %X was pressed\n", opcode, x, registers[x]);
                    }
                    break;
                }

                // ExA1: Skips next instruction if key in Vx is not pressed
                case 0xA1:
                {
                    uint8_t x = (opcode & 0x0F00) >> 8;

                    if (key[registers[x]] != 1)
                    {
                        pc += 4;
                        printf("0x%X: Skipping next instruction; %X was not pressed\n", opcode, x, registers[x]);
                    } else {
                        pc += 2;
                        printf("0x%X: Not skipping next instruction; %X was pressed\n", opcode, x, registers[x]);
                    }
                    break;
                }

                default:
                {
                    printf("Unknown opcode: %X\n", opcode);
                    pause();
                    break;
                }
            }

            break;
        }

        case 0xF:
        {
            switch (opcode & 0xFF)
            {
                // FX07: Set Vx = delay timer
                case 0x07:
                {
                    uint8_t x = (opcode & 0xF00) >> 8;
                    registers[x] = delayTimer;
                    printf("0x%X: Setting V[%X] to delay timer = %X\n", opcode, x, delayTimer);
                    pc += 2;
                    break;
                }

                // FX0A: Wait for a key press, store the value of the key in Vx
                case 0x0A:
                {
                    uint8_t x = (opcode & 0xF00) >> 8;
                    for (int i = 0; i < 16; i++)
                    {
                        if (key[i] == 1)
                        {
                            registers[x] = i;
                            pc += 2;
                        }
                    }
                    break;
                }

                // FX15: Set delay timer = Vx
                case 0x15:
                {
                    uint8_t x = (opcode & 0xF00) >> 8;
                    delayTimer = registers[x];
                    printf("0x%X: Setting delay timer to V[%X] = %X\n", opcode, x, delayTimer);
                    pc += 2;
                    break;
                }

                // FX18: Set sound timer = Vx
                case 0x18:
                {
                    uint8_t x = (opcode & 0xF00) >> 8;
                    soundTimer = registers[x];
                    printf("0x%X: Setting sound timer to V[%X] = %X\n", opcode, x, delayTimer);
                    pc += 2;
                    break;
                }

                // FX1E: Set I = I + Vx
                case 0x1E:
                {
                    uint8_t x = (opcode & 0xF00) >> 8;
                    I += registers[x];
                    printf("0x%X: I+= V[%X]; I = %X", opcode, x, I);
                    pc += 2;
                    break;
                }

                // FX29: The value of I is set to the location for the hexadecimal sprite corresponding to the value of Vx
                case 0x29:
                {
                    uint8_t x = (opcode & 0xF00) >> 8;
                    uint8_t val = registers[x];
                    I = 5 * val;
                    pc += 2;
                    printf("0x%X: Setting I to the location of %X; I = %X\n", opcode, val, I);
                    break;
                }

                // FX33: Store BCD representation of Vx in I[hundreds], I+1[tens], I+2[ones]
                case 0x33:
                {
                    uint8_t x = (opcode & 0xF00) >> 8;
                    uint16_t val = registers[x];

                    uint16_t hundreds = (val - (val % 100));
                    uint16_t tens = (val - hundreds - ((val - hundreds) % 10));
                    uint16_t ones = val - hundreds - tens;

                    memory[I] = hundreds / 100;
                    memory[I+1] = tens / 10;
                    memory[I+2] = ones;
                    printf("0x%X: Storing bcd of %u at I [hundreds:%u, tens:%u, ones:%u]\n", opcode, val, hundreds / 100, tens / 10, ones);
                    pc += 2;
                    break;
                }

                // FX55: Store registers V0 through Vx in memory starting at location I
                case 0x55:
                {
                    uint8_t x = (opcode & 0xF00) >> 8;

                    for (int i = 0; i <= x; i++) memory[I + i] = registers[i];
                    pc += 2;
                    printf("0x%X: Writing to memory at I from V[0-%X]\n", opcode, x);
                    break;
                }

                // FX65: Write to registers V0 through Vx from memory starting at I
                case 0x65:
                {
                    uint8_t x = (opcode & 0xF00) >> 8;
                    for (int i = 0; i <= x; i++) registers[i] = memory[I + i];
                    printf("0x%X: Writing to registers 0 through %X from I\n", opcode, x);
                    pc += 2;
                    break;
                }

                default:
                {
                    std::cerr << "Unknown opcode: 0x" << std::hex << opcode << std::endl;
                    pause();
                }
            }

            break;
        }

        default:
        {
            std::cerr << "Unknown opcode: 0x" << std::hex << opcode << std::endl;
            pause();
        }
    }

    if (delayTimer > 0) delayTimer--;
    if (soundTimer > 0) soundTimer--;
}
