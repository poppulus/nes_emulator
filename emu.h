#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define STACK_RESET 0x01FF

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

typedef struct CPU
{
    unsigned char   register_a, 
                    register_x, 
                    register_y, 
                    status,
                    stack_pointer;

    unsigned char   memory[0xFFFF];

    unsigned short program_counter;
} CPU;

typedef struct Emulator
{
    CPU cpu;
} Emulator;


static unsigned char cpu_mem_read(unsigned char memory[], unsigned short addr)
{
    return memory[addr];
}

static void cpu_mem_write(unsigned char memory[], unsigned short addr, unsigned char data)
{
    memory[addr] = data;
}

static unsigned short cpu_mem_read_u16(CPU *cpu, unsigned short pos)
{
    unsigned short  hi = cpu->memory[pos], 
                    lo = cpu->memory[pos + 1];

    return (hi << 8) | lo;
}

static void cpu_mem_write_u16(CPU *cpu, unsigned short pos, unsigned short data)
{
    unsigned char   hi = data >> 8, 
                    lo = data & 0xFF;

    cpu->memory[pos] = lo;
    cpu->memory[pos + 1] = hi;
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

static void cpu_sec(unsigned char *status)
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

static void cpu_reset(CPU *cpu)
{
    cpu->register_a = 0;
    cpu->register_x = 0;
    cpu->register_y = 0;
    cpu->status = 0;
    cpu->stack_pointer = STACK_RESET;
    cpu->program_counter = cpu_mem_read_u16(cpu, 0xFFFC);
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
            opaddr = (unsigned short)cpu->memory[cpu->program_counter];
            break;
        case Zero_Page_X:
            pos = cpu->memory[cpu->program_counter] + cpu->register_x;
            opaddr = (unsigned short)pos;
            break;
        case Zero_Page_Y:
            pos = cpu->memory[cpu->program_counter] + cpu->register_y;
            opaddr = (unsigned short)pos;
            break;
        case Absolute:
            opaddr = cpu_mem_read_u16(cpu, cpu->program_counter);
            break;
        case Absolute_X:
            base = cpu_mem_read_u16(cpu, cpu->program_counter);
            opaddr = base + (unsigned short)cpu->register_x;
            break;
        case Absolute_Y:
            base = cpu_mem_read_u16(cpu, cpu->program_counter);
            opaddr = base + (unsigned short)cpu->register_y;
            break;
        case Indirect: // only for JMP
            base = cpu_mem_read_u16(cpu, cpu->program_counter);

            if ((base & 0xFF) == 0xFF)
            {
                lo = cpu->memory[base];
                hi = cpu->memory[base & 0xFF00];
                opaddr = ((unsigned short)hi << 8) | (unsigned short)lo;
            }
            else 
            {
                lo = cpu->memory[base];
                hi = cpu->memory[base + 1];
                opaddr = ((unsigned short)hi << 8) | (unsigned short)lo;
            }
            break;
        case Indirect_X:
            base_8 = cpu->memory[cpu->program_counter];

            unsigned char ptr = base_8 + cpu->register_x;

            lo = cpu->memory[(unsigned short)ptr];
            hi = cpu->memory[(unsigned short)(ptr + 1)];

            opaddr = ((unsigned short)hi << 8) | (unsigned short)lo;
            break;
        case Indirect_Y:
            base_8 = cpu->memory[cpu->program_counter];

            lo = cpu->memory[(unsigned short)base_8];
            hi = cpu->memory[(unsigned short)((unsigned char)(base_8 + 1))];

            unsigned short deref_base = ((unsigned short)hi << 8) | (unsigned short)lo;
            unsigned short deref = deref_base + (unsigned short)cpu->register_y;

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

    cpu->register_a &= cpu->memory[addr];

    if (cpu->register_a & Negative_Flag) cpu_sec(&cpu->status);
    else cpu_clc(&cpu->status);

    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
}

static void cpu_aax(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    cpu->register_a &= cpu->register_x;
    cpu->memory[addr] = cpu->register_a;

    cpu_update_zero_and_negative_flags(&cpu->status, cpu->memory[addr]);
}

static void cpu_arr(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    cpu->register_a &= cpu->memory[addr];
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

    cpu->register_a &= cpu->memory[addr];
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

    cpu->register_a &= cpu->memory[addr];
    cpu->register_x = cpu->register_a;

    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_x);
}

static void cpu_axa(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    cpu->register_x &= cpu->register_a;
    cpu->register_x &= 0x07;
    cpu->memory[addr] = cpu->register_x;
}

