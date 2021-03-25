#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/buffer.h"
#include "../common/read.h"
#include "error.h"
#include "hash.h"
#include "lambda.h"
#include "list.h"
#include "number.h"
#include "print.h"
#include "vector.h"

//
//  Printing
//

// forward declaration
static int print_object(MinimObject *obj, MinimEnv *env, Buffer *bf, PrintParams *pp);

static void print_list(MinimObject* obj, MinimEnv* env, Buffer *bf, PrintParams *pp)
{
    if (MINIM_CAR(obj))
        print_object(MINIM_CAR(obj), env, bf, pp);

    if (!MINIM_CDR(obj))
    {
        writec_buffer(bf, ')');
    }
    else if (MINIM_CDR(obj)->type == MINIM_OBJ_PAIR)
    {
        writec_buffer(bf, ' ');
        print_list(MINIM_CDR(obj), env, bf, pp);
    }
    else
    {
        writes_buffer(bf, " . ");
        print_object(MINIM_CDR(obj), env, bf, pp);
        writec_buffer(bf, ')');
    }
}

static int print_object(MinimObject *obj, MinimEnv *env, Buffer *bf, PrintParams *pp)
{
    if (MINIM_OBJ_VOIDP(obj))
    {
        writes_buffer(bf, "<void>");
    }
    else if (MINIM_OBJ_BOOLP(obj))
    {
        if (obj->u.ints.i1)   writes_buffer(bf, "true");
        else                  writes_buffer(bf, "false");
    }
    else if (MINIM_OBJ_EXACTP(obj))
    {
        char *str;
        size_t len;

        str = malloc(128 * sizeof(char));
        len = gmp_snprintf(str, 128, "%Qd", MINIM_EXACT(obj));

        if (len >= 128)
        {
            str = malloc((len + 1) * sizeof(char));
            len = gmp_snprintf(str, len, "%Qd", MINIM_EXACT(obj));
        }

        writes_buffer(bf, str);
        free(str);
    }
    else if (MINIM_OBJ_INEXACTP(obj))
    {
        writed_buffer(bf, MINIM_INEXACT(obj));
    }
    else if (MINIM_OBJ_SYMBOLP(obj))
    {
        char *str = obj->u.str.str;
        size_t len = strlen(str);

        if (!pp->quote)
            writec_buffer(bf, '\'');

        for (size_t i = 0; i < len; ++i)
        {
            if (str[i] == ' ')
            {
                writef_buffer(bf, "|~s|", str);
                return 0;
            }
        }

        writes_buffer(bf, str);
    }
    else if (MINIM_OBJ_STRINGP(obj))
    {
        if (pp->display)    writes_buffer(bf, obj->u.str.str);
        else                writef_buffer(bf, "\"~s\"", obj->u.str.str);
    }
    else if (MINIM_OBJ_ERRORP(obj))
    {
        MinimError *err = obj->u.ptrs.p1;

        if (err->where)   writef_buffer(bf, "; ~s: ~s", err->where, err->msg);
        else              writes_buffer(bf, err->msg);

        if (err->table)
        {
            for (size_t i = 0; i < err->table->len; ++i)
            {
                if (err->table->keys[i])
                    writef_buffer(bf, "\n;  ~s: ~s", err->table->keys[i], err->table->vals[i]);
            }
        }
                   
        if (err->top) writes_buffer(bf, "\n;  backtrace:");
        for (MinimErrorTrace *trace = err->top; trace; trace = trace->next)
        {
            if (trace->multiple)
                writef_buffer(bf, "\n;    ...");
            else if (trace->loc->name)
                writef_buffer(bf, "\n;    ~s:~u:~u ~s", trace->loc->name,
                                trace->loc->row, trace->loc->col, trace->name);
            else
                writef_buffer(bf, "\n;    ~s:~u:~u ~s", trace->loc->name,
                                trace->loc->row, trace->loc->col);
        }
    }
    else if (MINIM_OBJ_PAIRP(obj))
    {
        bool quotep = pp->quote;

        if (pp->quote)  writec_buffer(bf, '(');
        else            writes_buffer(bf, "'(");

        pp->quote = true; // push
        if (minim_listp(obj))
        {
            print_list(obj, env, bf, pp);
        }
        else
        {
            print_object(MINIM_CAR(obj), env, bf, pp);
            writes_buffer(bf, " . ");
            print_object(MINIM_CDR(obj), env, bf, pp);
            writec_buffer(bf, ')');
        }

        pp->quote = quotep; // pop
    }
    else if (MINIM_OBJ_BUILTINP(obj) || MINIM_OBJ_SYNTAXP(obj))
    {
        const char *key = env_peek_key(env, obj);

        if (key)    writef_buffer(bf, "<function:~s>", key);
        else        writes_buffer(bf, "<function:?>");
    }
    else if (MINIM_OBJ_CLOSUREP(obj))
    {
        MinimLambda *lam = obj->u.ptrs.p1;

        if (lam->name)    writef_buffer(bf, "<function:~s>", lam->name);
        else              writes_buffer(bf, "<function:?>");
    }
    else if (MINIM_OBJ_SEQP(obj))
    {
        writes_buffer(bf, "<sequence>");
    }
    else if (MINIM_OBJ_HASHP(obj))
    {
        MinimHash *ht = obj->u.ptrs.p1;
        bool first = true;
        bool quotep = pp->quote;
        
        pp->quote = true;
        writes_buffer(bf, "hash(");
        for (size_t i = 0; i < ht->size; ++i)
        {
            for (size_t j = 0; j < ht->arr[i].len; ++j)
            {
                writes_buffer(bf, (first ? "(" : " ("));
                print_object(MINIM_CAR(ht->arr[i].arr[j]), env, bf, pp);
                writes_buffer(bf, " . ");
                print_object(MINIM_CDR(ht->arr[i].arr[j]), env, bf, pp);
                writec_buffer(bf, ')');
                first = false;
            }
        }
        
        writec_buffer(bf, ')');
        pp->quote = quotep;
    }
    else if (MINIM_OBJ_VECTORP(obj))
    {
        bool first = true;
        bool quotep = pp->quote;

        pp->quote = true;
        writes_buffer(bf, "vector(");
        for (size_t i = 0; i < obj->u.vec.len; ++i)
        {
            if (!first)  writec_buffer(bf, ' ');
            print_object(obj->u.vec.arr[i], env, bf, pp);
            first = false;
        }

        writec_buffer(bf, ')');
        pp->quote = quotep;
    }
    else if (MINIM_OBJ_ASTP(obj))
    {
        writes_buffer(bf, "<syntax:");
        ast_to_buffer(obj->u.ptrs.p1, bf);
        writec_buffer(bf, '>');
    }

    return 1;
}

// Visible functions

void set_default_print_params(PrintParams *pp)
{
    pp->maxlen = UINT_MAX;
    pp->quote = false;
    pp->display = false;
}

int print_minim_object(MinimObject *obj, MinimEnv *env, PrintParams *pp)
{
    return print_to_port(obj, env, pp, stdout);
}

int print_to_buffer(Buffer *bf, MinimObject* obj, MinimEnv *env, PrintParams *pp)
{
    return print_object(obj, env, bf, pp);
}

int print_to_port(MinimObject *obj, MinimEnv *env, PrintParams *pp, FILE *stream)
{
    Buffer *bf;
    int status;
    
    init_buffer(&bf);
    status = print_object(obj, env, bf, pp);
    fputs(bf->data, stream);
    free_buffer(bf);

    return status;
}

char *print_to_string(MinimObject *obj, MinimEnv *env, PrintParams *pp)
{
    Buffer* bf;
    char *str;

    init_buffer(&bf);
    print_object(obj, env, bf, pp);
    str = release_buffer(bf);
    free_buffer(bf);

    return str;
}