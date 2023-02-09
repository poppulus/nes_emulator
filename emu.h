#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define Boolean             unsigned char
#define bool                Boolean
#define true                1
#define false               0

#define PRG_ROM_PAGE_SIZE   0x4000
#define CHR_ROM_PAGE_SIZE   0x2000

#define STACK_RESET         0x01FF
#define RAM                 0x0000
#define RAM_MIRRORS_END     0x1FFF
#define PPU_REGISTERS       0x2000
#define PPU_REGISTERS_END   0x3FFF

#define FRAME_WIDTH         256
#define FRAME_HEIGHT        240

enum Mirroring
{
    VERTICAL,
    HORIZONTAL,
    FOUR_SCREEN
};

enum ProcessorStatus
{
    Carry_Flag              = 0b00000001,
    Zero_Flag               = 0b00000010,
    Interrupt_Disable_Flag  = 0b00000100,
    Decimal_Mode_Flag       = 0b00001000,
    Break_Command_Flag      = 0b00010000,
    Unused_Flag             = 0b00100000,
    Overflow_Flag           = 0b01000000,
    Negative_Flag           = 0b10000000
};

enum AddressingMode
{
    Accumulator,
    Immediate,
    Zero_Page,
    Zero_Page_X,
    Zero_Page_Y,
    Absolute,
    Absolute_X,
    Absolute_Y,
    Indirect,
    Indirect_X,
    Indirect_Y,
    None_Addressing
};

enum ControlRegister
{
    NAMETABLE1                  = 0b00000001,
    NAMETABLE2                  = 0b00000010,
    VRAM_ADD_INCREMENT          = 0b00000100,
    SPRITE_PATTERN_ADDR         = 0b00001000,
    BACKGROUND_PATTERN_ADDR     = 0b00010000,
    SPRITE_SIZE                 = 0b00100000,
    MASTER_SLAVE_SELECT         = 0b01000000,
    GENERATE_NMI                = 0b10000000,
};

enum MaskRegister
{
    GREYSCALE           = 0b00000001,
    BACKGROUND_LEFTMOST = 0b00000010,
    SPRITES_LEFTMOST    = 0b00000100,
    BACKGROUND_SHOW     = 0b00001000,
    SPRITES_SHOW        = 0b00010000,
    EMPHASIZE_RED       = 0b00100000,
    EMPHASIZE_GREEN     = 0b01000000,
    EMPHASIZE_BLUE      = 0b10000000
};

enum StatusRegister
{
    PPU_BUS_1       = 0b00000001,
    PPU_BUS_2       = 0b00000010,
    PPU_BUS_3       = 0b00000100,
    PPU_BUS_4       = 0b00001000,
    PPU_BUS_5       = 0b00010000,
    SPRITE_OVERFLOW = 0b00100000,
    SPRITE_0_HIT    = 0b01000000,
    VERTICAL_BLANK  = 0b10000000,
};
                                // 64 * 3 RGB format
static unsigned char SYSTEM_PALETTE[192] = {
    0x80, 0x80, 0x80, 0x00, 0x3D, 0xA6, 0x00, 0x12, 0xB0, 0x44, 0x00, 0x96, 0xA1, 0x00, 0x5E,
    0xC7, 0x00, 0x28, 0xBA, 0x06, 0x00, 0x8C, 0x17, 0x00, 0x5C, 0x2F, 0x00, 0x10, 0x45, 0x00,
    0x05, 0x4A, 0x00, 0x00, 0x47, 0x2E, 0x00, 0x41, 0x66, 0x00, 0x00, 0x00, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0xC7, 0xC7, 0xC7, 0x00, 0x77, 0xFF, 0x21, 0x55, 0xFF, 0x82, 0x37, 0xFA,
    0xEB, 0x2F, 0xB5, 0xFF, 0x29, 0x50, 0xFF, 0x22, 0x00, 0xD6, 0x32, 0x00, 0xC4, 0x62, 0x00,
    0x35, 0x80, 0x00, 0x05, 0x8F, 0x00, 0x00, 0x8A, 0x55, 0x00, 0x99, 0xCC, 0x21, 0x21, 0x21,
    0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0xFF, 0xFF, 0xFF, 0x0F, 0xD7, 0xFF, 0x69, 0xA2, 0xFF,
    0xD4, 0x80, 0xFF, 0xFF, 0x45, 0xF3, 0xFF, 0x61, 0x8B, 0xFF, 0x88, 0x33, 0xFF, 0x9C, 0x12,
    0xFA, 0xBC, 0x20, 0x9F, 0xE3, 0x0E, 0x2B, 0xF0, 0x35, 0x0C, 0xF0, 0xA4, 0x05, 0xFB, 0xFF,
    0x5E, 0x5E, 0x5E, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0xFF, 0xFF, 0xFF, 0xA6, 0xFC, 0xFF,
    0xB3, 0xEC, 0xFF, 0xDA, 0xAB, 0xEB, 0xFF, 0xA8, 0xF9, 0xFF, 0xAB, 0xB3, 0xFF, 0xD2, 0xB0,
    0xFF, 0xEF, 0xA6, 0xFF, 0xF7, 0x9C, 0xD7, 0xE8, 0x95, 0xA6, 0xED, 0xAF, 0xA2, 0xF2, 0xDA,
    0x99, 0xFF, 0xFC, 0xDD, 0xDD, 0xDD, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11
};

typedef struct Frame
{
    unsigned char data[FRAME_WIDTH * FRAME_HEIGHT * 3];
    int data_len;
} Frame;

typedef struct AddrRegister
{
    // high byte first, low second
    unsigned char value[2];
    bool hi_ptr;
} AddrRegister;

typedef struct PPU
{
    unsigned char   *chr_rom,
                    palette_table[32],
                    vram[2048],
                    oam_data[256],
                    oam_addr,
                    internal_data_buf;

    int cycles;

    bool nmi_interrupt:1;

    unsigned short scanline;

    enum Mirroring          mirroring;
    enum ControlRegister    ctrl;
    enum MaskRegister       mask;
    enum StatusRegister     status;

    AddrRegister addr;
} PPU;

typedef struct Rom
{
    unsigned char   *prg_rom,
                    *chr_rom,
                    mapper;

    unsigned short prg_len, chr_len;

    enum Mirroring screen_mirroring;
} Rom;

typedef struct Bus
{
    unsigned char   cpu_vram[2048],
                    cycles;
    Rom rom;
    PPU ppu;
} Bus;

typedef struct CPU
{
    unsigned char   register_a, 
                    register_x, 
                    register_y, 
                    status,
                    stack_pointer;

    unsigned char   memory[0xFFFF];

    unsigned short program_counter;

    unsigned int cycles;

    Bus bus;
} CPU;

typedef struct Emulator
{
    CPU cpu;
} Emulator;


static void frame_new(Frame *frame)
{
    frame->data_len = FRAME_WIDTH * FRAME_HEIGHT * 3;
    for (int i = 0; i < frame->data_len; i++)
        frame->data[i] = 0;
}

static void frame_set_pixel(Frame *frame, short x, short y, unsigned char rgb[3])
{
    int base = y * 3 * FRAME_WIDTH + x * 3;

    if (base + 2 < frame->data_len)
    {
        frame->data[base] = rgb[0];
        frame->data[base + 1] = rgb[1];
        frame->data[base + 2] = rgb[2];
    }
}

static Frame *frame_show_tile(unsigned char chr_rom[], int bank, int tile_n)
{
    Frame *frame = malloc(sizeof(Frame));
    frame_new(frame);

    printf("creating new frame ...\n");

    int f_bank = 0; //bank * 0x1000;

    unsigned char *tile;

    for (int b = 0; b < 2; b++)
    {
        for (int i = 0; i < 256; i++)
        {
            tile = &chr_rom[(b * 0x1000) + (i * 16)];

            unsigned char   tile_x = i % 32,
                            tile_y = i / 32;

            for (int y = 0; y < 8; y++)
            {
                unsigned char   upper = tile[y],
                                lower = tile[y + 8];

                for (int x = 7; x >= 0; x--)
                {
                    unsigned char value = (1 & upper) << 1 | (1 & lower);

                    upper >>= 1;
                    lower >>= 1;

                    unsigned char *rgb;

                    switch (value)
                    {
                        case 0:
                            rgb = &SYSTEM_PALETTE[0x01];
                            break;
                        case 1:
                            rgb = &SYSTEM_PALETTE[0x23];
                            break;
                        case 2:
                            rgb = &SYSTEM_PALETTE[0x27];
                            break;
                        case 3:
                            rgb = &SYSTEM_PALETTE[0x30];
                            break;
                        default: 
                            break;
                    }
                    
                    frame_set_pixel(frame, tile_x * 8 + x, tile_y * 8 + y, rgb);
                }
            }
        }
    }

    printf("frame created!\n");
    
    return frame;
}

static void ppu_render(PPU *ppu, Frame *frame)
{
    unsigned char bank = ppu->ctrl & BACKGROUND_PATTERN_ADDR;

    for (int i = 0; i < 0x03c0; i++)
    {
        unsigned char   tile = ppu->vram[i],
                        tile_x = i % 32,
                        tile_y = i / 32,
                        *b_tile = &ppu->chr_rom[bank + tile * 16];

            for (int y = 0; y < 8; y++)
            {
                unsigned char   upper = b_tile[y],
                                lower = b_tile[y + 8];

                for (int x = 7; x >= 0; x--)
                {
                    unsigned char value = (1 & upper) << 1 | (1 & lower);

                    upper >>= 1;
                    lower >>= 1;

                    unsigned char *rgb;

                    switch (value)
                    {
                        case 0:
                            rgb = &SYSTEM_PALETTE[0x01];
                            break;
                        case 1:
                            rgb = &SYSTEM_PALETTE[0x23];
                            break;
                        case 2:
                            rgb = &SYSTEM_PALETTE[0x27];
                            break;
                        case 3:
                            rgb = &SYSTEM_PALETTE[0x30];
                            break;
                        default: 
                            break;
                    }
                    
                    frame_set_pixel(frame, tile_x * 8 + x, tile_y * 8 + y, rgb);
                }
            }
    }
}

static unsigned char vram_addr_increment(enum ControlRegister ctrl)
{
    if (ctrl & VRAM_ADD_INCREMENT)  return 1;
    else                            return 32;
}

static void addr_reset(AddrRegister *addr)
{
    addr->value[0] = 0;
    addr->value[1] = 0;
    addr->hi_ptr = true;
}

static void addr_set(AddrRegister *addr, unsigned short data)
{
    addr->value[0] = (unsigned char)(data >> 8);
    addr->value[1] = (unsigned char)(data & 0xFF);
}

static unsigned short addr_get(AddrRegister *addr)
{
    return (unsigned short)(addr->value[0] << 8) | (unsigned short)addr->value[1];
}

static void addr_update(AddrRegister *addr, unsigned char data)
{
    if (addr->hi_ptr)   addr->value[0] = data;
    else                addr->value[1] = data;
    
    if (addr_get(addr) > 0x3FFF)
        addr_set(addr, addr_get(addr) & 0b11111111111111);
    
    addr->hi_ptr = !addr->hi_ptr;
}

static void addr_reset_latch(AddrRegister *addr)
{
    addr->hi_ptr = true;
}