static void cpu_axs(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    cpu->register_x &= cpu->register_a;
    cpu->register_x -= cpu->memory[addr];

    if (cpu->register_x & Carry_Flag) cpu_sec(&cpu->status);
    else cpu_clc(&cpu->status);

    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_x);
}

static void cpu_dcp(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    cpu->memory[addr] -= 1;

    if (cpu->memory[addr] & Carry_Flag) cpu_sec(&cpu->status);
    else cpu_clc(&cpu->status);
}

static void cpu_dop(CPU *cpu, enum AddressingMode mode)
{
    
}

static void cpu_isc(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    unsigned char   value = cpu->memory[addr] + 1 - (cpu->status & Carry_Flag),
                    acc = cpu->register_a,
                    sum = acc + value;

    // check carry and overflow
    if ((acc ^ sum) & (value ^ sum) & 0x80)
        cpu->status = cpu->status | Overflow_Flag;
    else 
        cpu->status = cpu->status & 0b10111111;

    if (acc >= value) cpu_sec(&cpu->status);
    else cpu_clc(&cpu->status);

    cpu->memory[addr] += 1;
    cpu->register_a -= cpu->memory[addr] - (cpu->status & Carry_Flag);

    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
}

static void cpu_kil(CPU *cpu, enum AddressingMode mode)
{
    
}

static void cpu_lar(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    unsigned char and = cpu->memory[addr] & cpu->memory[cpu->stack_pointer];

    cpu->register_a = and;
    cpu->register_x = and;
    cpu->memory[addr] = and;
    
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->memory[addr]);
}

static void cpu_lax(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    cpu->register_a = cpu->memory[addr];
    cpu->register_x = cpu->memory[addr];

    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_x);
}

static void cpu_dop()
{
    // jump over extra byte
}

static void cpu_rla(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    unsigned char   shift = cpu->memory[addr] << 1, 
                    rotate = cpu->memory[addr] >> 7;

    cpu->memory[addr] = shift | rotate;

    unsigned char result = cpu->memory[addr] &cpu->register_a;

    if (cpu->memory[addr] <= result) cpu_sec(&cpu->status);
    else cpu_clc(&cpu->status);

    cpu_update_zero_and_negative_flags(&cpu->status, result);
}

static void cpu_rra(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    unsigned char   shift = cpu->memory[addr] >> 1,
                    rotate = cpu->memory[addr] << 7,
                    value = cpu->memory[addr] + (cpu->status & Carry_Flag),
                    sum = cpu->register_a + value;

    cpu->memory[addr] = shift | rotate;

    if (cpu->register_a <= value) cpu_sec(&cpu->status);
    else cpu_clc(&cpu->status);

    if ((cpu->register_a ^ value) & (value ^ sum) & 0x80)
        cpu->status = cpu->status | Overflow_Flag;
    else 
        cpu->status = cpu->status & 0b10111111;

    cpu->register_a += value;
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
}

static void cpu_slo(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    cpu->memory[addr] <<= 1;

    unsigned char result = cpu->register_a | cpu->memory[addr];

    if (cpu->memory[addr] <= result) cpu_sec(&cpu->status);
    else cpu_clc(&cpu->status);

    cpu_update_zero_and_negative_flags(&cpu->status, result);
}

static void cpu_sre(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    cpu->memory[addr] >>= 1;

    unsigned char result = cpu->memory[addr] ^ cpu->register_a;

    if (cpu->memory[addr] >= result) cpu_sec(&cpu->status);
    else cpu_clc(&cpu->status);

    cpu_update_zero_and_negative_flags(&cpu->status, result);
}

static void cpu_sxa(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    unsigned char hi = (unsigned char)addr;

    cpu->memory[addr] = (cpu->register_x & hi) + 1;
}

static void cpu_sya(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    unsigned char hi = (unsigned char)addr;

    cpu->memory[addr] = (cpu->register_y & hi) + 1;
}

static void cpu_top(CPU *cpu, enum AddressingMode mode)
{
    // jump over 2 extra bytes
}

static void cpu_xaa(CPU *cpu, enum AddressingMode mode)
{
    // find documentation
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    cpu->register_a = cpu->register_x;

    unsigned char result = cpu->register_a & cpu->memory[addr];

    cpu_update_zero_and_negative_flags(&cpu->status, result);
}

static void cpu_xas(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    unsigned char hi = (unsigned char)addr;

    cpu->memory[cpu->stack_pointer] = cpu->register_x & cpu->register_a;

    unsigned char result = (cpu->memory[cpu->stack_pointer] & hi) + 1;

    cpu->memory[addr] = result;
}

