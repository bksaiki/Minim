#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lambda.h"
#include "list.h"
#include "print.h"

typedef struct PrintBuffer
{
    char *buffer;
    size_t pos;
    size_t max;
    size_t curr;

    bool quote;
} PrintBuffer;

static void init_buffer(PrintBuffer **pb, size_t start, size_t max)
{
    PrintBuffer* bf;

    bf = malloc(sizeof(PrintBuffer));
    bf->buffer = malloc(start * sizeof(char));
    bf->pos = 0;
    bf->max = max;
    bf->curr = start;
    bf->quote = false;
    bf->buffer[0] = '\0';

    *pb = bf;
}

static void free_buffer(PrintBuffer *pb)
{
    if (pb->buffer) 
    {
        free(pb->buffer);
        pb->buffer = NULL;
    }

    free(pb);
}

static void resize_buffer(PrintBuffer *pb, size_t requested)
{
    if (requested > pb->max)
    {
        pb->buffer = realloc(pb->buffer, pb->max);
        pb->curr = pb->max;
    }
    else if (requested > pb->curr)
    {
        pb->buffer = realloc(pb->buffer, requested);
        pb->curr = pb->max;
    }
    else
    {
        printf("Requested a smaller buffer!!\n");
    }
}

//
//  Printing
//

static int print_object(MinimObject *obj, MinimEnv *env, PrintBuffer *pb);

static void print_to_buffer(PrintBuffer *pb, const char *str)
{
    size_t len = strlen(str);

    if (pb->pos + len >= pb->curr)
        resize_buffer(pb, pb->curr * 2);

    strcat(pb->buffer, str);
    pb->pos += len;
}

static void print_list(MinimObject* obj, MinimEnv* env, PrintBuffer *pb)
{
    MinimObject** pair = ((MinimObject**) obj->data);

    if (pair[0])
    {
        pb->quote = true;
        print_object(pair[0], env, pb);
    }

    if (pair[1])
    {
        pb->quote = true;
        print_to_buffer(pb, " ");
        print_list(pair[1], env, pb);
    }
    else
    {
        print_to_buffer(pb, ")");
    }
}

static int print_object(MinimObject *obj, MinimEnv *env, PrintBuffer *pb)
{
    char str[2048];

    if (obj->type == MINIM_OBJ_VOID)
    {
        print_to_buffer(pb, "<void>");
    }
    else if (obj->type == MINIM_OBJ_BOOL)
    {
        print_to_buffer(pb, ((*((int*) obj->data)) ? "true" : "false"));
    }
    else if (obj->type == MINIM_OBJ_NUM)
    {
        sprintf(str, "%d", *((int*)obj->data));
        print_to_buffer(pb, str);
    }
    else if (obj->type == MINIM_OBJ_SYM)
    {
        if (pb->quote)  sprintf(str, "%s", ((char*) obj->data));
        else            sprintf(str, "'%s", ((char*) obj->data));
        print_to_buffer(pb, str);
    }
    else if (obj->type == MINIM_OBJ_ERR)
    {
        sprintf(str, "%s", ((char*) obj->data));
        print_to_buffer(pb, str);
    }
    else if (obj->type == MINIM_OBJ_PAIR)
    {
        MinimObject **pair = ((MinimObject**) obj->data);

        if (minim_listp(obj))
        {
            print_to_buffer(pb, ((pb->quote) ? "(" : "'("));
            pb->quote = true;
            print_list(obj, env, pb);
        }
        else
        {
            print_to_buffer(pb, ((pb->quote) ? "(" : "'("));
            pb->quote = true;
            print_object(pair[0], env, pb);
            print_to_buffer(pb, " . ");
            print_object(pair[1], env, pb);
            print_to_buffer(pb, ")");
        }
    }
    else if (obj->type == MINIM_OBJ_FUNC)
    {
        const char *key = env_peek_key(env, obj);

        if (key)    sprintf(str, "<function:%s>", key);
        else        sprintf(str, "<function:?>");
        print_to_buffer(pb, str);
    }
    else if (obj->type == MINIM_OBJ_CLOSURE)
    {
        MinimLambda *lam = obj->data;

        if (lam->name)      sprintf(str, "<function:%s>", lam->name);
        else                sprintf(str, "<function:?>");
        print_to_buffer(pb, str);
    }
    else
    {
        print_to_buffer(pb, "<Unknown type>");
        return 1;
    }

    return 0;
}

// Visible functions

int print_minim_object(MinimObject *obj, MinimEnv *env, size_t maxlen)
{
    return print_to_port(obj, env, maxlen, stdout);
}

// Writes a string representation of the object to stream.
int print_to_port(MinimObject *obj, MinimEnv *env, size_t maxlen, FILE *stream)
{
    PrintBuffer *pb;
    int status;

    init_buffer(&pb, 2048, maxlen);
    status = print_object(obj, env, pb);
    fputs(pb->buffer, stream);
    free_buffer(pb);

    return status;
}

char *print_to_string(MinimObject *obj, MinimEnv *env, size_t maxlen)
{
    PrintBuffer *pb;
    char *str;

    init_buffer(&pb, 2048, maxlen);
    print_object(obj, env, pb);

    str = malloc((pb->pos + 1) * sizeof(char));
    strcpy(str, pb->buffer);
    free_buffer(pb);

    return str;
}