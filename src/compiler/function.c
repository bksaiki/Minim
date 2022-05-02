#include "../core/minimpriv.h"
#include "compilepriv.h"

void init_function(Function **pfunc)
{
    Function *func = GC_alloc(sizeof(Function));
    func->pseudo = minim_null;
    func->pseudo_it = minim_null;
    func->stash = minim_null;
    func->calls = minim_null;
    init_buffer(&func->code_buf);
    func->code = NULL;
    func->name = NULL;
    func->start = 0;
    func->argc = 0;

    *pfunc = func;
}

void function_add_line(Function *func, MinimObject *instr)
{
    if (minim_nullp(func->pseudo))
    {
        func->pseudo = minim_cons(instr, minim_null);
        func->pseudo_it = func->pseudo;
    }
    else
    {
        MINIM_CDR(func->pseudo_it) = minim_cons(instr, minim_null);
        func->pseudo_it = MINIM_CDR(func->pseudo_it);
    }
}

void debug_function(MinimEnv *env, Function *func)
{
    printf("  function %s:\n", func->name);
    if (func->pseudo) {
        for (MinimObject *it = func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
        {
            printf("   ");
            print_syntax_to_port(MINIM_CAR(it), stdout);
            printf("\n");
        }
    }
}

void compiler_add_function(Compiler *compiler, Function *func)
{
    ++compiler->func_count;
    compiler->funcs = GC_realloc(compiler->funcs, compiler->func_count * sizeof(Function*));
    compiler->funcs[compiler->func_count - 1] = func;
}

Function *compiler_get_function(Compiler *compiler, MinimObject *name)
{
    for (size_t i = 0; i < compiler->func_count; ++i)
    {
        if (strcmp(compiler->funcs[i]->name, MINIM_SYMBOL(name)) == 0)
            return compiler->funcs[i];
    }

    return NULL;
}

bool is_argument_location(MinimObject *obj)
{
    return (MINIM_OBJ_SYMBOLP(obj) && MINIM_SYMBOL(obj)[0] == 'a');
}
