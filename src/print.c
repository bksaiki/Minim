#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/buffer.h"
#include "lambda.h"
#include "list.h"
#include "number.h"
#include "print.h"

//
//  Printing
//

// forward declaration
static int print_object(MinimObject *obj, MinimEnv *env, Buffer *bf, PrintParams *pp);

static void print_list(MinimObject* obj, MinimEnv* env, Buffer *bf, PrintParams *pp)
{
    MinimObject** pair = ((MinimObject**) obj->data);

    if (pair[0])
        print_object(pair[0], env, bf, pp);

    if (!pair[1])
    {
        writec_buffer(bf, ')');
    }
    else if (pair[1]->type == MINIM_OBJ_PAIR)
    {
        writec_buffer(bf, ' ');
        print_list(pair[1], env, bf, pp);
    }
    else
    {
        writes_buffer(bf, " . ");
        print_object(pair[1], env, bf, pp);
        writec_buffer(bf, ')');
    }
}

static int print_object(MinimObject *obj, MinimEnv *env, Buffer *bf, PrintParams *pp)
{
    if (obj->type == MINIM_OBJ_VOID)
    {
        writes_buffer(bf, "<void>");
    }
    else if (obj->type == MINIM_OBJ_BOOL)
    {
        int b = *((int*) obj->data);

        if (b)  writes_buffer(bf, "true");
        else    writes_buffer(bf, "false");
    }
    else if (obj->type == MINIM_OBJ_NUM)
    {
        char *tmp = minim_number_to_str(obj->data);
        writes_buffer(bf, tmp);
        free(tmp);
    }
    else if (obj->type == MINIM_OBJ_SYM)
    {
        char *str = obj->data;
        size_t len = strlen(str);

        if (!pp->quote)
            writec_buffer(bf, '\'');

        for (int i = 0; i < len; ++i)
        {
            if (str[i] == ' ')
            {
                writef_buffer(bf, "|~s|", str);
                return 0;
            }
        }

        writes_buffer(bf, str);
    }
    else if (obj->type == MINIM_OBJ_STRING)
    {
        if (pp->display)    writes_buffer(bf, obj->data);
        else                writef_buffer(bf, "\"~s\"", obj->data);
    }
    else if (obj->type == MINIM_OBJ_ERR)
    {
        writes_buffer(bf, obj->data);
    }
    else if (obj->type == MINIM_OBJ_PAIR)
    {
        MinimObject **pair = ((MinimObject**) obj->data);

        if (pp->quote)  writec_buffer(bf, '(');
        else            writes_buffer(bf, "'(");
        pp->quote = true;

        if (minim_listp(obj))
        {
            print_list(obj, env, bf, pp);
        }
        else
        {
            print_object(pair[0], env, bf, pp);
            writes_buffer(bf, " . ");
            print_object(pair[1], env, bf, pp);
            writec_buffer(bf, ')');
        }
    }
    else if (obj->type == MINIM_OBJ_FUNC || obj->type == MINIM_OBJ_SYNTAX)
    {
        const char *key = env_peek_key(env, obj);

        if (key)    writef_buffer(bf, "<function:~s>", key);
        else        writes_buffer(bf, "<function:?>");
    }
    else if (obj->type == MINIM_OBJ_CLOSURE)
    {
        MinimLambda *lam = obj->data;

        if (lam->name)    writef_buffer(bf, "<function:~s>", lam->name);
        else              writes_buffer(bf, "<function:?>");
    }
    else if (obj->type == MINIM_OBJ_ITER)
    {
        writes_buffer(bf, "<iterator>");
    }
    else if (obj->type == MINIM_OBJ_SEQ)
    {
        writes_buffer(bf, "<sequence>");
    }
    else
    {
        writes_buffer(bf, "<unknown type>");
        return 0;
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