static void cpu_adc(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    unsigned char   value = cpu->memory[addr] + (cpu->status & Carry_Flag),
                    acc = cpu->register_a,
                    sum = acc + value;

    if ((acc ^ sum) & (value ^ sum) & 0x80)
        cpu->status = cpu->status | Overflow_Flag;
    else 
        cpu->status = cpu->status & 0b10111111;

    if (acc <= value) cpu_sec(&cpu->status);
    else cpu_clc(&cpu->status);

    cpu->register_a = sum;
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
}

static void cpu_sbc(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    unsigned char   value = cpu->memory[addr] - (!(cpu->status & Carry_Flag)),
                    acc = cpu->register_a,
                    sum = acc - value;

    if (acc >= value) cpu_sec(&cpu->status);
    else cpu_clc(&cpu->status);

    if ((acc ^ sum) & (value ^ sum) & 0x80)
        cpu->status = cpu->status | Overflow_Flag;
    else 
        cpu->status = cpu->status & 0b10111111;

    cpu->register_a = sum;
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
}

static void cpu_and(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    cpu_update_zero_and_negative_flags(
        &cpu->status, cpu->register_a & cpu->memory[addr]);
}

static void cpu_bcc(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    if ((cpu->status & Carry_Flag) == 0)
        cpu->program_counter += (char)cpu->memory[addr];
}

static void cpu_bcs(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    if ((cpu->status & Carry_Flag) != 0)
        cpu->program_counter += (char)cpu->memory[addr];
}

static void cpu_beq(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    if ((cpu->status & Zero_Flag) != 0)
        cpu->program_counter += (char)cpu->memory[addr];
}

static void cpu_bne(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    if ((cpu->status & Zero_Flag) == 0)
        cpu->program_counter += (char)cpu->memory[addr];
}

static void cpu_bit(CPU *cpu, enum AddressingMode mode)
{
    unsigned short  addr = cpu_get_operand_address(cpu, mode);

    unsigned char   b6 = cpu->memory[addr] | Overflow_Flag,
                    b7 = cpu->memory[addr] | Negative_Flag,
                    and = cpu->register_a & cpu->memory[addr];

    if (and == 0) cpu->status = cpu->status | Zero_Flag;
    else cpu->status = cpu->status & 0b11111101;

    cpu->status = cpu->status | b6;
    cpu->status = cpu->status | b7;
}

static void cpu_bmi(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    if ((cpu->status & Negative_Flag) != 0)
        cpu->program_counter += (char)cpu->memory[addr];
}

static void cpu_bpl(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    if ((cpu->status & Negative_Flag) == 0)
        cpu->program_counter += (char)cpu->memory[addr];
}

static void cpu_bvc(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    if ((cpu->status & Overflow_Flag) == 0)
        cpu->program_counter += (char)cpu->memory[addr];
}

static void cpu_bvs(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    if ((cpu->status & Overflow_Flag) != 0)
        cpu->program_counter += (char)cpu->memory[addr];
}

static void cpu_cmp(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    unsigned char result = cpu->register_a - cpu->memory[addr];

    if (cpu->register_a >= cpu->memory[addr])
        cpu_sec(&cpu->status);
    else 
        cpu_clc(&cpu->status);

    if (cpu->register_a == cpu->memory[addr]) 
        cpu_zero_set(&cpu->status);
    else 
        cpu_zero_clear(&cpu->status);

    if (result & 0b10000000) 
        cpu->status = cpu->status | 0b10000000;
    else 
        cpu->status = cpu->status & 0b01111111;
}

static void cpu_cpx(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    unsigned char result = cpu->register_x - cpu->memory[addr];

    if (cpu->register_x >= cpu->memory[addr])
        cpu_sec(&cpu->status);
    else 
        cpu_clc(&cpu->status);

    if (cpu->register_x == cpu->memory[addr]) 
        cpu_zero_set(&cpu->status);
    else 
        cpu_zero_clear(&cpu->status);

    if (result & 0b10000000) 
        cpu->status = cpu->status | 0b10000000;
    else 
        cpu->status = cpu->status & 0b01111111;
}

static void cpu_cpy(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    unsigned char result = cpu->register_y - cpu->memory[addr];

    if (cpu->register_y >= cpu->memory[addr])
        cpu_sec(&cpu->status);
    else 
        cpu_clc(&cpu->status);

    if (cpu->register_y == cpu->memory[addr]) 
        cpu_zero_set(&cpu->status);
    else 
        cpu_zero_clear(&cpu->status);

    if (result & 0b10000000) 
        cpu->status = cpu->status | 0b10000000;
    else 
        cpu->status = cpu->status & 0b01111111;
}

