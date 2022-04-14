#include "jit.h"

// translation

#define REG_RAX     REG_RT
#define REG_RDI     REG_TC
#define REG_RSI     REG_R0
#define REG_RDX     REG_R1
#define REG_RCX     REG_R2
#define REG_RBX     REG_T0
#define REG_R12     REG_T1
#define REG_R13     REG_T2
#define REG_R14     REG_T3

#define REG_RAX_STR     REG_RT_STR
#define REG_RDI_STR     REG_TC_STR
#define REG_RSI_STR     REG_R0_STR
#define REG_RDX_STR     REG_R1_STR
#define REG_RCX_STR     REG_R2_STR
#define REG_RBX_STR     REG_T0_STR
#define REG_R12_STR     REG_T1_STR
#define REG_R13_STR     REG_T2_STR
#define REG_R14_STR     REG_T3_STR

#define REX(dst, src)       (0x40 | (((src) & 0x1) << 2) | ((dst) & 0x1))
#define REX_W(dst, src)     (0x48 | (((src) & 0x1) << 2) | ((dst) & 0x1))
#define MOV(dst, src)       (0xC0 | (((src) & 0x7) << 3) | ((dst) & 0x7))
#define MOVI(dst)           (0xB8 | ((dst) & 0x7))
#define CALL(tgt)           (0xD0 | ((tgt) & 0x7))

static bool
extended_reg(MinimObject *reg)
{
    return (minim_eqp(reg, intern(REG_R12_STR)) ||
            minim_eqp(reg, intern(REG_R13_STR)) ||
            minim_eqp(reg, intern(REG_R14_STR)));
}

static uint8_t
register_operand(MinimEnv *env, MinimObject *target)
{
    if (minim_eqp(target, intern(REG_RAX_STR)))         return 0;
    else if (minim_eqp(target, intern(REG_RCX_STR)))    return 1;
    else if (minim_eqp(target, intern(REG_RDX_STR)))    return 2;
    else if (minim_eqp(target, intern(REG_RBX_STR)))    return 3;
    else if (minim_eqp(target, intern(REG_RSI_STR)))    return 6;
    else if (minim_eqp(target, intern(REG_RDI_STR)))    return 7;
    else if (minim_eqp(target, intern(REG_R12_STR)))    return 4;
    else if (minim_eqp(target, intern(REG_R13_STR)))    return 5;
    else if (minim_eqp(target, intern(REG_R14_STR)))    return 6;
    else    THROW(env, minim_error("invalid move target ~s", "compiler", MINIM_SYMBOL(target)));
}

static void
assemble_move_immediate(Buffer *bf,
                        MinimEnv *env,
                        MinimObject *target,
                        uintptr_t addr)
{
    uint8_t prefix, op;
    prefix = REX_W(extended_reg(target), 0);
    op = MOVI(register_operand(env, target));

    writec_buffer(bf, prefix);          // REX prefix
    writec_buffer(bf, op);              // op + ModR/M
    write_buffer(bf, &addr, sizeof(uintptr_t));
}

static void
assemble_move(Buffer *bf,
              MinimEnv *env,
              MinimObject *target,
              MinimObject *src)
{
    uint8_t prefix, args;  
    prefix = REX_W(extended_reg(target), extended_reg(src));
    args = MOV(register_operand(env, target), register_operand(env, src));

    writec_buffer(bf, prefix);          // REX prefix
    writec_buffer(bf, '\x89');          // MOV
    writec_buffer(bf, args);            // ModR/M
}

static void
assemble_call(Buffer *bf,
              MinimEnv *env,
              MinimObject *target)
{
    uint8_t prefix, args;
    prefix = REX_W(extended_reg(target), 0);
    args = CALL(register_operand(env, target));

    if (prefix == 0x41)     writec_buffer(bf, '\x41');      // REX prefix (extended only)
    writec_buffer(bf, '\xFF');                              // CALL
    writec_buffer(bf, args);                                // ModR/M
}

static void
allocate_local_vars(MinimEnv *env, MinimObject *stash)
{

}

static void
save_scratch_registers(MinimEnv *env, MinimObject *stash)
{

}

void function_assemble_x86(MinimEnv *env, Function *func, Buffer *bf)
{
    MinimObject *line, *op;

    // function header
    writec_buffer(bf, '\x55');                  //  push rbp
    writes_buffer(bf, "\x48\x89\xE5");          //  mov rbp, rsp
    allocate_local_vars(env, func->stash);      // (allocate enough space on stack)
    save_scratch_registers(env, func->stash);   // (save scratch registers)

    // translate each instruction
    for (MinimObject *it = func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
    {
        line = MINIM_STX_VAL(MINIM_CAR(it));
        op = MINIM_STX_VAL(MINIM_CAR(line));
        if (minim_eqp(op, intern("$mov")))
        {
            MinimObject *target = MINIM_STX_VAL(MINIM_CADR(line));
            MinimObject *val = MINIM_CAR(MINIM_CDDR(line));
            
            if (MINIM_STX_PAIRP(val))
            {
                if (minim_eqp(MINIM_STX_VAL(MINIM_STX_CAR(val)), intern("$addr")))
                {
                    uintptr_t addr = resolve_address(env, val);
                    if (addr == 0)
                        THROW(env, minim_error("invalid instruction", "compiler"));

                    assemble_move_immediate(bf, env, target, addr);
                }
                else if (minim_eqp(MINIM_STX_VAL(MINIM_STX_CAR(val)), intern("$syntax")))
                {
                    uintptr_t addr = ((uintptr_t) MINIM_STX_CADR(val));
                    assemble_move_immediate(bf, env, target, addr);
                }
                else if (minim_eqp(MINIM_STX_VAL(MINIM_STX_CAR(val)), intern("$sym-addr")))
                {
                    uintptr_t addr = ((uintptr_t) MINIM_STX_SYMBOL(MINIM_STX_CADR(val)));
                    assemble_move_immediate(bf, env, target, addr);
                }
                else
                {
                    THROW(env, minim_error("invalid mov instruction", "compiler"));
                }
            }
            else
            {
                assemble_move(bf, env, target, MINIM_STX_VAL(val));
            }
        }
        else if (minim_eqp(op, intern("$call")))
        {
            MinimObject *target = MINIM_STX_VAL(MINIM_CADR(line));
            assemble_call(bf, env, target);
        }
        else if (minim_eqp(op, intern("$ret")))
        {
            writes_buffer(bf, "\x5D\xC3");
        }
        else
        {
            THROW(env, minim_error("invalid instruction", "compiler"));
        }
    }
}