static void addr_increment(AddrRegister *addr, unsigned char inc)
{
    unsigned char lo = addr->value[1];

    addr->value[1] = (addr->value[1] + inc) % 0x100;

    if (lo > addr->value[1])
        addr->value[0] = (addr->value[0] + 1) % 0x100;

    if (addr_get(addr) > 0x3FFF)
        addr_set(addr, addr_get(addr) & 0b11111111111111);
}

static bool ppu_tick(PPU *ppu, unsigned char cycles)
{
    ppu->cycles += cycles;

    if (ppu->cycles > 341)
    {
        ppu->cycles -= 341;
        ppu->scanline += 1;

        if (ppu->scanline == 241)
        {
            if (ppu->ctrl & GENERATE_NMI)
                ppu->status |= VERTICAL_BLANK;
        }

        if (ppu->scanline >= 262)
        {
            ppu->scanline = 0;
            ppu->status &= 0b01111111;
            return true;
        }
    }

    return false;
}

static void ppu_load(PPU *ppu, unsigned char chr_rom[], enum Mirroring mirroring)
{
    ppu->chr_rom = chr_rom;
    ppu->mirroring = mirroring;

    ppu->ctrl = 0;
    ppu->cycles = 0;
    ppu->mask = 0;
    ppu->scanline = 0;
    ppu->status = 0b10100000;

    ppu->nmi_interrupt = false;

    for (int i = 0; i < 2048; i++)
        ppu->vram[i] = 0;
    
    for (int i = 0; i < 256; i++)
        ppu->oam_data[i] = 0;

    for (int i = 0; i < 32; i++)
        ppu->palette_table[i] = 0;
}

static void ppu_write_to_ctrl(PPU *ppu, unsigned char value)
{
    unsigned char before_nmi_status = ppu->ctrl & GENERATE_NMI;

    ppu->ctrl |= value;

    if (!before_nmi_status 
    && ppu->ctrl & GENERATE_NMI 
    && ppu->status & VERTICAL_BLANK)
    {
        ppu->nmi_interrupt = true;
    }
}

static void ppu_write_to_ppu_addr(PPU *ppu, unsigned char data)
{
    addr_update(&ppu->addr, data);
}

static void ppu_increment_vram_addr(PPU *ppu)
{
    addr_increment(&ppu->addr, vram_addr_increment(ppu->ctrl));
}

static unsigned short ppu_mirror_vram_addr(PPU *ppu, unsigned short addr)
{
    unsigned short  mirrored_vram = addr & 0b10111111111111,
                    vram_index = mirrored_vram - 0x2000,
                    name_table = vram_index / 0x0400;

    unsigned short value = vram_index;

    switch (ppu->mirroring)
    {
        case VERTICAL:
            if (name_table == 2 || name_table == 3)
                value = vram_index - 0x0800;
            break;
        case HORIZONTAL:
            if (name_table == 1 || name_table == 2) 
                value = vram_index - 0x0400;
            else if (name_table == 3)
                value = vram_index - 0x0800;
            break;
        case FOUR_SCREEN:
            break;
        default:
            break;
    }

    return value;
}

static unsigned char ppu_read_data(PPU *ppu)
{
    unsigned short addr = addr_get(&ppu->addr);
    unsigned char data = 0;

    ppu_increment_vram_addr(ppu);

    switch (addr)
    {
        case 0x0000 ... 0x1FFF:
            data = ppu->internal_data_buf;
            ppu->internal_data_buf = ppu->chr_rom[addr];
            break;
        case 0x2000 ... 0x2FFF:
            data = ppu->internal_data_buf;
            ppu->internal_data_buf = ppu->vram[ppu_mirror_vram_addr(ppu, addr)];
            break;
        case 0x3000 ... 0x3EFF:
            break;
        case 0x3f00 ... 0x3FFF:
            data = ppu->palette_table[addr - 0x3F00];
            break;
        default:
            break;
    }

    return data;
}

static void ppu_write_to_data(PPU *ppu, unsigned char data)
{
    unsigned short addr = addr_get(&ppu->addr);

    ppu_increment_vram_addr(ppu);

    switch (addr)
    {
        case 0x0000 ... 0x1FFF:
            ppu->internal_data_buf = data;
            ppu->chr_rom[addr] = ppu->internal_data_buf;
            break;
        case 0x2000 ... 0x2FFF:
            ppu->internal_data_buf = data;
            ppu->vram[ppu_mirror_vram_addr(ppu, addr)] = ppu->internal_data_buf;
            break;
        case 0x3000 ... 0x3EFF:
            break;
        case 0x3f00 ... 0x3FFF:
            ppu->palette_table[addr - 0x3F00] = data;
            break;
        default:
            break;
    }
}

static bool rom_load(Rom *rom, unsigned char data[])
{
    if (data[0] != 'N' && data[1] != 'E' 
    && data[2] != 'S'&& data[3] != 0x1A)
    {
        printf("File is not in iNES file format!\n");
        return false;
    }

    unsigned char ines_ver = (data[7] >> 2) & 0b11;

    if (ines_ver != 0)
    {
        printf("iNES2.0 format not supported!\n");
        return false;
    }

    rom->mapper = (data[7] & 0b11110000) | (data[6] >> 4);

    if (data[6] & 0b1000)   rom->screen_mirroring = FOUR_SCREEN;
    else if (data[6] & 0b1) rom->screen_mirroring = VERTICAL;
    else                    rom->screen_mirroring = HORIZONTAL;

    rom->prg_len = data[4] * PRG_ROM_PAGE_SIZE;
    rom->chr_len = data[5] * CHR_ROM_PAGE_SIZE;

    printf("nr kb prg banks:%d\nnr kb chr banks:%d\n", data[4], data[5]);

    printf("prg size: %d chr size: %d\n", rom->prg_len, rom->chr_len);

                                    // check if trainer is set, else skip
    unsigned short  prg_rom_start = (data[6] & 0b100) ? 512 + 16 : 16,
                    chr_rom_start = prg_rom_start + rom->prg_len;

    printf("prg start: %d chr start: %d\n", prg_rom_start, chr_rom_start);

    rom->prg_rom = malloc(rom->prg_len);
    rom->chr_rom = malloc(rom->chr_len);

    int i = 0;

    for (; i < rom->prg_len; i++)
        rom->prg_rom[i] = data[prg_rom_start + i];
    
    for (i = 0; i < rom->chr_len; i++)
        rom->chr_rom[i] = data[chr_rom_start + i];

    printf("Rom loaded successfully!\n");

    return true;
}

static unsigned char rom_read_prg_rom(Bus *bus, unsigned short addr)
{
    addr -= 0x8000;

    if (bus->rom.prg_len == 0x4000 && addr >= 0x4000)
        addr %= 0x4000;
    
    return bus->rom.prg_rom[addr];
}

static void bus_tick(Bus *bus, unsigned char cycles)
{
    bus->cycles += cycles;

    unsigned char nmi_before = bus->ppu.nmi_interrupt;

    ppu_tick(&bus->ppu, cycles * 3);

    unsigned char nmi_after = bus->ppu.nmi_interrupt;

    if (!nmi_before && nmi_after)
    {
        
    }
}

static void bus_free_rom(Rom *rom)
{
    printf("free chr rom\n");
    free(rom->chr_rom);
    printf("free prg rom\n");
    free(rom->prg_rom);
}

static unsigned char bus_mem_read(Bus *bus, unsigned short addr)
{
    unsigned char mem_addr = (unsigned char)addr >> 8;

    switch (addr)
    {
        case RAM ... RAM_MIRRORS_END:
            mem_addr = bus->cpu_vram[addr & 0b0000011111111111];
            break;
        case PPU_REGISTERS:
        case 0x2001:
        case 0x2003:
        case 0x2005:
        case 0x2006:
        case 0x4014:
            printf("attempt to read from write-only PPU address:%X\n", addr);
            break;
        case 0x2002:
            mem_addr = bus->ppu.status;
            bus->ppu.status &= 0b01111111;
            bus->ppu.addr.hi_ptr = true;
            break;
        case 0x2004:
            mem_addr = bus->ppu.oam_data[bus->ppu.oam_addr];
            break;
        case 0x2007:
            mem_addr = ppu_read_data(&bus->ppu);
            break;
        case 0x2008 ... PPU_REGISTERS_END:
            mem_addr = bus_mem_read(bus, addr & 0b0010000000000111);
            break;
        case 0x8000 ... 0xFFFF:
            mem_addr = rom_read_prg_rom(bus, addr);
            break;
        default: 
            printf("ignoring memory access:%X\n", addr);
            break;
    }

    return mem_addr;
}

static void bus_mem_write(Bus *bus, unsigned short addr, unsigned char data)
{
    switch (addr)
    {
        case RAM ... RAM_MIRRORS_END:
            bus->cpu_vram[addr & 0b11111111111] = data;
            break;
        case PPU_REGISTERS:
            ppu_write_to_ctrl(&bus->ppu, data);
            break;
        case 0x2001:
            bus->ppu.mask = data;
            break;
        case 0x2003:
            bus->ppu.oam_addr = data;
            break;
        case 0x2004:
            bus->ppu.oam_data[bus->ppu.oam_addr] = data;
            bus->ppu.oam_addr++;
            break;
        case 0x2006:
            ppu_write_to_ppu_addr(&bus->ppu, data);
            break;
        case 0x2007:
            ppu_write_to_data(&bus->ppu, data);
            break;
        case 0x4014:
            unsigned char lo = 0x00;
            unsigned short cpu_read_addr = (unsigned short)data << 8 | (unsigned short)lo;

            for (int i = 0; i < 256; i++)
            {
                bus_mem_write(bus, 0x2004, bus_mem_read(bus, cpu_read_addr));
                lo++;
            }
            break;
        case 0x2008 ... PPU_REGISTERS_END:
            bus_mem_write(bus, addr & 0b0010000000000111, data);
            break;
        case 0x8000 ... 0xFFFF:
            printf("Attempt to write to cartridge ROM space!\n");
            break;
        default: 
            printf("ignoring memory write-access:%X\n", addr);
            break;
    }
}

static unsigned char cpu_mem_read(Bus *bus, unsigned short addr)
{
    return bus_mem_read(bus, addr);
}

static void cpu_mem_write(Bus *bus, unsigned short addr, unsigned char data)
{
    bus_mem_write(bus, addr, data);
}

static unsigned short cpu_mem_read_u16(CPU *cpu, unsigned short pos)
{
    unsigned short  lo = (unsigned short)cpu_mem_read(&cpu->bus, pos), 
                    hi = (unsigned short)cpu_mem_read(&cpu->bus, pos + 1);

    return (hi << 8) | (unsigned short)lo;
}

static void cpu_mem_write_u16(CPU *cpu, unsigned short pos, unsigned short data)
{
    unsigned char   hi = data >> 8, 
                    lo = data & 0xFF;

    cpu_mem_write(&cpu->bus, pos, lo);
    cpu_mem_write(&cpu->bus, pos + 1, hi);
}

static void cpu_update_zero_and_negative_flags(unsigned char *cpu_status, unsigned char result)
{
    if (result == 0) *cpu_status = *cpu_status | Zero_Flag;
    else *cpu_status = *cpu_status & 0b11111101;

    if ((result & 0b10000000) != 0) *cpu_status = *cpu_status | Negative_Flag;
    else *cpu_status = *cpu_status & 0b01111111;
}

static void cpu_clc(unsigned char *cpu_status)
{
    *cpu_status = *cpu_status & 0b11111110;
}

