#include <limits.h>
#include <string.h>

#include "../gc/gc.h"
#include "../common/buffer.h"
#include "../common/path.h"

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

static int print_object(MinimObject *obj, MinimEnv *env, Buffer *bf, PrintParams *pp)
{
    if (minim_voidp(obj))
    {
        writes_buffer(bf, "<void>");
    }
    else if (minim_truep(obj))
    {
        writes_buffer(bf, "#t");
    }
    else if (minim_falsep(obj))
    {
        writes_buffer(bf, "#f");
    }
    else if (minim_nullp(obj))
    {
        if (!pp->quote)  writec_buffer(bf, '\'');
        writes_buffer(bf, "()");
    }
    else if (MINIM_OBJ_EXACTP(obj))
    {
        char *str;
        size_t len;

        str = GC_alloc_atomic(128 * sizeof(char));
        len = gmp_snprintf(str, 128, "%Qd", MINIM_EXACTNUM(obj));

        if (len >= 128)
        {
            str = GC_realloc_atomic(str, (len + 1) * sizeof(char));
            len = gmp_snprintf(str, len, "%Qd", MINIM_EXACTNUM(obj));
        }

        writes_buffer(bf, str);
    }
    else if (MINIM_OBJ_INEXACTP(obj))
    {
        writed_buffer(bf, MINIM_INEXACTNUM(obj));
    }
    else if (MINIM_OBJ_SYMBOLP(obj))
    {
        char *str = MINIM_SYMBOL(obj);
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
        if (pp->display)    writes_buffer(bf, MINIM_STRING(obj));
        else                writef_buffer(bf, "\"~s\"", MINIM_STRING(obj));
    }
    else if (MINIM_OBJ_ERRORP(obj))
    {
        MinimError *err = MINIM_ERROR(obj);

        if (err->where)   writef_buffer(bf, "; ~s: ~s", err->where, err->msg);
        else              writef_buffer(bf, "; ~s", err->msg);

        if (err->table)
        {
            for (size_t i = 0; i < err->table->len; ++i)
            {
                if (err->table->keys[i])
                {
                    if (strcmp(err->table->keys[i], "at") == 0)
                    {
                        size_t idx;

                        writes_buffer(bf, "\n;  at: ");
                        for (idx = 0; err->table->vals[i][idx] && err->table->vals[i][idx] != '\n' &&
                                      idx < MINIM_DEFAULT_ERR_LOC_LEN; ++idx)
                            writec_buffer(bf, err->table->vals[i][idx]);
                        
                        if (idx == MINIM_DEFAULT_ERR_LOC_LEN)
                            writes_buffer(bf, "...");
                    }
                    else
                    {
                        writef_buffer(bf, "\n;  ~s: ~s", err->table->keys[i],
                                                         err->table->vals[i]);
                    }
                }
            }
        }
                   
        if (err->top) writes_buffer(bf, "\n;  backtrace:");
        for (MinimErrorTrace *trace = err->top; trace; trace = trace->next)
        {
            if (trace->multiple)
            {
                writef_buffer(bf, "\n;    ...");
            }
            else if (trace->loc->name)
            {
                writef_buffer(bf, "\n;    ~s:~u:~u", trace->loc->name, trace->loc->row, trace->loc->col);
                if (trace->name) writef_buffer(bf, " ~s", trace->name);
            }
            else
            {
                writef_buffer(bf, "\n;    :~u:~u ~s", trace->loc->row, trace->loc->col);
            }
        }
    }
    else if (MINIM_OBJ_PAIRP(obj))
    {
        bool quotep = pp->quote;

        if (pp->quote)  writec_buffer(bf, '(');
        else            writes_buffer(bf, "'(");

        pp->quote = true; // push
        print_object(MINIM_CAR(obj), env, bf, pp);
        for (MinimObject *it = MINIM_CDR(obj); !minim_nullp(it); it = MINIM_CDR(it))
        {
            if (!MINIM_OBJ_PAIRP(it))   // improper list
            {
                writes_buffer(bf, " . ");
                print_object(it, env, bf, pp);
                break;
            }
            else
            {
                writec_buffer(bf, ' ');
            }

            print_object(MINIM_CAR(it), env, bf, pp); 
        }

        writec_buffer(bf, ')');
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
        MinimLambda *lam = MINIM_CLOSURE(obj);

        if (lam->name)    writef_buffer(bf, "<function:~s>", lam->name);
        else              writes_buffer(bf, "<function>");
    }
    else if (MINIM_OBJ_SEQP(obj))
    {
        writes_buffer(bf, "<sequence>");
    }
    else if (MINIM_OBJ_HASHP(obj))
    {
        MinimHash *ht = MINIM_HASH_TABLE(obj);
        bool first = true;
        bool quotep = pp->quote;
        
        if (pp->quote)  writes_buffer(bf, "#hash(");
        else            writes_buffer(bf, "'#hash(");
        
        pp->quote = true;
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

        if (pp->quote)  writes_buffer(bf, "#(");
        else            writes_buffer(bf, "'#(");

        pp->quote = true;
        for (size_t i = 0; i < MINIM_VECTOR_LEN(obj); ++i)
        {
            if (!first)  writec_buffer(bf, ' ');
            print_object(MINIM_VECTOR_REF(obj, i), env, bf, pp);
            first = false;
        }

        writec_buffer(bf, ')');
        pp->quote = quotep;
    }
    else if (MINIM_OBJ_ASTP(obj))
    {
        if (pp->syntax)
        {
            ast_to_buffer(MINIM_AST(obj), bf);
        }
        else
        {
            bool syntaxp = pp->syntax;

            pp->syntax = true;
            writes_buffer(bf, "<syntax:");
            ast_to_buffer(MINIM_AST(obj), bf);
            writec_buffer(bf, '>');
            pp->syntax = syntaxp;
        }
    }
    else if (MINIM_OBJ_PROMISEP(obj))
    {
        bool syntaxp = pp->syntax;

        pp->syntax = true;
        writes_buffer(bf, "<promise");
        writec_buffer(bf, MINIM_PROMISE_STATE(obj) ? '!' : ':');
        print_object(MINIM_PROMISE_VAL(obj), env, bf, pp);
        writec_buffer(bf, '>');
        pp->syntax = syntaxp;
    }
    else if (MINIM_OBJ_VALUESP(obj))
    {
        if (MINIM_VALUES_SIZE(obj) == 0)
            return 1;

        print_object(MINIM_VALUES_REF(obj, 0), env, bf, pp);
        for (size_t i = 1; i < MINIM_VALUES_SIZE(obj); ++i)
        {
            writec_buffer(bf, '\n');
            print_object(MINIM_VALUES_REF(obj, i), env, bf, pp);
        }
    }
    else
    {
        // case MINIM_OBJ_TAIL_CALL:
        // case MINIM_OBJ_TRANSFORM:
        writes_buffer(bf, "invalid object");
    }

    return 1;
}

// Visible functions

void set_default_print_params(PrintParams *pp)
{
    pp->maxlen = UINT_MAX;
    pp->quote = false;
    pp->display = false;
    pp->syntax = false;
}

void debug_print_minim_object(MinimObject *obj, MinimEnv *env)
{
    PrintParams pp;

    set_default_print_params(&pp);
    print_to_port(obj, env, &pp, stdout);
    fprintf(stdout, "\n");
    fflush(stdout);
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

    return status;
}

char *print_to_string(MinimObject *obj, MinimEnv *env, PrintParams *pp)
{
    Buffer* bf;
    char *str;

    init_buffer(&bf);
    print_object(obj, env, bf, pp);
    str = get_buffer(bf);

    return str;
}
