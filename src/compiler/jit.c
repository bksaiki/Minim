#include "../core/minimpriv.h"
#include "compilepriv.h"

#define INIT_ENV_ADDR       (&init_env)
#define QUOTE_ADDR          (&syntax_unwrap_rec)

uintptr_t resolve_address(MinimObject *stx)
{
    MinimObject *addr_str = MINIM_STX_VAL(MINIM_STX_CADR(stx));
    if (minim_eqp(addr_str, intern("init_env")))
    {
        return ((uintptr_t) INIT_ENV_ADDR);
    }
    else if (minim_eqp(addr_str, intern("quote")))
    {
        return ((uintptr_t) QUOTE_ADDR);
    }
    else
    {
        return 0;
    }
}
