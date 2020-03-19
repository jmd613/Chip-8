#pragma once

#include <string>
#include <stdint.h>

class Chip8
{
public:
    Chip8(void);
    void Init(void);
    void Update(void);
    bool LoadRom(const char* path);

    bool drawFlag;
    uint8_t display[2048];
    uint8_t key[16];
private:
    uint8_t memory[4096];
    uint8_t registers[16];
    uint8_t* stack[16];

    uint16_t I;
    uint8_t *pc;
    uint8_t **sp;

    uint8_t soundTimer;
    uint8_t delayTimer;
};