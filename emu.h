#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

#define PRG_ROM_PAGE_SIZE   0x4000
#define CHR_ROM_PAGE_SIZE   0x2000

#define STACK_RESET         0x01FF
#define RAM                 0x0000
#define RAM_MIRRORS_END     0x1FFF
#define PPU_REGISTERS       0x2000
#define PPU_REGISTERS_END   0x3FFF

#define FRAME_WIDTH         256 
#define FRAME_HEIGHT        240
// value of width * height * 3
#define FRAME_LENGTH        184320
// width of frame width * 3 = 256 * 3
#define FRAME_PITCH         768

#define TEST_FRAME_LENGTH   61440 

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

enum PPUControlRegister
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

enum PPUMaskRegister
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

enum PPUStatusRegister
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
extern uint8_t NES_PALETTE[192];

enum JoypadButton
{
    RIGHT   = 0b10000000,
    LEFT    = 0b01000000,
    DOWN    = 0b00100000,
    UP      = 0b00010000,
    START   = 0b00001000,
    SELECT  = 0b00000100,
    B       = 0b00000010,
    A       = 0b00000001
};

enum AudioStatusRegister
{
    PULSE_WAVE_ONE  = 0b00000001,
    PULSE_WAVE_TWO  = 0b00000010,
    TRIANGLE_WAVE   = 0b00000100,
    NOISE_WAVE      = 0b00001000,
    DMC_ACTIVE      = 0b00010000,
    FRAME_INTERRUPT = 0b01000000,
    DMC_INTERRUPT   = 0b10000000
};

enum AudioCounterRegister
{
    IRQ_INHIBIT = 0b01000000,
    STEP_MODE   = 0b10000000,
};

enum SweepRegister
{
    SHIFT_COUNT_1   = 0b00000001,
    SHIFT_COUNT_2   = 0b00000010,
    SHIFT_COUNT_3   = 0b00000100,
    NEGATE          = 0b00001000,
    DIVIDER_1       = 0b00010000,
    DIVIDER_2       = 0b00100000,
    DIVIDER_3       = 0b01000000,
    ENABLED         = 0b10000000
};

typedef struct Pulse
{
    enum SweepRegister sweep;

    unsigned char   timer_low, 
                    timer_high;

    unsigned short  timer_period;

    unsigned char   envelope;

    unsigned char   length_counter;

    unsigned char   output;

    bool            length_halt_flag:1, const_vol_env_flag:1;
} Pulse;

typedef struct Triangle
{
    unsigned char   timer_low, 
                    timer_high;

    unsigned short  timer_period;

    unsigned char   length_counter,
                    linear_counter;

    unsigned char   output;

    bool            length_halt:1;
} Triangle;

typedef struct Noise
{
    unsigned char   timer_low, 
                    timer_high;

    unsigned char   envelope, period;

    unsigned char   length_counter,
                    linear_shift;

    unsigned char   output;

    bool            length_halt:1, constant:1, mode:1;
} Noise;

typedef struct DMC
{
    unsigned char   timer_low, 
                    timer_high;

    unsigned char   memory_reader, 
                    sample_buffer,
                    output_unit, 
                    rate;

    unsigned char   output;

    unsigned short  sample_addr, sample_length;

    bool            loop:1;
} DMC;

typedef struct Joypad
{
    bool                strobe:1;
    unsigned char       index, button_status;
} Joypad;

typedef struct Rect
{
    short   x1, y1, x2, y2;
} Rect;

typedef struct Palette
{
    unsigned char   p1, p2, p3, p4;
} Palette;

typedef struct Frame
{
    unsigned char   data[FRAME_LENGTH];
} Frame;

typedef struct AddrRegister
{
    // high byte first, low second
    unsigned char   value[2];
    bool            hi_ptr:1;
} AddrRegister;

typedef struct ScrollRegister
{
    unsigned char   x, y;
    bool            toggle:1;
} ScrollRegister;

typedef struct AudioProcessingUnit
{
    enum AudioStatusRegister status;
    enum AudioCounterRegister ctr_register;

    Pulse       pulse1, pulse2;
    Triangle    triangle;
    Noise       noise;
    DMC         dmc;
} APU;

typedef struct PPU
{
    unsigned char           *chr_rom,
                            palette_table[32],
                            vram[2048],
                            oam_data[256],
                            oam_addr,
                            internal_data_buf,
                            latch;

    bool                    nmi_interrupt:1, nmi_write:1;

    unsigned short          scanline, cycles;

    enum Mirroring          mirroring;
    enum PPUControlRegister ctrl;
    enum PPUMaskRegister    mask;
    enum PPUStatusRegister  status;

    AddrRegister            addr;
    ScrollRegister          scroll;

    Frame                   *frame;
} PPU;

