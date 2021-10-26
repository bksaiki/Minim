#include "../core/builtin.h"
#include "compilepriv.h"

size_t top_level_def_count(MinimEnv *env, MinimModule *module)
{
    MinimObject *op;
    size_t top_vars = 0;

    for (size_t i = 0; i < module->exprc; ++i)
    {
        // definitely syntax or function
        if (module->exprs[i]->type == SYNTAX_NODE_LIST &&
            module->exprs[i]->childc > 0)
        {
            op = env_get_sym(env, module->exprs[i]->children[0]->sym);
            if (op && MINIM_OBJ_SYNTAXP(op) &&
                MINIM_SYNTAX(op) == minim_builtin_def_values)
            {
                top_vars += module->exprs[i]->children[1]->childc;
            }
        }
    }
    
    return top_vars;
}