static void cpu_sec(unsigned char *status)
{
    *status = *status | Carry_Flag;
}

static void cpu_cld(unsigned char *status)
{
    *status = *status & 0b11110111;
}

static void cpu_sed(unsigned char *status)
{
    *status = *status | Decimal_Mode_Flag;
}

static void cpu_cli(unsigned char *status)
{
    *status = *status & 0b11111011;
}

static void cpu_sei(unsigned char *status)
{
    *status = *status | Interrupt_Disable_Flag;
}

static void cpu_zero_clear(unsigned char *status)
{
    *status = *status & 0b11111101;
}

static void cpu_zero_set(unsigned char *status)
{
    *status = *status | Zero_Flag;
}

static void cpu_clv(unsigned char *status)
{
    *status = *status & 0b10111111;
}

static void rom_init(Rom *rom)
{
    rom->screen_mirroring = 0;
    rom->mapper = 0;
    rom->chr_len = 0;
    rom->prg_len = 0;

    rom->chr_rom = NULL;
    rom->prg_rom = NULL;
}

static void rom_reset(Rom *rom)
{
    if (rom->chr_rom != NULL)
    {
        free(rom->chr_rom);
        rom->chr_rom = NULL;
    }
    if (rom->prg_rom != NULL)
    {
        free(rom->prg_rom);
        rom->prg_rom = NULL;
    }

    rom->screen_mirroring = 0;
    rom->mapper = 0;
    rom->chr_len = 0;
    rom->prg_len = 0;
}

static void cpu_interrupt_nmi(CPU *cpu)
{
    cpu_mem_write_u16(cpu, 0x0100 + cpu->stack_pointer - 1, cpu->program_counter);
    cpu->stack_pointer -= 2;

    unsigned char flags = cpu->status;

    flags &= 0b11101111;
    flags |= Unused_Flag;

    cpu_mem_write(&cpu->bus, 0x0100 + cpu->stack_pointer, flags);
    cpu->stack_pointer -= 1;

    cpu->status |= Interrupt_Disable_Flag;

    bus_tick(&cpu->bus, 2);

    cpu->program_counter = cpu_mem_read_u16(cpu, 0xFFFA);
}

static void cpu_init(CPU *cpu)
{
    int i = 0;

    for (; i < 65535; i++)
        cpu->memory[i] = 0;

    for (i = 0; i < 2048; i++)
        cpu->bus.cpu_vram[i] = 0;

    memcpy(&cpu->memory[0x8000], cpu->bus.rom.prg_rom, cpu->bus.rom.prg_len);

    cpu->register_a = 0;
    cpu->register_x = 0;
    cpu->register_y = 0;
    cpu->status = 0b00100100;
    cpu->stack_pointer = 0xFD;     
    cpu->program_counter = cpu_mem_read_u16(cpu, 0xFFFC); //   cpu_mem_read_u16(cpu, 0xFFFC);
                                   //   ... should be the standard 
                                   //   testing with 0xC000 nestest.rom for now   
    // for testing with nestest.rom                     
    cpu->cycles = 7;
    cpu->bus.cycles = 7;
}

static void cpu_reset(CPU *cpu)
{
    cpu->stack_pointer -= 3;
    cpu->status |= Interrupt_Disable_Flag;
}

static unsigned short cpu_get_operand_address(CPU *cpu, enum AddressingMode mode)
{
    unsigned short  opaddr = 0, 
                    base;
                    
    unsigned char   pos, 
                    lo, 
                    hi, 
                    base_8;

    switch (mode)
    {
        case Immediate:
            opaddr = cpu->program_counter;
            break;
        case Zero_Page:
            opaddr = (unsigned short)cpu_mem_read(&cpu->bus, cpu->program_counter);
            break;
        case Zero_Page_X:
            pos = cpu_mem_read(&cpu->bus, cpu->program_counter);
            opaddr = (unsigned short)((pos + cpu->register_x) % 0x100);
            break;
        case Zero_Page_Y:
            pos = cpu_mem_read(&cpu->bus, cpu->program_counter);
            opaddr = (unsigned short)((pos + cpu->register_y) % 0x100);
            break;
        case Absolute:
            opaddr = cpu_mem_read_u16(cpu, cpu->program_counter);
            break;
        case Absolute_X:
            base = cpu_mem_read_u16(cpu, cpu->program_counter);
            opaddr = (base + (unsigned short)cpu->register_x) % 0x10000;
            break;
        case Absolute_Y:
            base = cpu_mem_read_u16(cpu, cpu->program_counter);
            opaddr = (base + (unsigned short)cpu->register_y) % 0x10000;
            break;
        case Indirect: // only for JMP
            base = cpu_mem_read_u16(cpu, cpu->program_counter);

            if ((base & 0xFF) == 0xFF)
            {
                lo = cpu_mem_read(&cpu->bus, base);
                hi = cpu_mem_read(&cpu->bus, base & 0xFF00);
                opaddr = ((unsigned short)hi << 8) | (unsigned short)lo;
            }
            else 
            {
                lo = cpu_mem_read(&cpu->bus, base);
                hi = cpu_mem_read(&cpu->bus, (base + 1) % 0x10000);
                opaddr = ((unsigned short)hi << 8) | (unsigned short)lo;
            }
            break;
        case Indirect_X:
            base_8 = cpu_mem_read(&cpu->bus, cpu->program_counter);

            unsigned char ptr = (base_8 + cpu->register_x) % 0x100;

            lo = cpu_mem_read(&cpu->bus, (unsigned short)ptr);
            hi = cpu_mem_read(&cpu->bus, (unsigned short)((ptr + 1) % 0x100));

            opaddr = ((unsigned short)hi << 8) | (unsigned short)lo;
            break;
        case Indirect_Y:
            base_8 = cpu_mem_read(&cpu->bus, cpu->program_counter);

            lo = cpu_mem_read(&cpu->bus, (unsigned short)base_8);
            hi = cpu_mem_read(&cpu->bus, (unsigned short)((unsigned char)(base_8 + 1) % 0x100));

            unsigned short deref_base   = ((unsigned short)hi << 8) | (unsigned short)lo;
            unsigned short deref        = (deref_base + (unsigned short)cpu->register_y) % 0x10000;

            opaddr = deref;
            break;
        default: 
        case None_Addressing:
            printf("Mode %d is not supported!\n", opaddr);
            break;
    }

    return opaddr;
}

static void cpu_aac(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    cpu->register_a &= cpu_mem_read(&cpu->bus, addr);

    if (cpu->register_a & 0b10000000) cpu_sec(&cpu->status);
    else cpu_clc(&cpu->status);

    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
}

static void cpu_aax(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    cpu_mem_write(&cpu->bus, addr, cpu->register_a & cpu->register_x);
}

static void cpu_arr(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    cpu->register_a &= cpu_mem_read(&cpu->bus, addr);
    cpu->register_a = (cpu->register_a >> 1) | (cpu->register_a << 7);

    if ((cpu->register_a & 0b00100000) 
    && (cpu->register_a & 0b01000000))
    {
        cpu_sec(&cpu->status);
        cpu_clv(&cpu->status);
    }
    if ((cpu->register_a & 0b00100000) == 0 
    && (cpu->register_a & 0b01000000) == 0)
    {
        cpu_clc(&cpu->status);
        cpu->status = cpu->status & 0b10111111;
    }
    if ((cpu->register_a & 0b00100000) 
    && (cpu->register_a & 0b01000000) == 0)
    {
        cpu->status = cpu->status | Overflow_Flag;
        cpu_clc(&cpu->status);
    }
    if ((cpu->register_a & 0b00100000) == 0 
    && (cpu->register_a & 0b01000000) == 0)
    {
        cpu_sec(&cpu->status);
        cpu->status = cpu->status | Overflow_Flag;
    }

    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
}

static void cpu_asr(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    cpu->register_a &= cpu_mem_read(&cpu->bus, addr);
    cpu->register_a >>= 1;

    if (cpu->register_a & 0b00000001) 
        cpu->status = cpu->status | 0b00000001;
    else 
        cpu->status = cpu->status & 0b11111110;

    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
}

static void cpu_atx(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    cpu->register_a &= cpu_mem_read(&cpu->bus, addr);
    cpu->register_x = cpu->register_a;

    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_x);
}

static void cpu_axa(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    cpu->register_x &= cpu->register_a;
    cpu->register_x &= 0x07;
    cpu_mem_write(&cpu->bus, addr, cpu->register_x);
}

static void cpu_axs(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    cpu->register_x &= cpu->register_a;
    cpu->register_x -= cpu_mem_read(&cpu->bus, addr);

    if (cpu->register_x & Carry_Flag) cpu_sec(&cpu->status);
    else cpu_clc(&cpu->status);

    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_x);
}

static void cpu_dcp(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    cpu_mem_write(&cpu->bus, addr, cpu_mem_read(&cpu->bus, addr) - 1);

    unsigned char mem = cpu_mem_read(&cpu->bus, addr);
    unsigned char result = cpu->register_a - mem;

    if (cpu->register_a >= mem)
        cpu_sec(&cpu->status);
    else 
        cpu_clc(&cpu->status);

    if (cpu->register_a == mem)
        cpu_zero_set(&cpu->status);
    else 
        cpu_zero_clear(&cpu->status);

    if (result & 0b10000000) 
        cpu->status = cpu->status | Negative_Flag;
    else 
        cpu->status = cpu->status & 0b01111111;
}

static void cpu_dop(CPU *cpu, enum AddressingMode mode)
{
    
}

static void cpu_isc(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    cpu_mem_write(&cpu->bus, addr, cpu_mem_read(&cpu->bus, addr) + 1);

    unsigned char arg = cpu_mem_read(&cpu->bus, addr) ^ 0xFF;

    short         sum = cpu->register_a + arg + (cpu->status & Carry_Flag);

    if (~(cpu->register_a ^ arg) & (cpu->register_a ^ sum) & 0x80)
        cpu->status = cpu->status | Overflow_Flag;
    else 
        cpu->status = cpu->status & 0b10111111;

    if (sum > 0xFF) cpu_sec(&cpu->status);
    else cpu_clc(&cpu->status);

    cpu->register_a = sum & 0xFF;

    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
}

static void cpu_kil(CPU *cpu)
{
    
}

static bool cpu_lar(CPU *cpu, enum AddressingMode mode)
{
    unsigned short  addr = cpu_get_operand_address(cpu, mode),
                    base = cpu_get_operand_address(cpu, Absolute);

    bool extra_cycle = false;

    if ((addr >> 8) < (base >> 8))
    {
        cpu->cycles += 1; 
        extra_cycle = true;
    }

    unsigned char and = cpu_mem_read(&cpu->bus, addr) & cpu_mem_read(&cpu->bus, 0x0100 + cpu->stack_pointer);

    cpu->register_a = and;
    cpu->register_x = and;
    cpu_mem_write(&cpu->bus, 0x0100 + cpu->stack_pointer, and);
    
    cpu_update_zero_and_negative_flags(&cpu->status, cpu_mem_read(&cpu->bus, addr));

    return extra_cycle;
}

