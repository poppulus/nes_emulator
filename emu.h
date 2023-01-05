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
    Immediate,
    Zero_Page,
    Zero_Page_X,
    Zero_Page_Y,
    Absolute,
    Absolute_X,
    Absolute_Y,
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

static void cpu_clear_carry(unsigned char *cpu_status)
{
    *cpu_status = *cpu_status & 0b11111110;
}

static void cpu_set_carry(unsigned char *status)
{
    *status = *status | Carry_Flag;
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

    cpu->register_a = sum;
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
}

static void cpu_and(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    cpu->register_a = cpu->register_a & cpu->memory[addr];
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
}

static void cpu_bcc(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    if ((cpu->status & Carry_Flag) == 0)
        cpu->program_counter += cpu->memory[addr];
}

static void cpu_bcs(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    if ((cpu->status & Carry_Flag) != 0)
        cpu->program_counter += cpu->memory[addr];
}

static void cpu_beq(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    if ((cpu->status & Zero_Flag) != 0)
        cpu->program_counter += cpu->memory[addr];
}

static void cpu_bne(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    if ((cpu->status & Zero_Flag) == 0)
        cpu->program_counter += cpu->memory[addr];
}

static void cpu_bit(CPU *cpu, enum AddressingMode mode)
{
    unsigned short  addr = cpu_get_operand_address(cpu, mode);

    unsigned char   b6 = cpu->memory[addr] & Overflow_Flag,
                    b7 = cpu->memory[addr] & Negative_Flag,
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
        cpu->program_counter += cpu->memory[addr];
}

static void cpu_bpl(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    if ((cpu->status & Negative_Flag) == 0)
        cpu->program_counter += cpu->memory[addr];
}

static void cpu_bvc(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    if ((cpu->status & Overflow_Flag) == 0)
        cpu->program_counter += cpu->memory[addr];
}

static void cpu_bvc(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    if ((cpu->status & Overflow_Flag) != 0)
        cpu->program_counter += cpu->memory[addr];
}

static void cpu_cmp(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    if (cpu->register_a >= cpu->memory[addr])
        cpu_set_carry(&cpu->status);
    else 
        cpu_clear_carry(&cpu->status);

    if (cpu->register_a == cpu->memory[addr]) 
        cpu->status = cpu->status | Zero_Flag;
    else 
        cpu->status = cpu->status & 0b11111101;
}

static void cpu_cpx(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    if (cpu->register_x >= cpu->memory[addr])
        cpu_set_carry(&cpu->status);
    else 
        cpu_clear_carry(&cpu->status);

    if (cpu->register_x == cpu->memory[addr]) 
        cpu->status = cpu->status | Zero_Flag;
    else 
        cpu->status = cpu->status & 0b11111101;
}

static void cpu_cpy(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);

    if (cpu->register_y >= cpu->memory[addr])
        cpu_set_carry(&cpu->status);
    else 
        cpu_clear_carry(&cpu->status);

    if (cpu->register_y == cpu->memory[addr]) 
        cpu->status = cpu->status | Zero_Flag;
    else 
        cpu->status = cpu->status & 0b11111101;
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

static void cpu_brk(unsigned char *status)
{
    *status = *status | Break_Command_Flag;
}

static void cpu_eor(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    cpu->register_a = cpu->register_a ^ cpu->memory[addr];
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
}

static void cpu_ora(CPU *cpu, enum AddressingMode mode)
{
    unsigned short addr = cpu_get_operand_address(cpu, mode);
    cpu->register_a = cpu->register_a | cpu->memory[addr];
    cpu_update_zero_and_negative_flags(&cpu->status, cpu->register_a);
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
            case 0x18: cpu_clear_carry(&cpu->status);
                break;
            case 0x38: cpu_set_carry(&cpu->status);
                break;
            case 0x69:
                cpu_adc(cpu, Immediate);
                cpu->program_counter += 1;
                break;
            case 0x58: // interrupt flag off
                cpu->status = cpu->status & 0b11111011;
                break;
            case 0x78: // interrupt flag on
                cpu->status = cpu->status | Interrupt_Disable_Flag;
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
            case 0xC8: cpu_iny(cpu);
                break;
            case 0xE8: cpu_inx(cpu);
                break;
            case 0xD8: // decimal flag off
                cpu->status = cpu->status & 0b11110111;
                break;
            case 0xF8: // decimal flag on
                cpu->status = cpu->status | Decimal_Mode_Flag;
                break;
            case 0xEA: cpu_nop();
                break;
            case 0x00: cpu_brk(&cpu->status);
                return;
        }
    }
}

static void e_file_handler(unsigned char *buffer, int len)
{
    printf("hello from emulator file handler!\n");
}
