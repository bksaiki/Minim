#include <setjmp.h>
#include <string.h>

#include "../gc/gc.h"
#include "env.h"
#include "error.h"
#include "global.h"
#include "hash.h"
#include "lambda.h"
#include "list.h"
#include "number.h"
#include "object.h"
#include "parser.h"
#include "sequence.h"
#include "tail_call.h"
#include "vector.h"

// Handlers

MinimObject *minim_error_handler = NULL;
MinimObject *minim_exit_handler = NULL;
MinimObject *minim_error_port = NULL;
MinimObject *minim_output_port = NULL;
MinimObject *minim_input_port = NULL;

// Destructors

static void GC_minim_port_dtor(void *ptr)
{
    MinimObject *obj;
    
    obj = (MinimObject*) ptr;
    if ((MINIM_PORT_MODE(obj) & MINIM_PORT_MODE_OPEN) &&
        !(MINIM_PORT_MODE(obj) & MINIM_PORT_MODE_NO_FREE))
    {
        if (MINIM_PORT_TYPE(obj) == MINIM_PORT_TYPE_FILE)
            fclose(MINIM_PORT_FILE(obj));
    }
}

// Visible functions

MinimObject *minim_exactnum(void *num)
{
    MinimObject *o = GC_alloc(minim_exactnum_size);
    o->type = MINIM_OBJ_EXACT;
    MINIM_EXACTNUM(o) = num;
    log_obj_created();
    return o;
}

MinimObject *minim_inexactnum(double num)
{
    MinimObject *o = GC_alloc(minim_inexactnum_size);
    o->type = MINIM_OBJ_INEXACT;
    MINIM_INEXACTNUM(o) = num;
    log_obj_created();
    return o;
}

MinimObject *minim_symbol(char *sym)
{
    MinimObject *o = GC_alloc(minim_symbol_size);
    o->type = MINIM_OBJ_SYM;
    MINIM_SYMBOL_SET_INTERNED(o, 0);
    MINIM_SYMBOL(o) = sym;
    log_obj_created();
    return o;
}

MinimObject *minim_string(char *str)
{
    MinimObject *o = GC_alloc(minim_string_size);
    o->type = MINIM_OBJ_STRING;
    MINIM_STRING(o) = str;
    log_obj_created();
    return o;
}

MinimObject *minim_cons(void *car, void *cdr)
{
    MinimObject *o = GC_alloc(minim_cons_size);
    o->type = MINIM_OBJ_PAIR;
    MINIM_CAR(o) = car;
    MINIM_CDR(o) = cdr;
    log_obj_created();
    return o;
}

MinimObject *minim_vector(size_t len, void *arr)
{
    MinimObject *o = GC_alloc(minim_vector_size);
    o->type = MINIM_OBJ_VECTOR;
    MINIM_VECTOR(o) = arr;
    MINIM_VECTOR_LEN(o) = len;
    log_obj_created();
    return o;
}

MinimObject *minim_hash_table(void *ht)
{
    MinimObject *o = GC_alloc(minim_hash_table_size);
    o->type = MINIM_OBJ_HASH;
    MINIM_HASH_TABLE(o) = ht;
    log_obj_created();
    return o;
}

MinimObject *minim_promise(void *val, void *env)
{
    MinimObject *o = GC_alloc(minim_promise_size);
    o->type = MINIM_OBJ_PROMISE;
    MINIM_PROMISE_VAL(o) = val;
    MINIM_PROMISE_ENV(o) = env;
    MINIM_PROMISE_SET_STATE(o, 0);
    log_obj_created();
    return o;
}

MinimObject *minim_builtin(void *func)
{
    MinimObject *o = GC_alloc(minim_builtin_size);
    o->type = MINIM_OBJ_FUNC;
    MINIM_BUILTIN(o) = func;
    log_obj_created();
    return o;
}

MinimObject *minim_syntax(void *func)
{
    MinimObject *o = GC_alloc(minim_syntax_size);
    o->type = MINIM_OBJ_SYNTAX;
    MINIM_SYNTAX(o) = func;
    log_obj_created();
    return o;
}

MinimObject *minim_closure(void *closure)
{
    MinimObject *o = GC_alloc(minim_closure_size);
    o->type = MINIM_OBJ_CLOSURE;
    MINIM_CLOSURE(o) = closure;
    log_obj_created();
    return o;
}

MinimObject *minim_tail_call(void *tc)
{
    MinimObject *o = GC_alloc(minim_tail_call_size);
    o->type = MINIM_OBJ_TAIL_CALL;
    MINIM_TAIL_CALL(o) = tc;
    log_obj_created();
    return o;
}

MinimObject *minim_transform(void *binding, int type)
{
    MinimObject *o = GC_alloc(minim_transform_size);
    o->type = MINIM_OBJ_TRANSFORM;
    MINIM_TRANSFORM_TYPE(o) = (uint8_t) type & 0xFF;
    MINIM_TRANSFORMER(o) = binding;
    log_obj_created();
    return o;
}