static void cpu_dec(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    cpu->memory[addr] -= 1;
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->memory[addr]);
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
    cpu->memory[cpu->stack_pointer] = cpu->program_counter;
    cpu->stack_pointer -= 1;
    cpu->memory[cpu->stack_pointer] = cpu->status;
    cpu->stack_pointer -= 1;
    cpu->program_counter = cpu->memory[0xFFFE];
    cpu->status = cpu->status | Break_Command_Flag;
}

static void cpu_eor(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    cpu_update_zero_and_negative_flags(
        &cpu->status, cpu->register_a ^ cpu->memory[addr]);
}

static void cpu_ora(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    cpu_update_zero_and_negative_flags(
        &cpu->status, cpu->register_a | cpu->memory[addr]);
}

static void cpu_nop()
{

}

static void cpu_inc(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    cpu->memory[addr] += 1;
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->memory[addr]);
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
    cpu->program_counter = cpu->memory[addr];
}

static void cpu_lda(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    unsigned char value = cpu->memory[addr];
    
    cpu->register_a = value;
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
}

static void cpu_ldx(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    cpu->register_x = cpu->memory[addr];
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_x);
}

static void cpu_ldy(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    cpu->register_y = cpu->memory[addr];
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_y);
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
    cpu->memory[addr] = cpu->register_a;
}

static void cpu_stx(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    cpu->memory[addr] = cpu->register_x;
}

static void cpu_sty(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    cpu->memory[addr] = cpu->register_y;
}

static void cpu_tsx(CPU *cpu)
{
    cpu->register_x = cpu->memory[cpu->stack_pointer];
    cpu_update_zero_and_negative_flags(cpu->status, cpu->register_a);
}

static void cpu_txs(CPU *cpu)
{
    cpu->memory[cpu->stack_pointer] = cpu->register_x;
}

static void cpu_pha(CPU *cpu)
{
    cpu->memory[cpu->stack_pointer] = cpu->register_a;
    cpu->stack_pointer -= 1;
}

static void cpu_php(CPU *cpu)
{
    cpu->memory[cpu->stack_pointer] = cpu->status;
    cpu->stack_pointer -= 1;
}

static void cpu_pla(CPU *cpu)
{
    cpu->register_a = cpu->memory[cpu->stack_pointer];
    cpu->stack_pointer += 1;
    cpu_update_zero_and_negative_flags(cpu->status, cpu->register_a);
}

static void cpu_plp(CPU *cpu)
{
    cpu->status = cpu->memory[cpu->stack_pointer];
    cpu->stack_pointer += 1;
}

static void cpu_rol(CPU *cpu, enum AddressingMode mode)
{
    if (mode == Accumulator)
    {
        unsigned char   old_bit = cpu->register_a,
                        shift = cpu->register_a << 1,
                        rotate = cpu->register_a >> 7;

        cpu->register_a = shift | rotate;
        cpu->register_a = cpu->register_a | (cpu->status | 0b00000001);

        if (old_bit & 0b10000000) 
            cpu->status = cpu->status | Carry_Flag;
        else 
            cpu->status = cpu->status & 0b11111110;

        cpu_update_zero_and_negative_flags(cpu->status, cpu->register_a);
    }
    else
    {
        unsigned short addr = cpu_get_operand_address(cpu, mode);

        unsigned char   old_bit = cpu->memory[addr],
                        shift = cpu->memory[addr] << 1,
                        rotate = cpu->memory[addr] >> 7;

        cpu->memory[addr] = shift | rotate;
        cpu->memory[addr] = cpu->memory[addr] | (cpu->status | 0b00000001);

        if (old_bit & 0b10000000) 
            cpu->status = cpu->status | Carry_Flag;
        else 
            cpu->status = cpu->status & 0b11111110;

        cpu_update_zero_and_negative_flags(cpu->status, cpu->memory[addr]);
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

        cpu_update_zero_and_negative_flags(cpu->status, cpu->register_a);
    }
    else
    {
        unsigned short addr = cpu_get_operand_address(cpu, mode);

        unsigned char   old_bit = cpu->memory[addr],
                        shift = cpu->memory[addr] >> 1,
                        rotate = cpu->memory[addr] << 7;

        cpu->memory[addr] = shift | rotate;

        if ((cpu->status & 0b00000001) != 0) 
            cpu->memory[addr] = cpu->memory[addr] | 0b10000000;
        else 
            cpu->memory[addr] = cpu->memory[addr] & 0b01111111;

        if (old_bit & 0b00000001) 
            cpu->status = cpu->status | Carry_Flag;
        else 
            cpu->status = cpu->status & 0b11111110;

        cpu_update_zero_and_negative_flags(cpu->status, cpu->memory[addr]);
    }
}

