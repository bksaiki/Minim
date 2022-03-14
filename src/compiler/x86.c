#include "../core/minimpriv.h"
#include "compilepriv.h"
#include "jit.h"

static void
assemble_move_immediate(Buffer *bf,
                        MinimEnv *env,
                        MinimObject *target,
                        uintptr_t addr)
{
    if (minim_eqp(target, intern(REG_RT_STR)))
        writes_buffer(bf, "\x48\xB8");
    else if (minim_eqp(target, intern(REG_R0_STR)))
        writes_buffer(bf, "\x48\xBE");
    else
        THROW(env, minim_error("invalid instruction", "compiler"));

    writeu_buffer(bf, addr);
}

static void
assemble_move(Buffer *bf,
              MinimEnv *env,
              MinimObject *target,
              MinimObject *src)
{
    writes_buffer(bf, "\x48");  // 64-bit mode
    writes_buffer(bf, "\x89");  // MOV
    if (minim_eqp(target, intern(REG_TC_STR)) && minim_eqp(src, intern(REG_RT_STR)))
        writes_buffer(bf, "\xC7");
    else
        THROW(env, minim_error("invalid instruction", "compiler"));
}

static void
assemble_call(Buffer *bf,
              MinimEnv *env,
              MinimObject *target)
{
    if (minim_eqp(target, intern(REG_RT_STR)))
        writes_buffer(bf, "\xFF\xD0");
    else
        THROW(env, minim_error("invalid instruction", "compiler"));
}

void function_assemble_x86(MinimEnv *env, Function *func, Buffer *bf)
{
    MinimObject *line, *op;

    // function header
    //  push rbp
    //  mov rbp, rsp
    writes_buffer(bf, "\x55\x48\x89\xE5");

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
                    uintptr_t addr = resolve_address(val);
                    if (addr == 0)
                    {
                        THROW(env, minim_error("invalid instruction", "compiler"));
                    }

                    assemble_move_immediate(bf, env, target, addr);
                }
                else if (minim_eqp(MINIM_STX_VAL(MINIM_STX_CAR(val)), intern("$syntax")))
                {
                    uintptr_t addr = ((uintptr_t) MINIM_STX_CADR(val));
                    assemble_move_immediate(bf, env, target, addr);
                }
                else
                {
                    THROW(env, minim_error("invalid instruction", "compiler"));
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
            writes_buffer(bf, "\xC3");
        }
    }
}
