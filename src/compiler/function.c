#include "../core/minimpriv.h"
#include "compilepriv.h"

void init_function(Function **pfunc)
{
    Function *func = GC_alloc(sizeof(Function));
    func->code = NULL;
    func->name = NULL;
    func->argc = 0;
    func->variary = true;

    *pfunc = func;
}

void compiler_add_function(Compiler *compiler, Function *func)
{
    compiler->func_count += 1;
    compiler->funcs = GC_realloc(compiler->funcs, compiler->func_count * sizeof(Function*));
    compiler->funcs[compiler->func_count - 1] = func;
}