static bool cpu_lax(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    bool extra_cycle = false;

    switch (mode)
    {
        case Absolute_Y:
            unsigned short base = cpu_get_operand_address(cpu, Absolute);
            if ((addr >> 8) != (base >> 8))
            {
                cpu->cycles += 1;
                extra_cycle = true;
            }
            break;
        case Indirect_Y:
            unsigned char   base_8 = cpu_mem_read(&cpu->bus, cpu->program_counter),
                            lo = cpu_mem_read(&cpu->bus, (unsigned short)base_8),
                            hi = cpu_mem_read(&cpu->bus, (unsigned short)((unsigned char)(base_8 + 1) % 0x100));

            unsigned short deref = (unsigned short)(hi << 8) | (unsigned short)lo;

            if ((addr >> 8) != (deref >> 8))
            {
                cpu->cycles += 1;
                extra_cycle = true;
            }
            break;
        default: 
            break;
    }

    unsigned char mem = cpu_mem_read(&cpu->bus, addr);

    cpu->register_a = mem;
    cpu->register_x = mem;

    cpu_update_zero_and_negative_flags(&cpu->status, mem);

    return extra_cycle;
}

static void cpu_rla(CPU *cpu, enum AddressingMode mode)
{
    unsigned short  addr = cpu_get_operand_address(cpu, mode);

    unsigned char   carry = (cpu->status & Carry_Flag),
                    old_bit = cpu_mem_read(&cpu->bus, addr),
                    shift = cpu_mem_read(&cpu->bus, addr) << 1,
                    rotate = cpu_mem_read(&cpu->bus, addr) >> 7;

    cpu_mem_write(&cpu->bus, addr, (shift | rotate));

    if (carry) 
        cpu_mem_write(&cpu->bus, addr, cpu_mem_read(&cpu->bus, addr) | Carry_Flag);
    else 
        cpu_mem_write(&cpu->bus, addr, cpu_mem_read(&cpu->bus, addr) & 0b11111110);

    if (old_bit & 0b10000000) 
        cpu->status = cpu->status | Carry_Flag;
    else 
        cpu->status = cpu->status & 0b11111110;

    cpu->register_a &= cpu_mem_read(&cpu->bus, addr);

    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
}

static void cpu_rra(CPU *cpu, enum AddressingMode mode)
{
    unsigned short  addr = cpu_get_operand_address(cpu, mode);

    unsigned char   old_bit = cpu_mem_read(&cpu->bus, addr),
                    shift = cpu_mem_read(&cpu->bus, addr) >> 1,
                    rotate = cpu_mem_read(&cpu->bus, addr) << 7;

    cpu_mem_write(&cpu->bus, addr, shift | rotate);

    if (cpu->status & 0b00000001) 
        cpu_mem_write(&cpu->bus, addr, cpu_mem_read(&cpu->bus, addr) | 0b10000000);
    else 
        cpu_mem_write(&cpu->bus, addr, cpu_mem_read(&cpu->bus, addr) & 0b01111111);

    unsigned char   carry = old_bit & 0b00000001,
                    arg = cpu_mem_read(&cpu->bus, addr);

    short           sum = cpu->register_a + arg + carry;

    if (~(cpu->register_a ^ arg) & (cpu->register_a ^ sum) & 0x80)
        cpu->status = cpu->status | Overflow_Flag;
    else 
        cpu->status = cpu->status & 0b10111111;

    if (sum > 0xFF) cpu_sec(&cpu->status);
    else            cpu_clc(&cpu->status);

    cpu->register_a = sum & 0xFF;
    
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
}

static void cpu_slo(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    unsigned char old_bit = cpu_mem_read(&cpu->bus, addr);

    cpu_mem_write(&cpu->bus, addr, cpu_mem_read(&cpu->bus, addr) << 1);

    if ((old_bit & 0b10000000) != 0)
        cpu->status = cpu->status | Carry_Flag;
    else 
        cpu->status = cpu->status & 0b11111110;

    cpu->register_a |= cpu_mem_read(&cpu->bus, addr);

    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
}

static void cpu_sre(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    unsigned char  old_bit = cpu_mem_read(&cpu->bus, addr);

    if ((old_bit & 0b00000001) != 0) 
        cpu->status = cpu->status | Carry_Flag;
    else 
        cpu->status = cpu->status & 0b11111110;

    cpu_mem_write(&cpu->bus, addr, (cpu_mem_read(&cpu->bus, addr) >> 1) & 0b01111111);

    cpu->register_a ^= cpu_mem_read(&cpu->bus, addr);

    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
}

static void cpu_sxa(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    unsigned char hi = (unsigned char)addr;
    cpu_mem_write(&cpu->bus, addr, (cpu->register_x & hi) + 1);
}

static void cpu_sya(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    unsigned char hi = (unsigned char)addr;
    cpu_mem_write(&cpu->bus, addr, (cpu->register_y & hi) + 1);
}

static bool cpu_top(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    bool extra_cycle = true;

    if (mode == Absolute_X)
    {
        unsigned short base = cpu_get_operand_address(cpu, Absolute);
        if ((addr >> 8) != (base >> 8))
        {
            cpu->cycles += 1; 
            extra_cycle = true;
        }
    }

    return extra_cycle;
}

static void cpu_xaa(CPU *cpu, enum AddressingMode mode)
{
    // find documentation
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    cpu->register_a = cpu->register_x;

    unsigned char result = cpu->register_a & cpu_mem_read(&cpu->bus, addr);

    cpu_update_zero_and_negative_flags(&cpu->status, result);
}

static void cpu_xas(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    unsigned char   hi = (unsigned char)addr,
                    mem = cpu_mem_read(&cpu->bus, 0x0100 + cpu->stack_pointer);

    cpu_mem_write(&cpu->bus, 0x0100 + cpu->stack_pointer, cpu->register_x & cpu->register_a);

    unsigned char result = (mem & hi) + 1;

    cpu_mem_write(&cpu->bus, addr, result);
}

static bool cpu_adc(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    bool extra_cycle = false;

    switch (mode)
    {
        case Absolute_X:
        case Absolute_Y:
            unsigned short base = cpu_get_operand_address(cpu, Absolute);
            if ((addr >> 8) != (base >> 8))
            {
                cpu->cycles += 1; 
                extra_cycle = true;
            }
            break;
        case Indirect_Y:
            unsigned char   base_8 = cpu_mem_read(&cpu->bus, cpu->program_counter),
                            lo = cpu_mem_read(&cpu->bus, (unsigned short)base_8),
                            hi = cpu_mem_read(&cpu->bus, (unsigned short)((unsigned char)(base_8 + 1) % 0x100));

            unsigned short deref = (unsigned short)(hi << 8) | (unsigned short)lo;

            if ((addr >> 8) != (deref >> 8))
            {
                cpu->cycles += 1; 
                extra_cycle = true;
            }
            break;
        default: 
            break;
    }

    unsigned char   arg = cpu_mem_read(&cpu->bus, addr);

    short           sum = cpu->register_a + arg + (cpu->status & Carry_Flag);

    if (~(cpu->register_a ^ arg) & (cpu->register_a ^ sum) & 0x80)
        cpu->status = cpu->status | Overflow_Flag;
    else 
        cpu->status = cpu->status & 0b10111111;

    cpu->register_a = sum & 0xFF;

    if (sum > 0xFF) cpu_sec(&cpu->status); //(cpu->register_a >> 8) & 0x01
    else cpu_clc(&cpu->status);

    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);

    return extra_cycle;
}

static bool cpu_sbc(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    bool extra_cycle = false;

    switch (mode)
    {
        case Absolute_X:
        case Absolute_Y:
            unsigned short base = cpu_get_operand_address(cpu, Absolute);
            if ((addr >> 8) != (base >> 8))
            {
                cpu->cycles += 1;
                extra_cycle = true;
            }
            break;
        case Indirect_Y:
            unsigned char   base_8 = cpu_mem_read(&cpu->bus, cpu->program_counter),
                            lo = cpu_mem_read(&cpu->bus, (unsigned short)base_8),
                            hi = cpu_mem_read(&cpu->bus, (unsigned short)((unsigned char)(base_8 + 1) % 0x100));

            unsigned short deref = (unsigned short)(hi << 8) | (unsigned short)lo;

            if ((addr >> 8) != (deref >> 8))
            {
                cpu->cycles += 1;
                extra_cycle = true;
            }
            break;
        default: 
            break;
    }

    unsigned char   arg = cpu_mem_read(&cpu->bus, addr) ^ 0xFF;

    short           sum = cpu->register_a + arg + (cpu->status & Carry_Flag);

    if (~(cpu->register_a ^ arg) & (cpu->register_a ^ sum) & 0x80)
        cpu->status = cpu->status | Overflow_Flag;
    else 
        cpu->status = cpu->status & 0b10111111;

    cpu->register_a = sum & 0xFF;

    if (sum > 0xFF) cpu_sec(&cpu->status);
    else cpu_clc(&cpu->status);

    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);

    return extra_cycle;
}

static bool cpu_and(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    bool extra_cycle = false;

    switch (mode)
    {
        case Absolute_X:
        case Absolute_Y:
            unsigned short base = cpu_get_operand_address(cpu, Absolute);
            if ((addr >> 8) != (base >> 8))
            {
                cpu->cycles += 1;
                extra_cycle = true;
            }
            break;
        case Indirect_Y:
            unsigned char   base_8 = cpu_mem_read(&cpu->bus, cpu->program_counter),
                            lo = cpu_mem_read(&cpu->bus, (unsigned short)base_8),
                            hi = cpu_mem_read(&cpu->bus, (unsigned short)((unsigned char)(base_8 + 1) % 0x100));

            unsigned short deref = (unsigned short)(hi << 8) | (unsigned short)lo;

            if ((addr >> 8) != (deref >> 8))
            {
                cpu->cycles += 1;
                extra_cycle = true;
            }
            break;
        default: 
            break;
    }

    cpu->register_a &= cpu_mem_read(&cpu->bus, addr);
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);

    return extra_cycle;
}

static int cpu_bcc(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    unsigned char extra_cycles = 0;

    if ((cpu->status & Carry_Flag) == 0)
    {
        unsigned short old = cpu->program_counter + 1;

        cpu->program_counter += (char)cpu_mem_read(&cpu->bus, addr);

        cpu->cycles += 1;
        extra_cycles = 1;

        if ((old >> 8) != ((cpu->program_counter + 1) >> 8))
        {
            cpu->cycles += 1;
            extra_cycles = 2;
        }
    }

    return extra_cycles;
}

static int cpu_bcs(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    unsigned char extra_cycles = 0;

    if ((cpu->status & Carry_Flag) != 0)
    {
        unsigned short old = cpu->program_counter + 1;

        cpu->program_counter += (char)cpu_mem_read(&cpu->bus, addr);

        cpu->cycles += 1;
        extra_cycles = 1;

        if ((old >> 8) != ((cpu->program_counter + 1) >> 8))
        {
            cpu->cycles += 1;
            extra_cycles = 2;
        }
    }

    return extra_cycles;
}

static int cpu_beq(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    unsigned char extra_cycles = 0;

    if ((cpu->status & Zero_Flag) != 0)
    {
        unsigned short old = cpu->program_counter + 1;

        cpu->program_counter += (char)cpu_mem_read(&cpu->bus, addr);

        cpu->cycles += 1;
        extra_cycles = 1;

        if ((old >> 8) != ((cpu->program_counter + 1) >> 8))
        {
            cpu->cycles += 1;
            extra_cycles = 2;
        }
    }

    return extra_cycles;
}

