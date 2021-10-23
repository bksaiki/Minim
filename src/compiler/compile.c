#include "compile.h"

void compile_expr(MinimEnv *env, SyntaxNode *stx, Buffer *bf)
{
    
}

Buffer *compile_module(MinimEnv *env, MinimModule *module)
{
    Buffer *bf;

    init_buffer(&bf);
#if defined(MINIM_LINUX)    // only enabled for linux
    for (size_t i = 0; i < module->exprc; ++i)
        compile_expr(env, module->exprs[i], bf);
#endif
    return bf;
}