typedef struct Rom
{
    unsigned char   *prg_rom,
                    *chr_rom,
                    mapper;

    unsigned short  prg_len, chr_len;

    enum Mirroring  screen_mirroring;
} Rom;

typedef struct Bus
{
    unsigned char   cpu_vram[2048],
                    *prg_rom;

    unsigned int    cycles;

    Joypad joypad1, joypad2;
    Rom rom;
    PPU ppu;
    APU apu;
} Bus;

typedef struct CPU
{
    unsigned char           register_a, 
                            register_x, 
                            register_y,
                            stack_pointer;

    enum ProcessorStatus    status;

    unsigned short          program_counter;
    unsigned int            cycles;

    Bus                     bus;
} CPU;

void apu_pulse_set_duty(Pulse *pulse, uint8_t data);
void apu_pulse_set_counter_hi_timer(Pulse *pulse, uint8_t data);
void apu_pulse_set_sweep(Pulse *pulse, uint8_t data);

void apu_triangle_set_counter_hi_timer(Triangle *triangle, uint8_t data);

void joypad_init(Joypad *joypad);
void joypad_write(Joypad *joypad, uint8_t data);
uint8_t joypad_read(Joypad *joypad);

void frame_init(Frame *frame);
void frame_set_pixel(uint8_t frame[], short x, short y, uint8_t rgb[3]);

Palette bg_palette(PPU *ppu, uint8_t *attr_table, uint8_t tile_column, uint8_t tile_row);
Palette ppu_sprite_palette(PPU *ppu, uint8_t palette_idx);

void ppu_render_name_table(PPU *ppu, 
                            Frame *frame, 
                            uint8_t test_frame[],
                            uint8_t *name_table, 
                            Rect viewport, 
                            short shift_x, 
                            short shift_y);

void ppu_render(PPU *ppu, Frame *frame);

void ppu_render_scanline_sprite(PPU *ppu, uint8_t frame[], uint8_t test[]);
void ppu_render_scanline_nametable(PPU *ppu,
                                        unsigned char vfb[], 
                                        unsigned char *name_table, 
                                        Rect viewport,
                                        short shift_x, 
                                        short shift_y);

void ppu_render_scanline(PPU *ppu, uint8_t frame[]);

extern void cpu_callback(Bus *bus);

uint8_t vram_addr_increment(enum PPUControlRegister ctrl);

void addr_reset(AddrRegister *addr);
void addr_set(AddrRegister *addr, uint16_t data);
uint16_t addr_get(AddrRegister addr);
void addr_update(AddrRegister *addr, uint8_t data);
void addr_reset_latch(AddrRegister *addr);
void addr_increment(AddrRegister *addr, uint8_t inc);

bool ppu_is_sprite_0_hit(PPU *ppu);
bool ppu_tick(PPU *ppu, uint16_t cycles);
void ppu_load(PPU *ppu, uint8_t chr_rom[], enum Mirroring mirroring);
void ppu_write_to_ctrl(PPU *ppu, uint8_t value);
void ppu_write_to_ppu_addr(PPU *ppu, uint8_t data);
void ppu_increment_vram_addr(PPU *ppu);
uint16_t ppu_mirror_vram_addr(PPU *ppu, uint16_t addr);
uint8_t ppu_read_data(PPU *ppu);
void ppu_write_to_data(PPU *ppu, uint8_t data);

bool rom_load(Rom *rom, uint8_t data[]);
void rom_init(Rom *rom);
void rom_reset(Rom *rom);

uint8_t rom_read_prg_rom(Bus *bus, uint16_t addr);

void bus_tick(Bus *bus, uint16_t cycles);
void bus_free_rom(Rom *rom);
uint8_t bus_mem_read(Bus *bus, uint16_t addr);
void bus_mem_write(Bus *bus, uint16_t addr, uint8_t data);

uint8_t cpu_mem_read(Bus *bus, uint16_t addr);
void cpu_mem_write(Bus *bus, uint16_t addr, uint8_t data);
uint16_t cpu_mem_read_u16(CPU *cpu, uint16_t pos);
void cpu_mem_write_u16(CPU *cpu, uint16_t pos, uint16_t data);

void cpu_update_zero_and_negative_flags(enum ProcessorStatus *cpu_status, uint8_t result);

void cpu_clc(enum ProcessorStatus *cpu_status);
void cpu_sec(enum ProcessorStatus *status);
void cpu_cld(enum ProcessorStatus *status);
void cpu_sed(enum ProcessorStatus *status);
void cpu_cli(enum ProcessorStatus *status);
void cpu_sei(enum ProcessorStatus *status);
void cpu_zero_clear(enum ProcessorStatus *status);
void cpu_zero_set(enum ProcessorStatus *status);
void cpu_clv(enum ProcessorStatus *status);

void cpu_interrupt_nmi(CPU *cpu);
void cpu_init(CPU *cpu);
void cpu_reset(CPU *cpu);