static int cpu_bne(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    unsigned char extra_cycles = 0;

    if ((cpu->status & Zero_Flag) == 0)
    {
        unsigned short old = cpu->program_counter + 1;

        cpu->program_counter += (char)cpu_mem_read(&cpu->bus, addr);

        cpu->cycles += 1;
        extra_cycles = 1;

        if ((old >> 8) != ((cpu->program_counter + 1) >> 8))
        {
            cpu->cycles += 1;
            extra_cycles = 2;
        }
    }

    return extra_cycles;
}

static void cpu_bit(CPU *cpu, enum AddressingMode mode)
{
    unsigned short  addr = cpu_get_operand_address(cpu, mode);

    unsigned char   mem = cpu_mem_read(&cpu->bus, addr),
                    and = cpu->register_a & mem,
                    bit6 = mem & Overflow_Flag,
                    bit7 = mem & Negative_Flag;

    if (and == 0) cpu->status = cpu->status | Zero_Flag;
    else cpu->status = cpu->status & 0b11111101;

    if (bit6) cpu->status = cpu->status | Overflow_Flag;
    else cpu->status = cpu->status & 0b10111111;

    if (bit7) cpu->status = cpu->status | Negative_Flag;
    else cpu->status = cpu->status & 0b01111111;
}

static int cpu_bmi(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    unsigned char extra_cycles = 0;

    if ((cpu->status & Negative_Flag) != 0)
    {
        unsigned short old = cpu->program_counter + 1;

        cpu->program_counter += (char)cpu_mem_read(&cpu->bus, addr);

        cpu->cycles += 1;
        extra_cycles = 1;

        if ((old >> 8) != ((cpu->program_counter + 1) >> 8))
        {
            cpu->cycles += 1;
            extra_cycles = 2;
        }
    }

    return extra_cycles;
}

static int cpu_bpl(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    unsigned char extra_cycles = 0;

    if ((cpu->status & Negative_Flag) == 0)
    {
        unsigned short old = cpu->program_counter + 1;

        cpu->program_counter += (char)cpu_mem_read(&cpu->bus, addr);

        cpu->cycles += 1;
        extra_cycles = 1;

        if ((old >> 8) != ((cpu->program_counter + 1) >> 8))
        {
            cpu->cycles += 1;
            extra_cycles = 2;
        }
    }
    return extra_cycles;
}

static int cpu_bvc(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    unsigned char extra_cycles = 0;

    if ((cpu->status & Overflow_Flag) == 0)
    {
        unsigned short old = cpu->program_counter + 1;

        cpu->program_counter += (char)cpu_mem_read(&cpu->bus, addr);

        cpu->cycles += 1;
        extra_cycles = 1;

        if ((old >> 8) != ((cpu->program_counter + 1) >> 8))
        {
            cpu->cycles += 1;
            extra_cycles = 2;
        }
    }

    return extra_cycles;
}

static int cpu_bvs(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    unsigned char extra_cycles = 0;

    if ((cpu->status & Overflow_Flag) != 0)
    {
        unsigned short old = cpu->program_counter + 1;

        cpu->program_counter += (char)cpu_mem_read(&cpu->bus, addr);

        cpu->cycles += 1;
        extra_cycles = 1;

        if ((old >> 8) != ((cpu->program_counter + 1) >> 8))
        {
            cpu->cycles += 1;
            extra_cycles = 2;
        }
    }

    return extra_cycles;
}

static bool cpu_cmp(CPU *cpu, enum AddressingMode mode)
{
    unsigned short  addr = cpu_get_operand_address(cpu, mode);

    unsigned char   mem = cpu_mem_read(&cpu->bus, addr), 
                    result = cpu->register_a - mem;

    bool extra_cycle = false;

    switch (mode)
    {
        case Absolute_X:
        case Absolute_Y:
            unsigned short base = cpu_get_operand_address(cpu, Absolute);
            if ((addr >> 8) != (base >> 8))
            {
                cpu->cycles += 1;
                extra_cycle = true;
            }
            break;
        case Indirect_Y:
            unsigned char   base_8 = cpu_mem_read(&cpu->bus, cpu->program_counter),
                            lo = cpu_mem_read(&cpu->bus, (unsigned short)base_8),
                            hi = cpu_mem_read(&cpu->bus, (unsigned short)((unsigned char)(base_8 + 1) % 0x100));

            unsigned short deref = (unsigned short)(hi << 8) | (unsigned short)lo;

            if ((addr >> 8) != (deref >> 8))
            {
                cpu->cycles += 1;
                extra_cycle = true;
            }
            break;
        default: 
            break;
    }

    if (cpu->register_a >= mem)
        cpu_sec(&cpu->status);
    else 
        cpu_clc(&cpu->status);

    if (cpu->register_a == mem)
        cpu_zero_set(&cpu->status);
    else 
        cpu_zero_clear(&cpu->status);

    if (result & 0b10000000) 
        cpu->status = cpu->status | Negative_Flag;
    else 
        cpu->status = cpu->status & 0b01111111;

    return extra_cycle;
}

static void cpu_cpx(CPU *cpu, enum AddressingMode mode)
{
    unsigned short  addr = cpu_get_operand_address(cpu, mode);

    unsigned char   mem = cpu_mem_read(&cpu->bus, addr), 
                    result = cpu->register_x - mem;

    if (cpu->register_x >= mem)
        cpu_sec(&cpu->status);
    else 
        cpu_clc(&cpu->status);

    if (cpu->register_x == mem) 
        cpu_zero_set(&cpu->status);
    else 
        cpu_zero_clear(&cpu->status);

    if (result & 0b10000000) 
        cpu->status = cpu->status | Negative_Flag;
    else 
        cpu->status = cpu->status & 0b01111111;
}

static void cpu_cpy(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    unsigned char   mem = cpu_mem_read(&cpu->bus, addr),
                    result = cpu->register_y - mem;

    if (cpu->register_y >= mem)
        cpu_sec(&cpu->status);
    else 
        cpu_clc(&cpu->status);

    if (cpu->register_y == mem) 
        cpu_zero_set(&cpu->status);
    else 
        cpu_zero_clear(&cpu->status);

    if (result & 0b10000000) 
        cpu->status = cpu->status | Negative_Flag;
    else 
        cpu->status = cpu->status & 0b01111111;
}

static void cpu_dec(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    cpu_mem_write(&cpu->bus, addr, cpu_mem_read_u16(cpu, addr) - 1);
    cpu_update_zero_and_negative_flags(&cpu->status, cpu_mem_read_u16(cpu, addr));
}

static void cpu_dex(CPU *cpu)
{
    cpu->register_x -= 1;
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_x);
}

static void cpu_dey(CPU *cpu)
{
    cpu->register_y -= 1;
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_y);
}

static void cpu_brk(CPU *cpu)
{
    // fix this(?)
    cpu_mem_write_u16(cpu, 0x0100 + cpu->stack_pointer - 1, cpu->program_counter);
    cpu->stack_pointer -= 2;
    cpu_mem_write(&cpu->bus, 0x0100 + cpu->stack_pointer, cpu->status);
    cpu->stack_pointer -= 1;
    cpu->status |= Break_Command_Flag;
    cpu->program_counter = cpu_mem_read_u16(cpu, 0xFFFE);
}

static bool cpu_eor(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    bool extra_cycle = false;

    switch (mode)
    {
        case Absolute_X:
        case Absolute_Y:
            unsigned short base = cpu_get_operand_address(cpu, Absolute);
            if ((addr >> 8) != (base >> 8))
            {
                cpu->cycles += 1;
                extra_cycle = true;
            }
            break;
        case Indirect_Y:
            unsigned char   base_8 = cpu_mem_read(&cpu->bus, cpu->program_counter),
                            lo = cpu_mem_read(&cpu->bus, (unsigned short)base_8),
                            hi = cpu_mem_read(&cpu->bus, (unsigned short)((unsigned char)(base_8 + 1) % 0x100));

            unsigned short deref = (unsigned short)(hi << 8) | (unsigned short)lo;

            if ((addr >> 8) != (deref >> 8))
            {
                cpu->cycles += 1;
                extra_cycle = true;
            }
            break;
        default: 
            break;
    }

    cpu->register_a ^= cpu_mem_read(&cpu->bus, addr);
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);

    return extra_cycle;
}

static bool cpu_ora(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    bool extra_cycle = false;

    switch (mode)
    {
        case Absolute_X:
        case Absolute_Y:
            unsigned short base = cpu_get_operand_address(cpu, Absolute);
            if ((addr >> 8) != (base >> 8))
            {
                cpu->cycles += 1;
                extra_cycle = true;
            }
            break;
        case Indirect_Y:
            unsigned char   base_8 = cpu_mem_read(&cpu->bus, cpu->program_counter),
                            lo = cpu_mem_read(&cpu->bus, (unsigned short)base_8),
                            hi = cpu_mem_read(&cpu->bus, (unsigned short)((unsigned char)(base_8 + 1) % 0x100));

            unsigned short deref = (unsigned short)(hi << 8) | (unsigned short)lo;

            if ((addr >> 8) != (deref >> 8))
            {
                cpu->cycles += 1;
                extra_cycle = true;
            }
            break;
        default: 
            break;
    }

    cpu->register_a |= cpu_mem_read(&cpu->bus, addr);
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);

    return extra_cycle;
}

static void cpu_nop(CPU *cpu)
{

}

static void cpu_inc(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    cpu_mem_write(&cpu->bus, addr, cpu_mem_read_u16(cpu, addr) + 1);
    cpu_update_zero_and_negative_flags(&cpu->status, cpu_mem_read_u16(cpu, addr));
}

static void cpu_inx(CPU *cpu)
{
    cpu->register_x += 1;
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_x);
}

static void cpu_iny(CPU *cpu)
{
    cpu->register_y += 1;
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_y);
}

static void cpu_jmp(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    cpu->program_counter = addr;
}

static bool cpu_lda(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    bool extra_cycle = false;

    switch (mode)
    {
        case Absolute_X:
        case Absolute_Y:
            unsigned short base = cpu_get_operand_address(cpu, Absolute);
            if ((addr >> 8) != (base >> 8))
            {
                cpu->cycles += 1; 
                extra_cycle = true;
            }
            break;
        case Indirect_Y:
            unsigned char   base_8 = cpu_mem_read(&cpu->bus, cpu->program_counter),
                            lo = cpu_mem_read(&cpu->bus, (unsigned short)base_8),
                            hi = cpu_mem_read(&cpu->bus, (unsigned short)((unsigned char)(base_8 + 1) % 0x100));

            unsigned short deref = (unsigned short)(hi << 8) | (unsigned short)lo;

            if ((addr >> 8) != (deref >> 8))
            {
                cpu->cycles += 1; 
                extra_cycle = true;
            }
            break;
        default: 
            break;
    }

    cpu->register_a = cpu_mem_read(&cpu->bus, addr);
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);

    return extra_cycle;
}

static bool cpu_ldx(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    bool extra_cycle = false;

    if (mode == Absolute_Y)
    {
        unsigned short base = cpu_get_operand_address(cpu, Absolute);
        if ((addr >> 8) != (base >> 8))
        {
            cpu->cycles += 1; 
            extra_cycle = true;
        }
    }

    cpu->register_x = cpu_mem_read(&cpu->bus, addr);
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_x);

    return extra_cycle;
}

