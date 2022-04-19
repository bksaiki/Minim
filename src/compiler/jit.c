#include "jit.h"

#define INIT_ENV_ADDR       (&init_env)
#define SET_SYM_ADDR        (&jit_set_sym)
#define GET_SYM_ADDR        (&jit_get_sym)
#define QUOTE_ADDR          (&syntax_unwrap_rec)

uintptr_t resolve_address(MinimEnv *env, MinimObject *addr)
{
    MinimObject *obj;

    if (minim_eqp(addr, intern("init_env")))        return ((uintptr_t) INIT_ENV_ADDR);
    else if (minim_eqp(addr, intern("quote")))      return ((uintptr_t) QUOTE_ADDR);
    else if (minim_eqp(addr, intern("set_sym")))    return ((uintptr_t) SET_SYM_ADDR);
    else if (minim_eqp(addr, intern("get_sym")))    return ((uintptr_t) GET_SYM_ADDR);
    
    obj = env_get_sym(env, MINIM_SYMBOL(addr));
    if (obj == NULL) {
        THROW(env, minim_error("could not resolve address: ~s", "resolve_address()",
                                MINIM_SYMBOL(addr)));
    }

    return ((uintptr_t) obj);
}

void add_call_site(Function *func, MinimObject *name, uintptr_t loc)
{
    func->calls = minim_cons(
        minim_cons(name, uint_to_minim_number(loc)),
        minim_null);
}
