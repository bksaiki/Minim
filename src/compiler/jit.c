#include "jit.h"

#define INIT_ENV_ADDR       (&init_env)
#define SET_SYM_ADDR        (&jit_set_sym)
#define QUOTE_ADDR          (&syntax_unwrap_rec)

MinimObject *jit_get_sym(MinimEnv *env, MinimObject *sym)
{
    return env_get_sym(env, MINIM_SYMBOL(sym));
}

uintptr_t resolve_address(MinimEnv *env, MinimObject *stx)
{
    MinimObject *addr_str, *obj;
    
    addr_str = MINIM_STX_VAL(MINIM_STX_CADR(stx));
    if (minim_eqp(addr_str, intern("init_env")))        return ((uintptr_t) INIT_ENV_ADDR);
    else if (minim_eqp(addr_str, intern("quote")))      return ((uintptr_t) QUOTE_ADDR);
    else if (minim_eqp(addr_str, intern("set_sym")))    return ((uintptr_t) SET_SYM_ADDR);
    
    obj = env_get_sym(env, MINIM_SYMBOL(addr_str));
    if (obj == NULL) {
        THROW(env, minim_error("could not resolve address: ~s", "resolve_address()",
                                MINIM_SYMBOL(addr_str)));
    }

    return ((uintptr_t) obj);
}