static bool cpu_ldy(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    bool extra_cycle = false;

    if (mode == Absolute_X)
    {
        unsigned short base = cpu_get_operand_address(cpu, Absolute);
        if ((base >> 8) != (addr >> 8))
        {
            cpu->cycles += 1; 
            extra_cycle = true;
        }
    }

    cpu->register_y = cpu_mem_read(&cpu->bus, addr);
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_y);

    return extra_cycle;
}

static void cpu_tax(CPU *cpu)
{
    cpu->register_x = cpu->register_a;
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_x);
}

static void cpu_tay(CPU *cpu)
{
    cpu->register_y = cpu->register_a;
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_y);
}

static void cpu_txa(CPU *cpu)
{
    cpu->register_a = cpu->register_x;
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
}

static void cpu_tya(CPU *cpu)
{
    cpu->register_a = cpu->register_y;
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
}

static void cpu_sta(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    cpu_mem_write(&cpu->bus, addr, cpu->register_a);
}

static void cpu_stx(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    cpu_mem_write(&cpu->bus, addr, cpu->register_x);
}

static void cpu_sty(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    cpu_mem_write(&cpu->bus, addr, cpu->register_y);
}

static void cpu_tsx(CPU *cpu)
{
    cpu->register_x = cpu->stack_pointer;
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_x);
}

static void cpu_txs(CPU *cpu)
{
    cpu->stack_pointer = cpu->register_x;
}

static void cpu_pha(CPU *cpu)
{
    cpu_mem_write(&cpu->bus, 0x0100 + cpu->stack_pointer, cpu->register_a);
    cpu->stack_pointer -= 1;
}

static void cpu_php(CPU *cpu)
{
    unsigned char p = cpu->status;
    p = p | Break_Command_Flag;
    p = p | Unused_Flag;
    cpu_mem_write(&cpu->bus, 0x0100 + cpu->stack_pointer, p);
    cpu->stack_pointer -= 1;
}

static void cpu_pla(CPU *cpu)
{
    cpu->stack_pointer += 1;
    cpu->register_a = cpu_mem_read(&cpu->bus, 0x0100 + cpu->stack_pointer);
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
}

static void cpu_plp(CPU *cpu)
{
    cpu->stack_pointer += 1;
    cpu->status = Unused_Flag;
    cpu->status |= cpu_mem_read(&cpu->bus, 0x0100 + cpu->stack_pointer);
    cpu->status &= 0b11101111;
}

static void cpu_rol(CPU *cpu, enum AddressingMode mode)
{
    if (mode == Accumulator)
    {
        unsigned char   carry = (cpu->status & Carry_Flag),
                        old_bit = cpu->register_a,
                        shift = cpu->register_a << 1,
                        rotate = cpu->register_a >> 7;

        cpu->register_a = shift | rotate;

        if (carry)  cpu->register_a |= Carry_Flag;
        else        cpu->register_a &= 0b11111110;

        if (old_bit & 0b10000000) 
            cpu->status = cpu->status | Carry_Flag;
        else 
            cpu->status = cpu->status & 0b11111110;

        cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
    }
    else
    {
        unsigned short  addr = cpu_get_operand_address(cpu, mode);

        unsigned char   carry = (cpu->status & Carry_Flag),
                        old_bit = cpu_mem_read(&cpu->bus, addr),
                        shift = cpu_mem_read(&cpu->bus, addr) << 1,
                        rotate = cpu_mem_read(&cpu->bus, addr) >> 7;

        cpu_mem_write(&cpu->bus, addr, shift | rotate);

        if (carry) 
            cpu_mem_write(&cpu->bus, addr, cpu_mem_read(&cpu->bus, addr) | Carry_Flag);
        else 
            cpu_mem_write(&cpu->bus, addr, cpu_mem_read(&cpu->bus, addr) & 0b11111110);

        if (old_bit & 0b10000000) 
            cpu->status = cpu->status | Carry_Flag;
        else 
            cpu->status = cpu->status & 0b11111110;

        cpu_update_zero_and_negative_flags(&cpu->status, cpu_mem_read(&cpu->bus, addr));
    }
}

static void cpu_ror(CPU *cpu, enum AddressingMode mode)
{
    if (mode == Accumulator)
    {
        unsigned char   old_bit = cpu->register_a,
                        shift = cpu->register_a >> 1,
                        rotate = cpu->register_a << 7;

        cpu->register_a = shift | rotate;

        if ((cpu->status & 0b00000001) != 0) 
            cpu->register_a = cpu->register_a | 0b10000000;
        else 
            cpu->register_a = cpu->register_a & 0b01111111;

        if (old_bit & 0b00000001) 
            cpu->status = cpu->status | Carry_Flag;
        else 
            cpu->status = cpu->status & 0b11111110;

        cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
    }
    else
    {
        unsigned short addr = cpu_get_operand_address(cpu, mode);

        unsigned char   old_bit = cpu_mem_read(&cpu->bus, addr),
                        shift = cpu_mem_read(&cpu->bus, addr) >> 1,
                        rotate = cpu_mem_read(&cpu->bus, addr) << 7;

        cpu_mem_write(&cpu->bus, addr, shift | rotate);

        if ((cpu->status & 0b00000001) != 0) 
            cpu_mem_write(&cpu->bus, addr, cpu_mem_read(&cpu->bus, addr) | 0b10000000);
        else 
            cpu_mem_write(&cpu->bus, addr, cpu_mem_read(&cpu->bus, addr) & 0b01111111);

        if (old_bit & 0b00000001) 
            cpu->status = cpu->status | Carry_Flag;
        else 
            cpu->status = cpu->status & 0b11111110;

        cpu_update_zero_and_negative_flags(&cpu->status, cpu_mem_read(&cpu->bus, addr));
    }
}

static void cpu_asl(CPU *cpu, enum AddressingMode mode)
{
    if (mode == Accumulator)
    {
        unsigned char old_bit = cpu->register_a;

        cpu->register_a <<= 1;

        if ((old_bit & 0b10000000) != 0)
            cpu->status = cpu->status | Carry_Flag;
        else 
            cpu->status = cpu->status & 0b11111110;

        cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
    }
    else
    {
        unsigned short addr = cpu_get_operand_address(cpu, mode);

        unsigned char   old_bit = cpu_mem_read_u16(cpu, addr);

        cpu_mem_write(&cpu->bus, addr, cpu_mem_read_u16(cpu, addr) << 1);

        if ((old_bit & 0b10000000) != 0)
            cpu->status = cpu->status | Carry_Flag;
        else 
            cpu->status = cpu->status & 0b11111110;

        cpu_update_zero_and_negative_flags(&cpu->status, cpu_mem_read_u16(cpu, addr));
    }
}

static void cpu_lsr(CPU *cpu, enum AddressingMode mode)
{
    if (mode == Accumulator)
    {
        unsigned char old_bit = cpu->register_a;

        cpu->register_a >>= 1;
        cpu->register_a = cpu->register_a & 0b01111111;

        if ((old_bit & 0b00000001) != 0) 
            cpu->status = cpu->status | Carry_Flag;
        else 
            cpu->status = cpu->status & 0b11111110;

        cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
    }
    else
    {
        unsigned short addr = cpu_get_operand_address(cpu, mode);

        unsigned char  old_bit = cpu_mem_read(&cpu->bus, addr);

        if ((old_bit & 0b00000001) != 0) 
            cpu->status = cpu->status | Carry_Flag;
        else 
            cpu->status = cpu->status & 0b11111110;

        cpu_mem_write(&cpu->bus, addr, (cpu_mem_read(&cpu->bus, addr) >> 1) & 0b01111111);

        cpu_update_zero_and_negative_flags(&cpu->status, cpu_mem_read(&cpu->bus, addr));
    }
}

static void cpu_rti(CPU *cpu)
{
    // pull status followed by counter
    cpu->stack_pointer += 1;
    cpu->status = Unused_Flag;
    cpu->status |= cpu_mem_read(&cpu->bus, 0x0100 + cpu->stack_pointer);
    cpu->stack_pointer += 2;
    cpu->program_counter = cpu_mem_read_u16(cpu, 0x0100 + cpu->stack_pointer - 1);
}

static void cpu_rts(CPU *cpu)
{
    cpu->stack_pointer += 2;
    cpu->program_counter = cpu_mem_read_u16(cpu, 0x0100 + cpu->stack_pointer - 1) + 1;
}

static void cpu_jsr(CPU *cpu)
{
    cpu_mem_write_u16(cpu, 0x0100 + cpu->stack_pointer - 1, cpu->program_counter + 1);
    cpu->stack_pointer -= 2;
    cpu->program_counter = cpu_get_operand_address(cpu, Absolute);
}

static void cpu_test(CPU *cpu)
{
    FILE *f;

    if (!(f = fopen("mytest.log", "w")))
    {
        printf("could not open log file!\n");
        return;
    }
    
    int test_counter = 1;

    unsigned char   val1 = cpu_mem_read(&cpu->bus, cpu->program_counter + 1),
                    val2 = cpu_mem_read(&cpu->bus, cpu->program_counter + 2);

    /*
    printf(
        "\nPC:%d OP:%d VAL1:%d VAL2:%d ", 
        cpu->program_counter, 
        opscode, 
        val1, 
        val2);

    printf(
        "A:%d X:%d Y:%d P:%d SP:%d\n",
        cpu->register_a,
        cpu->register_x,
        cpu->register_y,
        cpu->status,
        cpu->stack_pointer);

    
    
    // "C799  85 01     STA $01 = FF                    A:00 X:00 Y:00 P:66 SP:FB"

    printf("program counter: %d\n", test_counter);

    fprintf(
        f, 
        "%X  %X %X %X                                   A:%X X:%X Y:%X P:%X SP:%X PPU:  0, 0 CYC:%d\n",
        cpu->program_counter, opscode, val1, val2, cpu->register_a, cpu->register_x,
        cpu->register_y, cpu->status, cpu->stack_pointer, cpu->cycles);

    */

    if (++test_counter > 8991) return;
}

