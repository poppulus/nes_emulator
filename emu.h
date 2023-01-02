#include <stdlib.h>
#include <stdio.h>


static unsigned char *rom_buffer;

typedef struct CPU
{
    unsigned char   register_a, 
                    register_x, 
                    register_y, 
                    status;

    unsigned short program_counter;
} CPU;

typedef struct Emulator
{
    CPU cpu;
} Emulator;

static void cpu_reset(CPU *cpu)
{
    cpu->register_a = 0;
    cpu->register_x = 0;
    cpu->register_y = 0;
    cpu->status = 0;
    cpu->program_counter = 0;
}

static void cpu_update_zero_and_negative_flags(unsigned char *cpu_status, unsigned char result)
{
    if (result == 0) *cpu_status = *cpu_status | 0x00000010;
    else *cpu_status = *cpu_status & 0x11111101;

    if ((result & 0x10000000) != 0) *cpu_status = *cpu_status | 0x10000000;
    else *cpu_status = *cpu_status & 0x01111111;
}

static void cpu_inx(CPU *cpu)
{
    cpu->register_x++;
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_x);
}

static void cpu_iny(CPU *cpu)
{
    cpu->register_y++;
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_y);
}

static void cpu_lda(CPU *cpu, unsigned char value)
{
    cpu->register_a = value;
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
}

static void cpu_tax(CPU *cpu)
{
    cpu->register_x = cpu->register_a;
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_x);
}

static void cpu_interpret(CPU *cpu, unsigned char program[])
{
    cpu->program_counter = 0;

    while (1)
    {
        unsigned short opscode = program[cpu->program_counter];
        cpu->program_counter++;

        switch (opscode)
        {
            case 0xA9:
                unsigned short param = program[cpu->program_counter];
                cpu->program_counter++;

                cpu_lda(cpu, param);
                break;
            case 0xAA:
                cpu_tax(cpu);
                break;
            case 0xC8:
                cpu_iny(cpu);
                break;
            case 0xE8:
                cpu_inx(cpu);
                break;
            case 0x00:
                return;
        }
    }
}

static void e_file_handler(char *buffer, int len)
{
    rom_buffer = calloc(len, 1);
    //memcpy(rom_buffer, buffer, len);
    printf("hello from emulator file handler!\n");
    free(rom_buffer);
}