MinimObject *minim_ast(void *ast)
{
    MinimObject *o = GC_alloc(minim_ast_size);
    o->type = MINIM_OBJ_AST;
    MINIM_AST(o) = ast;
    log_obj_created();
    return o;
}

MinimObject *minim_sequence(void *seq)
{
    MinimObject *o = GC_alloc(minim_sequence_size);
    o->type = MINIM_OBJ_SEQ;
    MINIM_SEQUENCE(o) = seq;
    log_obj_created();
    return o;
}

MinimObject *minim_values(size_t len, void *arr)
{
    MinimObject *o = GC_alloc(minim_values_size);
    o->type = MINIM_OBJ_VALUES;
    MINIM_VALUES(o) = arr;
    MINIM_VALUES_SIZE(o) = len;
    log_obj_created();
    return o;
}

MinimObject *minim_err(void *err)
{
    MinimObject *o = GC_alloc(minim_error_size);
    o->type = MINIM_OBJ_ERR;
    MINIM_ERROR(o) = err;
    log_obj_created();
    return o;
}

MinimObject *minim_char(unsigned int ch)
{
    MinimObject *o = GC_alloc(minim_char_size);
    o->type = MINIM_OBJ_CHAR;
    MINIM_CHAR(o) = ch;
    log_obj_created();
    return o;
}

MinimObject *minim_jmp(void *ptr, void *val)
{
    MinimObject *o = GC_alloc(minim_jump_size);
    o->type = MINIM_OBJ_JUMP;
    MINIM_JUMP_BUF(o) = ptr;
    MINIM_JUMP_VAL(o) = val;
    log_obj_created();
    return o;
}

MinimObject *minim_file_port(FILE *f, uint8_t mode)
{
    MinimObject *o = GC_alloc_opt(minim_port_size, GC_minim_port_dtor, NULL);
    o->type = MINIM_OBJ_PORT;
    MINIM_PORT_TYPE(o) = MINIM_PORT_TYPE_FILE;
    MINIM_PORT_MODE(o) = mode;
    MINIM_PORT_FILE(o) = f;
    MINIM_PORT_NAME(o) = NULL;
    MINIM_PORT_ROW(o) = 1;
    MINIM_PORT_COL(o) = 0;
    MINIM_PORT_POS(o) = 0;
    log_obj_created();
    return o;
}

bool minim_eqp(MinimObject *a, MinimObject *b)
{
    if (a == b)                         // same object
        return true;

    if (minim_voidp(a) ||               // special objects
        minim_truep(a) ||
        minim_falsep(a) ||
        minim_booleanp(a) ||
        minim_nullp(a) ||
        minim_eofp(a))
        return false;

    if (!MINIM_OBJ_TYPE_EQP(a, b))      // different type
        return false;

    switch(a->type)
    {
    case MINIM_OBJ_EXACT:
    case MINIM_OBJ_INEXACT:
        return minim_number_cmp(a, b) == 0;

    case MINIM_OBJ_CHAR:
        return MINIM_CHAR(a) == MINIM_CHAR(b);

    case MINIM_OBJ_VECTOR:
        return MINIM_VECTOR_LEN(a) == 0 &&
               MINIM_VECTOR_LEN(b) == 0;

    case MINIM_OBJ_HASH:
        return MINIM_HASH_TABLE(a)->elems == 0 &&
               MINIM_HASH_TABLE(b)->elems == 0;

    /*
    case MINIM_OBJ_SYM:
    case MINIM_OBJ_STRING:
    case MINIM_OBJ_PAIR:
    case MINIM_OBJ_VECTOR:
    case MINIM_OBJ_HASH:
    case MINIM_OBJ_PROMISE:
    case MINIM_OBJ_CLOSURE:
    case MINIM_OBJ_FUNC:
    case MINIM_OBJ_SYNTAX:
    case MINIM_OBJ_AST
    case MINIM_OBJ_SEQ:
    case MINIM_OBJ_TAIL_CALL:
    case MINIM_OBJ_TRANSFORM:
    case MINIM_OBJ_VALUES:
    */
    default:
        return false;
    }
}

// Currently eq? and eqv? are identical
bool minim_eqvp(MinimObject *a, MinimObject *b)
{
    if (minim_eqp(a, b))
        return true;

    return false;
}

