#include "minimpriv.h"

// Stores custom print methods
//  => ((pred . method) ...)
MinimObject **custom_print_methods = NULL;
size_t custom_print_method_count = 0;

//
//  Printing
//

static int print_object(MinimObject *obj, MinimEnv *env, Buffer *bf, PrintParams *pp)
{
    if (env)
    {
        for (size_t i = custom_print_method_count - 1; i < custom_print_method_count; --i)
        {
            if (coerce_into_bool(eval_lambda(MINIM_CLOSURE(MINIM_CAR(custom_print_methods[i])), env, 1, &obj)))
            {
                MinimObject *str = eval_lambda(MINIM_CLOSURE(MINIM_CDR(custom_print_methods[i])), env, 1, &obj);
                if (!MINIM_OBJ_STRINGP(str))
                    THROW(env, minim_error("expected a string from the custom printer", NULL));
                    
                writes_buffer(bf, MINIM_STRING(str));
                return 1;
            }
        }
    }

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
    else if (minim_eofp(obj))
    {
        writes_buffer(bf, "<eof>");
    }
    else if (MINIM_OBJ_CHARP(obj))
    {
        if (!pp->display)   writes_buffer(bf, "#\\");
        writec_buffer(bf, MINIM_CHAR(obj));
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
            else if (trace->loc->src && MINIM_OBJ_SYMBOLP(trace->loc->src))
            {
                writef_buffer(bf, "\n;    ~s:~u:~u", MINIM_SYMBOL(trace->loc->src),
                                                     trace->loc->row,
                                                     trace->loc->col);

                if (trace->name)
                    writef_buffer(bf, " ~s", trace->name);
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
        const char *key;
        
        if (env)
        {
            key = env_peek_key(env, obj);
            if (key)    writef_buffer(bf, "<function:~s>", key);
            else        writes_buffer(bf, "<function:?>");
        }
        else
        {
            writes_buffer(bf, "<function:?>");
        }
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
        for (size_t i = 0; i < ht->alloc; ++i)
        {
            for (MinimHashBucket *b = ht->buckets[i]; b; b = b->next)
            {
                writes_buffer(bf, (first ? "(" : " ("));
                print_object(b->key, env, bf, pp);
                writes_buffer(bf, " . ");
                print_object(b->val, env, bf, pp);
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
        if (pp->syntax && pp->quote)
        {
            print_object(MINIM_STX_VAL(obj), env, bf, pp);
        }
        else
        {
            bool syntaxp = pp->syntax;
            bool quotep = pp->quote;

            pp->syntax = true;
            pp->quote = true;
            writes_buffer(bf, "<syntax:");
            print_object(MINIM_STX_VAL(obj), env, bf, pp);
            writec_buffer(bf, '>');
            pp->syntax = syntaxp;
            pp->quote = quotep;
        }
    }
    else if (MINIM_OBJ_PROMISEP(obj))
    {
        bool syntaxp = pp->syntax;
        bool quotep = pp->quote;

        pp->syntax = true;
        pp->quote = true;

        writes_buffer(bf, "<promise");
        writec_buffer(bf, MINIM_PROMISE_STATE(obj) ? '!' : ':');
        print_object(MINIM_PROMISE_VAL(obj), env, bf, pp);
        writec_buffer(bf, '>');

        pp->syntax = syntaxp;
        pp->quote = quotep;
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
    else if (MINIM_OBJ_PORTP(obj))
    {
        writes_buffer(bf, "<port>");
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

void set_print_params(PrintParams *pp,
                      size_t maxlen,
                      bool quote,
                      bool display,
                      bool syntax)
{
    pp->maxlen = maxlen;
    pp->quote = quote;
    pp->display = display;
    pp->syntax = syntax;
}

void debug_print_minim_object(MinimObject *obj, MinimEnv *env)
{
    PrintParams pp;

    set_default_print_params(&pp);
    print_to_port(obj, env, &pp, stdout);
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

int print_syntax_to_buffer(Buffer *bf, MinimObject *stx)
{
    PrintParams pp;

    set_syntax_print_params(&pp);
    return print_object(stx, NULL, bf, &pp);
}

int print_syntax_to_port(MinimObject *stx, FILE *f)
{
    Buffer *bf;
    int status;

    init_buffer(&bf);
    status = print_syntax_to_buffer(bf, stx);
    fputs(bf->data, f);

    return status;
}

//
//  Builtins
//

MinimObject *minim_builtin_def_print_method(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimArity arity;
    MinimLambda *pred, *print;

    if (!MINIM_OBJ_CLOSUREP(args[0]))
        THROW(env, minim_argument_error("procedure of one argument", "def-print-method", 0, args[0]));

    if (!MINIM_OBJ_CLOSUREP(args[1]))
        THROW(env, minim_argument_error("procedure of one argument", "def-print-method", 1, args[1]));

    pred = MINIM_CLOSURE(args[0]);
    minim_get_lambda_arity(pred, &arity);
    if (arity.low < 1)
        THROW(env, minim_argument_error("procedure of one argument", "def-print-method", 0, args[0]));

    print = MINIM_CLOSURE(args[1]);
    minim_get_lambda_arity(print, &arity);
    if (arity.low < 1)
        THROW(env, minim_argument_error("procedure of one argument", "def-print-method", 1, args[1]));

    if (custom_print_method_count == 0)
    {
        custom_print_methods = GC_alloc(sizeof(MinimObject*));
        custom_print_method_count = 1;
        custom_print_methods[0] = minim_cons(args[0], args[1]);
        GC_register_root(custom_print_methods);
    }
    else
    {
        for (size_t i = 0; i < custom_print_method_count; ++i)
        {
            if (MINIM_CLOSURE(MINIM_CAR(custom_print_methods[i])) == pred)
            {
                MINIM_CDR(custom_print_methods[i]) = args[1];
                return minim_void;
            }
        }

        ++custom_print_method_count;
        custom_print_methods = GC_realloc(custom_print_methods,
                                          custom_print_method_count * sizeof(MinimObject*));
        custom_print_methods[custom_print_method_count - 1] = minim_cons(args[0], args[1]);
    }

    return minim_void;
}