static void cpu_asl(CPU *cpu, enum AddressingMode mode)
{
    if (mode == Accumulator)
    {
        unsigned char old_bit = cpu->register_a;

        cpu->register_a <<= 1;
        cpu->register_a = cpu->register_a & 0b11111110;

        if ((old_bit & 0b10000000) != 0)
            cpu->status = cpu->status | 0b10000000;
        else 
            cpu->status = cpu->status & 0b01111111;

        cpu_update_zero_and_negative_flags(cpu->status, cpu->register_a);
    }
    else
    {
        unsigned short addr = cpu_get_operand_address(cpu, mode);

        unsigned char old_bit = cpu->memory[addr];

        cpu->memory[addr] <<= 1;
        cpu->memory[addr] = cpu->memory[addr] & 0b11111110;

        if ((old_bit & 0b10000000) != 0)
            cpu->status = cpu->status | 0b10000000;
        else 
            cpu->status = cpu->status & 0b01111111;

        cpu_update_zero_and_negative_flags(cpu->status, cpu->memory[addr]);
    }
}

static void cpu_lsr(CPU *cpu, enum AddressingMode mode)
{
    if (mode == Accumulator)
    {
        unsigned char old_bit = cpu->register_a;

        cpu->register_a >>= 1;
        cpu->register_a = cpu->register_a & 0b011111111;

        if ((old_bit & 0b00000001) != 0) 
            cpu->status = cpu->status | 0b00000001;
        else 
            cpu->status = cpu->status & 0b11111110;

        cpu_update_zero_and_negative_flags(cpu->status, cpu->register_a);
    }
    else
    {
        unsigned short addr = cpu_get_operand_address(cpu, mode);

        unsigned char old_bit = cpu->memory[addr];

        cpu->memory[addr] >>= 1;
        cpu->memory[addr] = cpu->memory[addr] & 0b011111111;

        cpu_update_zero_and_negative_flags(cpu->status, cpu->memory[addr]);
    }
}

static void cpu_rti(CPU *cpu)
{
    // pull status followed by counter
    cpu->status = cpu->memory[cpu->stack_pointer];
    cpu->stack_pointer += 1;
    cpu->program_counter = cpu->memory[cpu->stack_pointer];
    cpu->stack_pointer += 1;
}

static void cpu_rts(CPU *cpu)
{
    cpu->program_counter = cpu->memory[cpu->stack_pointer] - 1;
    cpu->stack_pointer += 1;
}

static void cpu_jsr(CPU *cpu)
{
    unsigned short addr = cpu_get_operand_address(cpu, Absolute);
    cpu->memory[cpu->stack_pointer] = addr - 1;
    cpu->stack_pointer -= 1;
    cpu->program_counter = cpu->memory[addr];
}