bool minim_equalp(MinimObject *a, MinimObject *b)
{
    if (minim_eqvp(a, b))
        return true;

    if (minim_voidp(a) ||               // special objects
        minim_truep(a) ||
        minim_falsep(a) ||
        minim_booleanp(a) ||
        minim_nullp(a) ||
        minim_eofp(a))
        return false;

    if (MINIM_OBJ_NUMBERP(a) && MINIM_OBJ_NUMBERP(b))   // multiple number types
        return minim_number_cmp(a, b) == 0;

    if (!MINIM_OBJ_TYPE_EQP(a, b))      // early exit, different type
        return false;

    switch (a->type)
    {      
    case MINIM_OBJ_STRING:
        return strcmp(MINIM_STRING(a), MINIM_STRING(b)) == 0;

    case MINIM_OBJ_PAIR:
        return minim_equalp(MINIM_CAR(a), MINIM_CAR(b)) &&
               minim_equalp(MINIM_CDR(a), MINIM_CDR(b));

    case MINIM_OBJ_VECTOR: {
        if (MINIM_VECTOR_LEN(a) != MINIM_VECTOR_LEN(b))
            return false;
        for (size_t i = 0; i < MINIM_VECTOR_LEN(a); ++i)
        {
            if (!minim_equalp(MINIM_VECTOR_REF(a, i), MINIM_VECTOR_REF(b, i)))
                return false;
        }
        return true;
    }

    case MINIM_OBJ_HASH:    // TODO: equality for hash tables
        return minim_hash_table_eqp(MINIM_HASH_TABLE(a), MINIM_HASH_TABLE(b));

    case MINIM_OBJ_AST:
        return ast_equalp(MINIM_AST(a), MINIM_AST(b));

    /*
    case MINIM_OBJ_SYM:
    case MINIM_OBJ_CHAR:

    case MINIM_OBJ_FUNC:
    case MINIM_OBJ_SYNTAX:
    case MINIM_OBJ_AST:
    case MINIM_OBJ_PROMISE:
    case MINIM_OBJ_CLOSURE:
    case MINIM_OBJ_EXIT:
    case MINIM_OBJ_SEQ:
    case MINIM_OBJ_TAIL_CALL:
    case MINIM_OBJ_TRANSFORM:
    case MINIM_OBJ_ERR:
    case MINIM_OBJ_VALUES:
    */
    default:
        return false;
    }
}

Buffer* minim_obj_to_bytes(MinimObject *obj)
{
    Buffer *bf, *bf2;
    
    init_buffer(&bf);
    if (minim_booleanp(obj))
    {
        writec_buffer(bf, (minim_truep(obj) ? '1' : '0'));
        return bf;
    }
    else if (minim_voidp(obj))
    {
        writes_buffer(bf, "void");
        return bf;
    }
    else if (minim_nullp(obj))
    {
        writes_buffer(bf, "'()");
    }


    switch (obj->type)
    {
    case MINIM_OBJ_FUNC:
        writeu_buffer(bf, (size_t) MINIM_BUILTIN(obj));
        break;

    case MINIM_OBJ_SYNTAX:
        writeu_buffer(bf, (size_t) MINIM_SYNTAX(obj));
        break;

    case MINIM_OBJ_SYM:
        writes_buffer(bf, MINIM_SYMBOL(obj));
        break;

    case MINIM_OBJ_CHAR:
        writec_buffer(bf, MINIM_CHAR(obj));
        break;

    case MINIM_OBJ_STRING:
        writes_buffer(bf, MINIM_STRING(obj));
        break;

    case MINIM_OBJ_PAIR:
        minim_cons_to_bytes(obj, bf);
        break;

    // Dump integer limbs
    case MINIM_OBJ_EXACT:
        write_buffer(bf, MINIM_EXACTNUM(obj)->_mp_num._mp_d,
                     abs(MINIM_EXACTNUM(obj)->_mp_num._mp_size) * sizeof(mp_limb_t));
        write_buffer(bf, MINIM_EXACTNUM(obj)->_mp_den._mp_d,
                     abs(MINIM_EXACTNUM(obj)->_mp_den._mp_size) * sizeof(mp_limb_t));
        break;

    case MINIM_OBJ_INEXACT:
        write_buffer(bf, &MINIM_INEXACTNUM(obj), sizeof(double));
        break;

    case MINIM_OBJ_AST:
        ast_dump_in_buffer(MINIM_AST(obj), bf);
        break;

    case MINIM_OBJ_CLOSURE:
        minim_lambda_to_buffer(MINIM_CLOSURE(obj), bf);
        break;

    case MINIM_OBJ_VECTOR:
        minim_vector_bytes(obj, bf);
        break;

    case MINIM_OBJ_PROMISE:
        bf2 = minim_obj_to_bytes(MINIM_CAR(obj));
        writeb_buffer(bf, bf2);
        break;

    /*
    case MINIM_OBJ_EXIT:
    case MINIM_OBJ_SEQ:
    case MINIM_OBJ_TAIL_CALL:
    case MINIM_OBJ_TRANSFORM:
    case MINIM_OBJ_ERR:
    */
    default:
        writeu_buffer(bf, obj->type);
        break;
    }

    return bf;
}