unsigned short cpu_get_operand_address(CPU *cpu, enum AddressingMode mode);

void cpu_aac(CPU *cpu, enum AddressingMode mode);
void cpu_aax(CPU *cpu, enum AddressingMode mode);
void cpu_arr(CPU *cpu, enum AddressingMode mode);
void cpu_asr(CPU *cpu, enum AddressingMode mode);
void cpu_atx(CPU *cpu, enum AddressingMode mode);
void cpu_axa(CPU *cpu, enum AddressingMode mode);
void cpu_axs(CPU *cpu, enum AddressingMode mode);
void cpu_dcp(CPU *cpu, enum AddressingMode mode);
void cpu_dop(CPU *cpu, enum AddressingMode mode);
void cpu_isc(CPU *cpu, enum AddressingMode mode);
void cpu_kil(CPU *cpu);
bool cpu_lar(CPU *cpu, enum AddressingMode mode);
bool cpu_lax(CPU *cpu, enum AddressingMode mode);
void cpu_rla(CPU *cpu, enum AddressingMode mode);
void cpu_rra(CPU *cpu, enum AddressingMode mode);
void cpu_slo(CPU *cpu, enum AddressingMode mode);
void cpu_sre(CPU *cpu, enum AddressingMode mode);
void cpu_sxa(CPU *cpu, enum AddressingMode mode);
void cpu_sya(CPU *cpu, enum AddressingMode mode);
bool cpu_top(CPU *cpu, enum AddressingMode mode);
void cpu_xaa(CPU *cpu, enum AddressingMode mode);
void cpu_xas(CPU *cpu, enum AddressingMode mode);
bool cpu_adc(CPU *cpu, enum AddressingMode mode);
bool cpu_sbc(CPU *cpu, enum AddressingMode mode);
bool cpu_and(CPU *cpu, enum AddressingMode mode);
int cpu_bcc(CPU *cpu, enum AddressingMode mode);
int cpu_bcs(CPU *cpu, enum AddressingMode mode);
int cpu_beq(CPU *cpu, enum AddressingMode mode);
int cpu_bne(CPU *cpu, enum AddressingMode mode);
void cpu_bit(CPU *cpu, enum AddressingMode mode);
int cpu_bmi(CPU *cpu, enum AddressingMode mode);
int cpu_bpl(CPU *cpu, enum AddressingMode mode);
int cpu_bvc(CPU *cpu, enum AddressingMode mode);
int cpu_bvs(CPU *cpu, enum AddressingMode mode);
bool cpu_cmp(CPU *cpu, enum AddressingMode mode);
void cpu_cpx(CPU *cpu, enum AddressingMode mode);
void cpu_cpy(CPU *cpu, enum AddressingMode mode);
void cpu_dec(CPU *cpu, enum AddressingMode mode);
void cpu_dex(CPU *cpu);
void cpu_dey(CPU *cpu);
void cpu_brk(CPU *cpu);
bool cpu_eor(CPU *cpu, enum AddressingMode mode);
bool cpu_ora(CPU *cpu, enum AddressingMode mode);
void cpu_nop(CPU *cpu);
void cpu_inc(CPU *cpu, enum AddressingMode mode);
void cpu_inx(CPU *cpu);
void cpu_iny(CPU *cpu);
void cpu_jmp(CPU *cpu, enum AddressingMode mode);
bool cpu_lda(CPU *cpu, enum AddressingMode mode);
bool cpu_ldx(CPU *cpu, enum AddressingMode mode);
bool cpu_ldy(CPU *cpu, enum AddressingMode mode);
void cpu_tax(CPU *cpu);
void cpu_tay(CPU *cpu);
void cpu_txa(CPU *cpu);
void cpu_tya(CPU *cpu);
void cpu_sta(CPU *cpu, enum AddressingMode mode);
void cpu_stx(CPU *cpu, enum AddressingMode mode);
void cpu_sty(CPU *cpu, enum AddressingMode mode);
void cpu_tsx(CPU *cpu);
void cpu_txs(CPU *cpu);
void cpu_pha(CPU *cpu);
void cpu_php(CPU *cpu);
void cpu_pla(CPU *cpu);
void cpu_plp(CPU *cpu);
void cpu_rol(CPU *cpu, enum AddressingMode mode);
void cpu_ror(CPU *cpu, enum AddressingMode mode);
void cpu_asl(CPU *cpu, enum AddressingMode mode);
void cpu_lsr(CPU *cpu, enum AddressingMode mode);
void cpu_rti(CPU *cpu);
void cpu_rts(CPU *cpu);
void cpu_jsr(CPU *cpu);

void cpu_interpret(CPU *cpu);

void cpu_test(CPU *cpu);

void e_file_handler(unsigned char *buffer, int len);

void test_format_mem_access(const char *filename);