static void cpu_interpret(CPU *cpu, unsigned char program[])
{
    int len = strlen((char*)program); 
    memcpy(&cpu->memory[0x8000], program, len);
    cpu_mem_write_u16(cpu, 0xFFFC, 0x8000);

    while (1)
    {
        unsigned char opscode = program[cpu->program_counter];
        cpu->program_counter++;

        switch (opscode)
        {
            case 0x0B:
            case 0x2B:
                cpu_aac(cpu, Immediate);
                cpu->program_counter += 1;
                break;
            case 0x87:
                cpu_aax(cpu, Zero_Page);
                cpu->program_counter += 1;
                break;
            case 0x97:
                cpu_aax(cpu, Zero_Page_Y);
                cpu->program_counter += 1;
                break;
            case 0x83:
                cpu_aax(cpu, Indirect_X);
                cpu->program_counter += 1;
                break;
            case 0x8F:
                cpu_aax(cpu, Absolute);
                cpu->program_counter += 2;
                break;
            case 0x6B:
                cpu_arr(cpu, Immediate);
                cpu->program_counter += 1;
                break;
            case 0x4B:
                cpu_asr(cpu, Immediate);
                cpu->program_counter += 1;
                break;
            case 0xAB:
                cpu_atx(cpu, Immediate);
                cpu->program_counter += 1;
                break;
            case 0x93:
                cpu_axa(cpu, Indirect_Y);
                cpu->program_counter += 1;
                break;
            case 0x9F:
                cpu_axa(cpu, Absolute);
                cpu->program_counter += 2;
                break;
            case 0xCB:
                cpu_axs(cpu, Immediate);
                cpu->program_counter += 1;
                break;

            // dcp next

            /*  adc start   */
            case 0x69:
                cpu_adc(cpu, Immediate);
                cpu->program_counter += 1;
                break;
            case 0x65:
                cpu_adc(cpu, Zero_Page);
                cpu->program_counter += 1;
                break;
            case 0x75:
                cpu_adc(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                break;
            case 0x6D:
                cpu_adc(cpu, Absolute);
                cpu->program_counter += 2;
                break;
            case 0x7D:
                cpu_adc(cpu, Absolute_X);
                cpu->program_counter += 2;
                break;
            case 0x79:
                cpu_adc(cpu, Absolute_Y);
                cpu->program_counter += 2;
                break;
            case 0x61:
                cpu_adc(cpu, Indirect_X);
                cpu->program_counter += 1;
                break;
            case 0x71:
                cpu_adc(cpu, Indirect_Y);
                cpu->program_counter += 1;
                break;
            /*  adc end     */
            case 0xE9:
                cpu_sbc(cpu, Immediate);
                cpu->program_counter += 1;
                break;
            case 0xE5:
                cpu_sbc(cpu, Zero_Page);
                cpu->program_counter += 1;
                break;
            case 0xF5:
                cpu_sbc(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                break;
            case 0xED:
                cpu_sbc(cpu, Absolute);
                cpu->program_counter += 2;
                break;
            case 0xFD:
                cpu_sbc(cpu, Absolute_X);
                cpu->program_counter += 2;
                break;
            case 0xF9:
                cpu_sbc(cpu, Absolute_Y);
                cpu->program_counter += 2;
                break;
            case 0xE1:
                cpu_sbc(cpu, Indirect_X);
                cpu->program_counter += 1;
                break;
            case 0xF1:
                cpu_sbc(cpu, Indirect_Y);
                cpu->program_counter += 1;
                break;
            case 0x29: 
                cpu_and(cpu, Immediate);
                cpu->program_counter += 1;
                break;
            case 0x25: 
                cpu_and(cpu, Zero_Page);
                cpu->program_counter += 1;
                break;
            case 0x35: 
                cpu_and(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                break;
            case 0x2D: 
                cpu_and(cpu, Absolute);
                cpu->program_counter += 2;
                break;
            case 0x3D: 
                cpu_and(cpu, Absolute_X);
                cpu->program_counter += 2;
                break;
            case 0x39: 
                cpu_and(cpu, Absolute_Y);
                cpu->program_counter += 2;
                break;
            case 0x21: 
                cpu_and(cpu, Indirect_X);
                cpu->program_counter += 1;
                break;
            case 0x31: 
                cpu_and(cpu, Indirect_Y);
                cpu->program_counter += 1;
                break;
            case 0x49:
                cpu_eor(cpu, Immediate);
                cpu->program_counter += 1;
                break;
            case 0x45:
                cpu_eor(cpu, Zero_Page);
                cpu->program_counter += 1;
                break;
            case 0x55:
                cpu_eor(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                break;
            case 0x4D:
                cpu_eor(cpu, Absolute);
                cpu->program_counter += 2;
                break;
            case 0x5D:
                cpu_eor(cpu, Absolute_X);
                cpu->program_counter += 2;
                break;
            case 0x59:
                cpu_eor(cpu, Absolute_Y);
                cpu->program_counter += 2;
                break;
            case 0x41:
                cpu_eor(cpu, Indirect_X);
                cpu->program_counter += 1;
                break;
            case 0x51:
                cpu_eor(cpu, Indirect_Y);
                cpu->program_counter += 1;
                break;
            case 0x09:
                cpu_ora(cpu, Immediate);
                cpu->program_counter += 1;
                break;
            case 0x05:
                cpu_ora(cpu, Zero_Page);
                cpu->program_counter += 1;
                break;
            case 0x15:
                cpu_ora(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                break;
            case 0x0D:
                cpu_ora(cpu, Absolute);
                cpu->program_counter += 2;
                break;
            case 0x1D:
                cpu_ora(cpu, Absolute_X);
                cpu->program_counter += 2;
                break;
            case 0x19:
                cpu_ora(cpu, Absolute_Y);
                cpu->program_counter += 2;
                break;
            case 0x01:
                cpu_ora(cpu, Indirect_X);
                cpu->program_counter += 1;
                break;
            case 0x11:
                cpu_ora(cpu, Indirect_Y);
                cpu->program_counter += 1;
                break;
            case 0x58: cpu_cli(&cpu->status);
                break;
            case 0x78: cpu_sei(&cpu->status);
                break;
            /*      STA begin   */
            case 0x85:
                cpu_sta(cpu, Zero_Page);
                cpu->program_counter += 1;
                break;
            case 0x95:
                cpu_sta(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                break;
            case 0x8D:
                cpu_sta(cpu, Absolute);
                cpu->program_counter += 2;
                break;
            case 0x9D:
                cpu_sta(cpu, Absolute_X);
                cpu->program_counter += 2;
                break;
            case 0x99:
                cpu_sta(cpu, Absolute_Y);
                cpu->program_counter += 2;
                break;
            case 0x81:
                cpu_sta(cpu, Indirect_X);
                cpu->program_counter += 1;
                break;
            case 0x91:
                cpu_sta(cpu, Indirect_Y);
                cpu->program_counter += 1;
                break;
            /*      STA end     */
            case 0x86:
                cpu_stx(cpu, Zero_Page);
                cpu->program_counter += 1;
                break;
            case 0x96:
                cpu_stx(cpu, Zero_Page_Y);
                cpu->program_counter += 1;
                break;
            case 0x8E:
                cpu_stx(cpu, Absolute);
                cpu->program_counter += 2;
                break;
            case 0x84:
                cpu_sty(cpu, Zero_Page);
                cpu->program_counter += 1;
                break;
            case 0x94:
                cpu_sty(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                break;
            case 0x8C:
                cpu_sty(cpu, Absolute);
                cpu->program_counter += 2;
                break;
            case 0xA9: 
                cpu_lda(cpu, Immediate); 
                cpu->program_counter += 1; 
                break;
            case 0xA5:
                cpu_lda(cpu, Zero_Page);
                cpu->program_counter += 1;
                break;
            case 0xB5:
                cpu_lda(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                break;
            case 0xAD:
                cpu_lda(cpu, Absolute);
                cpu->program_counter += 2;
                break;
            case 0xBD:
                cpu_lda(cpu, Absolute_X);
                cpu->program_counter += 2;
                break;
            case 0xB9:
                cpu_lda(cpu, Absolute_Y);
                cpu->program_counter += 2;
                break;
            case 0xA1:
                cpu_lda(cpu, Indirect_X);
                cpu->program_counter += 1;
                break;
            case 0xB1:
                cpu_lda(cpu, Indirect_Y);
                cpu->program_counter += 1;
                break;
            case 0xAA: cpu_tax(cpu);
                break;
            case 0x8A: cpu_txa(cpu);
                break;
            case 0xA8: cpu_tay(cpu);
                break;
            case 0x98: cpu_tya(cpu);
                break;
            case 0xE6: 
                cpu_inc(cpu, Zero_Page);
                cpu->program_counter += 1;
                break;
            case 0xF6: 
                cpu_inc(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                break;
            case 0xEE: 
                cpu_inc(cpu, Absolute);
                cpu->program_counter += 2;
                break;
            case 0xFE: 
                cpu_inc(cpu, Absolute_X);
                cpu->program_counter += 2;
                break;
            case 0xC9:
                cpu_cmp(cpu, Immediate);
                cpu->program_counter += 1;
                break;
            case 0xC5:
                cpu_cmp(cpu, Zero_Page);
                cpu->program_counter += 1;
                break;
            case 0xD5:
                cpu_cmp(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                break;
            case 0xCD:
                cpu_cmp(cpu, Absolute);
                cpu->program_counter += 2;
                break;
            case 0xDD:
                cpu_cmp(cpu, Absolute_X);
                cpu->program_counter += 2;
                break;
            case 0xD9:
                cpu_cmp(cpu, Absolute_Y);
                cpu->program_counter += 2;
                break;
            case 0xC1:
                cpu_cmp(cpu, Indirect_X);
                cpu->program_counter += 1;
                break;
            case 0xD1:
                cpu_cmp(cpu, Indirect_Y);
                cpu->program_counter += 1;
                break;
            case 0xE0:
                cpu_cpx(cpu, Immediate);
                cpu->program_counter += 1;
                break;
            case 0xE4:
                cpu_cpx(cpu, Zero_Page);
                cpu->program_counter += 1;
                break;
            case 0xEC:
                cpu_cpx(cpu, Absolute);
                cpu->program_counter += 2;
                break;
            case 0xC0:
                cpu_cpy(cpu, Immediate);
                cpu->program_counter += 1;
                break;
            case 0xC4:
                cpu_cpy(cpu, Zero_Page);
                cpu->program_counter += 1;
                break;
            case 0xCC:
                cpu_cpy(cpu, Absolute);
                cpu->program_counter += 2;
                break;
            case 0xC6:
                cpu_dec(cpu, Zero_Page);
                cpu->program_counter += 1;
                break;
            case 0xD6:
                cpu_dec(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                break;
            case 0xCE:
                cpu_dec(cpu, Absolute);
                cpu->program_counter += 2;
                break;
            case 0xDE:
                cpu_dec(cpu, Absolute_X);
                cpu->program_counter += 2;
                break;
            case 0x4C: 
                cpu_jmp(cpu, Absolute);
                cpu->program_counter += 2;
                break;
            case 0x6C:
                cpu_jmp(cpu, Indirect);
                cpu->program_counter += 2;
                break;
            case 0xCA: cpu_dex(cpu);
                break;
            case 0x88: cpu_dey(cpu);
                break;
            case 0xC8: cpu_iny(cpu);
                break;
            case 0xE8: cpu_inx(cpu);
                break;
            case 0x18: cpu_clc(&cpu->status);
                break;
            case 0x38: cpu_sec(&cpu->status);
                break;
            case 0xB8: cpu_clv(&cpu->status);
                break;
            case 0xD8: cpu_cld(&cpu->status);
                break;
            case 0xF8: cpu_sed(&cpu->status);
                break;
            case 0x0A: cpu_asl(cpu, Accumulator);
                break;
            case 0x06:
                cpu_asl(cpu, Zero_Page);
                cpu->program_counter += 1;
                break;
            case 0x16:
                cpu_asl(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                break;
            case 0x0E:
                cpu_asl(cpu, Absolute);
                cpu->program_counter += 2;
                break;
            case 0x1E:
                cpu_asl(cpu, Absolute_X);
                cpu->program_counter += 2;
                break;
            case 0x4A: cpu_lsr(cpu, Accumulator);
                break;
            case 0x46:
                cpu_lsr(cpu, Zero_Page);
                cpu->program_counter += 1;
                break;
            case 0x56:
                cpu_lsr(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                break;
            case 0x4E:
                cpu_lsr(cpu, Absolute);
                cpu->program_counter += 2;
                break;
            case 0x5A:
                cpu_lsr(cpu, Absolute_X);
                cpu->program_counter += 2;
                break;
            case 0x2A: cpu_rol(cpu, Accumulator);
                break;
            case 0x26:
                cpu_rol(cpu, Zero_Page);
                cpu->program_counter += 1;
                break;
            case 0x36:
                cpu_rol(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                break;
            case 0x2E:
                cpu_rol(cpu, Absolute);
                cpu->program_counter += 2;
                break;
            case 0x3E:
                cpu_rol(cpu, Absolute_X);
                cpu->program_counter += 2;
                break;
            case 0x6A:
                cpu_ror(cpu, Accumulator);
                break;
            case 0x66:
                cpu_ror(cpu, Zero_Page);
                cpu->program_counter += 1;
                break;
            case 0x76:
                cpu_ror(cpu, Zero_Page_X);
                cpu->program_counter += 1;
                break;
            case 0x6E:
                cpu_ror(cpu, Absolute);
                cpu->program_counter += 2;
                break;
            case 0x7E:
                cpu_ror(cpu, Absolute_X);
                cpu->program_counter += 2;
                break;
            case 0x90:
                cpu_bcc(cpu, Immediate);
                cpu->program_counter += 1;
                break;
            case 0xB0:
                cpu_bcs(cpu, Immediate);
                cpu->program_counter += 1;
                break;
            case 0xF0:
                cpu_beq(cpu, Immediate);
                cpu->program_counter += 1;
                break;
            case 0xD0:
                cpu_bne(cpu, Immediate);
                cpu->program_counter += 1;
                break;
            case 0x24:
                cpu_bit(cpu, Zero_Page);
                cpu->program_counter += 1;
                break;
            case 0x2C:
                cpu_bit(cpu, Absolute);
                cpu->program_counter += 2;
                break;
            case 0x30:
                cpu_bmi(cpu, Immediate);
                cpu->program_counter += 1;
                break;
            case 0x10:
                cpu_bpl(cpu, Immediate);
                cpu->program_counter += 1;
                break;
            case 0x50:
                cpu_bvc(cpu, Immediate);
                cpu->program_counter += 1;
                break;
            case 0x70:
                cpu_bvs(cpu, Immediate);
                cpu->program_counter += 1;
                break;
            case 0x20:
                cpu_jsr(cpu);
                cpu->program_counter += 2;
                break;
            case 0x60: cpu_rts(cpu);
                break;
            case 0xEA:
            case 0x1A:
            case 0x3A:
            case 0x7A:
            case 0xDA:
            case 0xFA:
                cpu_nop();
                break;
            case 0x00: cpu_brk(cpu);
                break;
        }
    }
}

static void e_file_handler(unsigned char *buffer, int len)
{
    printf("hello from emulator file handler!\n");
}