static void cpu_interpret(CPU *cpu)
{
    //printf("start cpu interpret loop\n");

    //while (1)
    //{
        unsigned char   program_counter_state = 0,
                        opcode_cycles = 0,
                        opscode = cpu_mem_read(&cpu->bus, cpu->program_counter);

        if (cpu->bus.ppu.nmi_interrupt)
            cpu_interrupt_nmi(cpu);

        cpu->program_counter++;

        switch (opscode)
        {
            case 0x02:
            case 0x12:
            case 0x22:
            case 0x32:
            case 0x42:
            case 0x52:
            case 0x62:
            case 0x72:
            case 0x92:
            case 0xB2:
            case 0xD2:
            case 0xF2:
                cpu_kil(cpu);
lockup:
                goto lockup;
                return;

            case 0x40: 
                cpu_rti(cpu); 
                cpu->cycles += 6; 
                opcode_cycles += 6;
                break;

            case 0x28: 
                cpu_plp(cpu); 
                cpu->cycles += 4; 
                opcode_cycles += 4;
                break;

            case 0x68: 
                cpu_pla(cpu); 
                cpu->cycles += 4; 
                opcode_cycles += 4;
                break;

            case 0x08: 
                cpu_php(cpu); 
                cpu->cycles += 3; 
                opcode_cycles += 3;
                break;

            case 0x48: 
                cpu_pha(cpu);
                cpu->cycles += 3;
                opcode_cycles += 3;
                break;

            case 0x9A: 
                cpu_txs(cpu); 
                cpu->cycles += 2; 
                opcode_cycles += 2;
                break;

            case 0xBA: 
                cpu_tsx(cpu); 
                cpu->cycles += 2; 
                opcode_cycles += 2;
                break;

            case 0xA2:
                opcode_cycles += cpu_ldx(cpu, Immediate); 
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0xA6:
                opcode_cycles += cpu_ldx(cpu, Zero_Page); 
                cpu->program_counter += 1;
                cpu->cycles += 3;
                opcode_cycles += 3;
                break;
            case 0xB6:
                opcode_cycles += cpu_ldx(cpu, Zero_Page_Y); 
                cpu->program_counter += 1;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0xAE:
                opcode_cycles += cpu_ldx(cpu, Absolute); 
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0xBE:
                opcode_cycles += cpu_ldx(cpu, Absolute_Y); 
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;

            case 0xA0:
                opcode_cycles += cpu_ldy(cpu, Immediate); 
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0xA4:
                opcode_cycles += cpu_ldy(cpu, Zero_Page); 
                cpu->program_counter += 1;
                cpu->cycles += 3;
                opcode_cycles += 3;
                break;
            case 0xB4:
                opcode_cycles += cpu_ldy(cpu, Zero_Page_X); 
                cpu->program_counter += 1;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0xAC:
                opcode_cycles += cpu_ldy(cpu, Absolute); 
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0xBC:
                opcode_cycles += cpu_ldy(cpu, Absolute_X); 
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;

            case 0xBB:
                opcode_cycles += cpu_lar(cpu, Absolute_Y);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;

            case 0xA7:
                opcode_cycles += cpu_lax(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 3;
                opcode_cycles += 3;
                break;
            case 0xB7:
                opcode_cycles += cpu_lax(cpu, Zero_Page_Y);
                cpu->program_counter += 1;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0xAF:
                opcode_cycles += cpu_lax(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0xBF:
                opcode_cycles += cpu_lax(cpu, Absolute_Y);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0xA3:
                opcode_cycles += cpu_lax(cpu, Indirect_X);
                cpu->program_counter += 1;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0xB3:
                opcode_cycles += cpu_lax(cpu, Indirect_Y);
                cpu->program_counter += 1;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;

            case 0x0B:
            case 0x2B:
                cpu_aac(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
                
            case 0x87:
                cpu_aax(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 3;
                opcode_cycles += 3;
                break;
            case 0x97:
                cpu_aax(cpu, Zero_Page_Y);
                cpu->program_counter += 1;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0x83:
                cpu_aax(cpu, Indirect_X);
                cpu->program_counter += 1;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0x8F:
                cpu_aax(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;

            case 0x6B:
                cpu_arr(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0x4B:
                cpu_asr(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0xAB:
                cpu_atx(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0x93:
                cpu_axa(cpu, Indirect_Y);
                cpu->program_counter += 1;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0x9F:
                cpu_axa(cpu, Absolute_Y);
                cpu->program_counter += 2;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;
            case 0xCB:
                cpu_axs(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;

            case 0xC7:
                cpu_dcp(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;
            case 0xD7:
                cpu_dcp(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0xCF:
                cpu_dcp(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0xDF:
                cpu_dcp(cpu, Absolute_X);
                cpu->program_counter += 2;
                cpu->cycles += 7;
                opcode_cycles += 7;
                break;
            case 0xDB:
                cpu_dcp(cpu, Absolute_Y);
                cpu->program_counter += 2;
                cpu->cycles += 7;
                opcode_cycles += 7;
                break;
            case 0xC3:
                cpu_dcp(cpu, Indirect_X);
                cpu->program_counter += 1;
                cpu->cycles += 8;
                opcode_cycles += 8;
                break;
            case 0xD3:
                cpu_dcp(cpu, Indirect_Y);
                cpu->program_counter += 1;
                cpu->cycles += 8;
                opcode_cycles += 8;
                break;

            case 0x27:
                cpu_rla(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;
            case 0x37:
                cpu_rla(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0x2F:
                cpu_rla(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0x3F:
                cpu_rla(cpu, Absolute_X);
                cpu->program_counter += 2;
                cpu->cycles += 7;
                opcode_cycles += 7;
                break;
            case 0x3B:
                cpu_rla(cpu, Absolute_Y);
                cpu->program_counter += 2;
                cpu->cycles += 7;
                opcode_cycles += 7;
                break;
            case 0x23:
                cpu_rla(cpu, Indirect_X);
                cpu->program_counter += 1;
                cpu->cycles += 8;
                opcode_cycles += 8;
                break;
            case 0x33:
                cpu_rla(cpu, Indirect_Y);
                cpu->program_counter += 1;
                cpu->cycles += 8;
                opcode_cycles += 8;
                break;

            case 0x67:
                cpu_rra(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;
            case 0x77:
                cpu_rra(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0x6F:
                cpu_rra(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0x7F:
                cpu_rra(cpu, Absolute_X);
                cpu->program_counter += 2;
                cpu->cycles += 7;
                opcode_cycles += 7;
                break;
            case 0x7B:
                cpu_rra(cpu, Absolute_Y);
                cpu->program_counter += 2;
                cpu->cycles += 7;
                opcode_cycles += 7;
                break;
            case 0x63:
                cpu_rra(cpu, Indirect_X);
                cpu->program_counter += 1;
                cpu->cycles += 8;
                opcode_cycles += 8;
                break;
            case 0x73:
                cpu_rra(cpu, Indirect_Y);
                cpu->program_counter += 1;
                cpu->cycles += 8;
                opcode_cycles += 8;
                break;

            /*  adc start   */
            case 0x69:
                opcode_cycles += cpu_adc(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0x65:
                opcode_cycles += cpu_adc(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 3;
                opcode_cycles += 3;
                break;
            case 0x75:
                opcode_cycles += cpu_adc(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0x6D:
                opcode_cycles += cpu_adc(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0x7D:
                opcode_cycles += cpu_adc(cpu, Absolute_X);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0x79:
                opcode_cycles += cpu_adc(cpu, Absolute_Y);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0x61:
                opcode_cycles += cpu_adc(cpu, Indirect_X);
                cpu->program_counter += 1;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0x71:
                opcode_cycles += cpu_adc(cpu, Indirect_Y);
                cpu->program_counter += 1;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;
            /*  adc end     */

            case 0xE9:
                opcode_cycles += cpu_sbc(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0xEB:
                opcode_cycles += cpu_sbc(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0xE5:
                opcode_cycles += cpu_sbc(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 3;
                opcode_cycles += 3;
                break;
            case 0xF5:
                opcode_cycles += cpu_sbc(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0xED:
                opcode_cycles += cpu_sbc(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0xFD:
                opcode_cycles += cpu_sbc(cpu, Absolute_X);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0xF9:
                opcode_cycles += cpu_sbc(cpu, Absolute_Y);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0xE1:
                opcode_cycles += cpu_sbc(cpu, Indirect_X);
                cpu->program_counter += 1;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0xF1:
                opcode_cycles += cpu_sbc(cpu, Indirect_Y);
                cpu->program_counter += 1;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;

            case 0xE7:
                cpu_isc(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;
            case 0xF7:
                cpu_isc(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0xEF:
                cpu_isc(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0xFF:
                cpu_isc(cpu, Absolute_X);
                cpu->program_counter += 2;
                cpu->cycles += 7;
                opcode_cycles += 7;
                break;
            case 0xFB:
                cpu_isc(cpu, Absolute_Y);
                cpu->program_counter += 2;
                cpu->cycles += 7;
                opcode_cycles += 7;
                break;
            case 0xE3:
                cpu_isc(cpu, Indirect_X);
                cpu->program_counter += 1;
                cpu->cycles += 8;
                opcode_cycles += 8;
                break;
            case 0xF3:
                cpu_isc(cpu, Indirect_Y);
                cpu->program_counter += 1;
                cpu->cycles += 8;
                opcode_cycles += 8;
                break;

            case 0x07:
                cpu_slo(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;
            case 0x17:
                cpu_slo(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0x0F:
                cpu_slo(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0x1F:
                cpu_slo(cpu, Absolute_X);
                cpu->program_counter += 2;
                cpu->cycles += 7;
                opcode_cycles += 7;
                break;
            case 0x1B:
                cpu_slo(cpu, Absolute_Y);
                cpu->program_counter += 2;
                cpu->cycles += 7;
                opcode_cycles += 7;
                break;
            case 0x03:
                cpu_slo(cpu, Indirect_X);
                cpu->program_counter += 1;
                cpu->cycles += 8;
                opcode_cycles += 8;
                break;
            case 0x13:
                cpu_slo(cpu, Indirect_Y);
                cpu->program_counter += 1;
                cpu->cycles += 8;
                opcode_cycles += 8;
                break;

            case 0x47:
                cpu_sre(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;
            case 0x57:
                cpu_sre(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0x4F:
                cpu_sre(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0x5F:
                cpu_sre(cpu, Absolute_X);
                cpu->program_counter += 2;
                cpu->cycles += 7;
                opcode_cycles += 7;
                break;
            case 0x5B:
                cpu_sre(cpu, Absolute_Y);
                cpu->program_counter += 2;
                cpu->cycles += 7;
                opcode_cycles += 7;
                break;
            case 0x43:
                cpu_sre(cpu, Indirect_X);
                cpu->program_counter += 1;
                cpu->cycles += 8;
                opcode_cycles += 8;
                break;
            case 0x53:
                cpu_sre(cpu, Indirect_Y);
                cpu->program_counter += 1;
                cpu->cycles += 8;
                opcode_cycles += 8;
                break;
            
            case 0x9E:
                cpu_sxa(cpu, Absolute_Y);
                cpu->program_counter += 2;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;

            case 0x9C:
                cpu_sya(cpu, Absolute_X);
                cpu->program_counter += 2;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;

            case 0x8B:
                cpu_xaa(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;

            case 0x9B:
                cpu_xas(cpu, Absolute_Y);
                cpu->program_counter += 2;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;

            case 0x29: 
                opcode_cycles += cpu_and(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0x25: 
                opcode_cycles += cpu_and(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 3;
                opcode_cycles += 3;
                break;
            case 0x35: 
                opcode_cycles += cpu_and(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0x2D: 
                opcode_cycles += cpu_and(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0x3D: 
                opcode_cycles += cpu_and(cpu, Absolute_X);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0x39: 
                opcode_cycles += cpu_and(cpu, Absolute_Y);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0x21: 
                opcode_cycles += cpu_and(cpu, Indirect_X);
                cpu->program_counter += 1;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0x31: 
                opcode_cycles += cpu_and(cpu, Indirect_Y);
                cpu->program_counter += 1;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;

            case 0x49:
                opcode_cycles += cpu_eor(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0x45:
                opcode_cycles += cpu_eor(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 3;
                opcode_cycles += 3;
                break;
            case 0x55:
                opcode_cycles += cpu_eor(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0x4D:
                opcode_cycles += cpu_eor(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0x5D:
                opcode_cycles += cpu_eor(cpu, Absolute_X);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0x59:
                opcode_cycles += cpu_eor(cpu, Absolute_Y);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0x41:
                opcode_cycles += cpu_eor(cpu, Indirect_X);
                cpu->program_counter += 1;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0x51:
                opcode_cycles += cpu_eor(cpu, Indirect_Y);
                cpu->program_counter += 1;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;

            case 0x09:
                opcode_cycles += cpu_ora(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0x05:
                opcode_cycles += cpu_ora(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 3;
                opcode_cycles += 3;
                break;
            case 0x15:
                opcode_cycles += cpu_ora(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0x0D:
                opcode_cycles += cpu_ora(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0x1D:
                opcode_cycles += cpu_ora(cpu, Absolute_X);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0x19:
                opcode_cycles += cpu_ora(cpu, Absolute_Y);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0x01:
                opcode_cycles += cpu_ora(cpu, Indirect_X);
                cpu->program_counter += 1;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0x11:
                opcode_cycles += cpu_ora(cpu, Indirect_Y);
                cpu->program_counter += 1;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;

            case 0x58: 
                cpu_cli(&cpu->status); 
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0x78: 
                cpu_sei(&cpu->status);
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;

            /*      STA begin   */
            case 0x85:
                cpu_sta(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 3;
                opcode_cycles += 3;
                break;
            case 0x95:
                cpu_sta(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0x8D:
                cpu_sta(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0x9D:
                cpu_sta(cpu, Absolute_X);
                cpu->program_counter += 2;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;
            case 0x99:
                cpu_sta(cpu, Absolute_Y);
                cpu->program_counter += 2;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;
            case 0x81:
                cpu_sta(cpu, Indirect_X);
                cpu->program_counter += 1;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0x91:
                cpu_sta(cpu, Indirect_Y);
                cpu->program_counter += 1;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            /*      STA end     */

            case 0x86:
                cpu_stx(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 3;
                opcode_cycles += 3;
                break;
            case 0x96:
                cpu_stx(cpu, Zero_Page_Y);
                cpu->program_counter += 1;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0x8E:
                cpu_stx(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;

            case 0x84:
                cpu_sty(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 3;
                opcode_cycles += 3;
                break;
            case 0x94:
                cpu_sty(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0x8C:
                cpu_sty(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;

            case 0xA9: 
                opcode_cycles += cpu_lda(cpu, Immediate); 
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0xA5:
                opcode_cycles += cpu_lda(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 3;
                opcode_cycles += 3;
                break;
            case 0xB5:
                opcode_cycles += cpu_lda(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0xAD:
                opcode_cycles += cpu_lda(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0xBD:
                opcode_cycles += cpu_lda(cpu, Absolute_X);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0xB9:
                opcode_cycles += cpu_lda(cpu, Absolute_Y);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0xA1:
                opcode_cycles += cpu_lda(cpu, Indirect_X);
                cpu->program_counter += 1;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0xB1:
                opcode_cycles += cpu_lda(cpu, Indirect_Y);
                cpu->program_counter += 1;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;

            case 0xAA: 
                cpu_tax(cpu); 
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0x8A: 
                cpu_txa(cpu); 
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0xA8: 
                cpu_tay(cpu); 
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0x98: 
                cpu_tya(cpu); 
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;

            case 0xE6: 
                cpu_inc(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;
            case 0xF6: 
                cpu_inc(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0xEE: 
                cpu_inc(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0xFE: 
                cpu_inc(cpu, Absolute_X);
                cpu->program_counter += 2;
                cpu->cycles += 7;
                opcode_cycles += 7;
                break;

            case 0xC9:
                opcode_cycles += cpu_cmp(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0xC5:
                opcode_cycles += cpu_cmp(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 3;
                opcode_cycles += 3;
                break;
            case 0xD5:
                opcode_cycles += cpu_cmp(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0xCD:
                opcode_cycles += cpu_cmp(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0xDD:
                opcode_cycles += cpu_cmp(cpu, Absolute_X);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0xD9:
                opcode_cycles += cpu_cmp(cpu, Absolute_Y);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;
            case 0xC1:
                opcode_cycles += cpu_cmp(cpu, Indirect_X);
                cpu->program_counter += 1;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0xD1:
                opcode_cycles += cpu_cmp(cpu, Indirect_Y);
                cpu->program_counter += 1;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;

            case 0xE0:
                cpu_cpx(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0xE4:
                cpu_cpx(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 3;
                opcode_cycles += 3;
                break;
            case 0xEC:
                cpu_cpx(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;

            case 0xC0:
                cpu_cpy(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0xC4:
                cpu_cpy(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 3;
                opcode_cycles += 3;
                break;
            case 0xCC:
                cpu_cpy(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;

            case 0xC6:
                cpu_dec(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;
            case 0xD6:
                cpu_dec(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0xCE:
                cpu_dec(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0xDE:
                cpu_dec(cpu, Absolute_X);
                cpu->program_counter += 2;
                cpu->cycles += 7;
                opcode_cycles += 7;
                break;

            case 0x4C: 
                cpu_jmp(cpu, Absolute);
                cpu->cycles += 3;
                opcode_cycles += 3;
                break;
            case 0x6C:
                cpu_jmp(cpu, Indirect);
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;

            case 0xCA: 
                cpu_dex(cpu); 
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0x88: 
                cpu_dey(cpu); 
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0xC8: 
                cpu_iny(cpu); 
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0xE8: 
                cpu_inx(cpu); 
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0x18: 
                cpu_clc(&cpu->status);
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0x38: 
                cpu_sec(&cpu->status); 
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0xB8: 
                cpu_clv(&cpu->status);
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0xD8: 
                cpu_cld(&cpu->status); 
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0xF8: 
                cpu_sed(&cpu->status); 
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;

            case 0x0A: 
                cpu_asl(cpu, Accumulator);
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0x06:
                cpu_asl(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;
            case 0x16:
                cpu_asl(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0x0E:
                cpu_asl(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0x1E:
                cpu_asl(cpu, Absolute_X);
                cpu->program_counter += 2;
                cpu->cycles += 7;
                opcode_cycles += 7;
                break;

            case 0x4A: 
                cpu_lsr(cpu, Accumulator);
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0x46:
                cpu_lsr(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;
            case 0x56:
                cpu_lsr(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0x4E:
                cpu_lsr(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0x5E:
                cpu_lsr(cpu, Absolute_X);
                cpu->program_counter += 2;
                cpu->cycles += 7;
                opcode_cycles += 7;
                break;

            case 0x2A: 
                cpu_rol(cpu, Accumulator);
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0x26:
                cpu_rol(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 5;
                opcode_cycles += 5;
                break;
            case 0x36:
                cpu_rol(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0x2E:
                cpu_rol(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 6;
                opcode_cycles += 6;
                break;
            case 0x3E:
                cpu_rol(cpu, Absolute_X);
                cpu->program_counter += 2;
                cpu->cycles += 7;
                opcode_cycles += 7;
                break;

            case 0x6A:
                cpu_ror(cpu, Accumulator); 
                cpu->cycles += 2;
                opcode_cycles = 2;
                break;
            case 0x66:
                cpu_ror(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 5;
                opcode_cycles = 5;
                break;
            case 0x76:
                cpu_ror(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                cpu->cycles += 6;
                opcode_cycles = 6;
                break;
            case 0x6E:
                cpu_ror(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 6;
                opcode_cycles = 6;
                break;
            case 0x7E:
                cpu_ror(cpu, Absolute_X);
                cpu->program_counter += 2;
                cpu->cycles += 7;
                opcode_cycles = 7;
                break;

            case 0x90:
                opcode_cycles += cpu_bcc(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0xB0:
                opcode_cycles += cpu_bcs(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0xF0:
                opcode_cycles += cpu_beq(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0xD0:
                opcode_cycles += cpu_bne(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0x24:
                cpu_bit(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 3;
                opcode_cycles = 3;
                break;
            case 0x2C:
                cpu_bit(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles = 4;
                break;
            case 0x30:
                opcode_cycles += cpu_bmi(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0x10:
                opcode_cycles += cpu_bpl(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0x50:
                opcode_cycles += cpu_bvc(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;
            case 0x70:
                opcode_cycles += cpu_bvs(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles += 2;
                break;

            case 0x20: 
                cpu_jsr(cpu); 
                cpu->cycles += 6;
                opcode_cycles = 6;
                break;
            case 0x60: 
                cpu_rts(cpu);
                cpu->cycles += 6;
                opcode_cycles = 6;
                break;

            case 0xEA:
            case 0x1A:
            case 0x3A:
            case 0x5A:
            case 0x7A:
            case 0xDA:
            case 0xFA:
                cpu_nop(cpu);
                cpu->cycles += 2;
                opcode_cycles = 2;
                break;

            case 0x80:
            case 0x82:
            case 0x89:
            case 0xC2:
            case 0xE2:
                cpu_dop(cpu, Immediate);
                cpu->program_counter += 1;
                cpu->cycles += 2;
                opcode_cycles = 2;
                break;
            case 0x04:
            case 0x44:
            case 0x64:
                cpu_dop(cpu, Zero_Page);
                cpu->program_counter += 1;
                cpu->cycles += 3;
                opcode_cycles = 3;
                break;
            case 0x14:
            case 0x34:
            case 0x54:
            case 0x74:
            case 0xD4:
            case 0xF4:
                cpu_dop(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                cpu->cycles += 4;
                opcode_cycles = 4;
                break;

            case 0x0C:
                cpu_top(cpu, Absolute);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles = 4;
                break;
            case 0x1C:
            case 0x3C:
            case 0x5C:
            case 0x7C:
            case 0xDC:
            case 0xFC:
                opcode_cycles += cpu_top(cpu, Absolute_X);
                cpu->program_counter += 2;
                cpu->cycles += 4;
                opcode_cycles += 4;
                break;

            case 0x00: 
                cpu_brk(cpu);
                cpu->cycles += 7;
                opcode_cycles = 7;
                break;
        }

        bus_tick(&cpu->bus, opcode_cycles);

        //if (program_counter_state == cpu->program_counter)
        //    cpu->program_counter += (unsigned short)opscode - 1;
    //}
}

static void e_file_handler(unsigned char *buffer, int len)
{
    printf("hello from emulator file handler!\n");
}

static void test_format_mem_access(const char *filename)
{
    FILE *f;

    f = fopen(filename, "r");

    if (!f)
    {
        printf("Could not open file!\n");
        return;
    }

    unsigned char *file_buffer;
    int i = 0;

    file_buffer = malloc(40976);

    while (!feof(f))
    {
        file_buffer[i] = fgetc(f);
        i++;
    }

    fclose(f);
    
    CPU cpu;

    printf("reset rom\n");
    rom_init(&cpu.bus.rom);

    printf("rom load\n");
    if (!rom_load(&cpu.bus.rom, file_buffer))
    {
        printf("Could not load file buffer!\n");
        return;
    }

    printf("reset cpu\n");
    cpu_init(&cpu);
    
    /*
    bus_mem_write(&cpu.bus, 100, 0x11);
    bus_mem_write(&cpu.bus, 101, 0x33);

    bus_mem_write(&cpu.bus, 0x33, 00);
    bus_mem_write(&cpu.bus ,0x34, 04);

    bus_mem_write(&cpu.bus, 0x400, 0xAA);

    cpu.program_counter = 64;
    cpu.register_y = 0;

    printf("0064  11 33     ORA ($33),Y = 0400 @ 0400 = AA  A:00 X:00 Y:00 P:24 SP:FD");
    */

    cpu_interpret(&cpu);

    printf("free file buffer\n");
    free(file_buffer);

    bus_free_rom(&cpu.bus.rom);
}
