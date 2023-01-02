#include <stdlib.h>
#include <stdio.h>


static unsigned char *rom_buffer;

typedef struct Emulator
{
    
} Emulator;

static void e_file_handler(char *buffer, int len)
{
    rom_buffer = calloc(len, 1);
    //memcpy(rom_buffer, buffer, len);
    printf("hello from emulator file handler!\n");
    free(rom_buffer);
}
