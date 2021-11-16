#include "../core/builtin.h"
#include "../core/syntax.h"
#include "compilepriv.h"

size_t top_level_def_count(MinimEnv *env, MinimModule *module)
{
    MinimObject *op;
    size_t top_vars = 0;

    for (size_t i = 0; i < module->exprc; ++i)
    {
        // definitely syntax or function
        if (MINIM_STX_PAIRP(module->exprs[i]))
        {
            op = env_get_sym(env, MINIM_STX_SYMBOL(MINIM_STX_CAR(module->exprs[i])));
            if (op && MINIM_OBJ_SYNTAXP(op) &&
                MINIM_SYNTAX(op) == minim_builtin_def_values)
            {
                top_vars += syntax_list_len(MINIM_STX_CADR(module->exprs[i]));
            }
        }
    }
    
    return top_vars;
}
