#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../gc/gc.h"
#include "buffer.h"
#include "common.h"

// *** Initialization //

static void gc_buffer_mrk(void (*mrk)(void*, void*), void *gc, void *ptr)
{
    mrk(gc, ((Buffer*) ptr)->data);
}

void init_buffer(Buffer **pbf)
{
    Buffer *bf = GC_alloc_opt(sizeof(Buffer), NULL, gc_buffer_mrk);

    bf->data = GC_alloc_atomic(MINIM_BUFFER_DEFAULT_SIZE * sizeof(char));
    bf->curr = MINIM_BUFFER_DEFAULT_SIZE;
    bf->pos = 0;
    bf->data[0] = '\0';

    *pbf = bf;
}

// *** Resizing *** //

static void resize_buffer(Buffer *bf, size_t size)
{
    if (size >= bf->curr)
    {
        bf->curr = size + MINIM_BUFFER_STEP_SIZE;
        bf->data = GC_realloc_atomic(bf->data, bf->curr * sizeof(char));
    }
}

void trim_buffer(Buffer *bf)
{
    if (bf->pos != bf->curr)
    {
        bf->data = GC_realloc_atomic(bf->data, bf->pos + 1);
        bf->curr = bf->pos + 1;
    }
}

// *** Erasure *** //

void clear_buffer(Buffer *bf)
{
    bf->data[0] = '\0';
    bf->pos = 0;
}

void reset_buffer(Buffer *bf)
{
    bf->data = GC_realloc_atomic(bf->data, MINIM_BUFFER_DEFAULT_SIZE * sizeof(char));
    bf->data[0] = '\0';
    bf->pos = 0;
}

// *** Writing *** //

void write_buffer(Buffer *bf, const void *data, size_t len)
{
    resize_buffer(bf, bf->pos + len);
    memcpy(bf->data + bf->pos, data, len);
    bf->pos += len;
    bf->data[bf->pos] = '\0';
}

void writes_buffer(Buffer *bf, const char *str)
{
    write_buffer(bf, &str[0], strlen(str));
}

void writec_buffer(Buffer *bf, char c)
{
    resize_buffer(bf, bf->pos + 1);
    bf->data[bf->pos] = c;
    bf->data[++bf->pos] = '\0';
}

void writei_buffer(Buffer *bf, long l)
{
    size_t len;

    resize_buffer(bf, bf->pos + 20);
    len = sprintf(bf->data + bf->pos, "%ld", l);
    bf->pos += len;
}

void writeu_buffer(Buffer *bf, unsigned long ul)
{
    size_t len;

    resize_buffer(bf, bf->pos + 21);
    len = sprintf(bf->data + bf->pos, "%lu", ul);
    bf->pos += len;
}

void writed_buffer(Buffer *bf, double d)
{
    size_t len;

    resize_buffer(bf, bf->pos + 25);
    len = sprintf(&bf->data[bf->pos], "%.16g", d);
    if (!isinf(d) && !isnan(d))
    {
        bool dot = false;

        for (size_t i = bf->pos; bf->data[i]; ++i)
        {
            if (bf->data[i] == '.' || bf->data[i] == 'e')
            {
                dot = true;
                break;
            }
        }

        if (!dot)
        {
            sprintf(&bf->data[bf->pos + len], ".0");
            len += 2;
        }
    }

    bf->pos += len;
}

void writeb_buffer(Buffer *bf, Buffer *src)
{
    resize_buffer(bf, bf->pos + src->pos);
    memcpy(bf->data + bf->pos, src->data, src->pos + 1);
    bf->pos += src->pos;
}

void writesn_buffer(Buffer *bf, const char *str, size_t len)
{
    resize_buffer(bf, bf->pos + len);
    strncpy(bf->data + bf->pos, str, len);
    bf->pos += len;
    bf->data[bf->pos] = '\0';
}

// *** Format *** //

void writef_buffer(Buffer *bf, const char *format, ...)
{
    va_list rest;

    va_start(rest, format);
    vwritef_buffer(bf, format, rest);
    va_end(rest);
}

void vwritef_buffer(Buffer *bf, const char* str, va_list va)
{
    for (size_t i = 0; str[i]; ++i)
    {
        if (str[i] == '~')
        {
            ++i;
            if (str[i] == '~')          writec_buffer(bf, '~');
            else if (str[i] == 'c')     writec_buffer(bf, (char) va_arg(va, int));
            else if (str[i] == 'i')     writei_buffer(bf, va_arg(va, long));
            else if (str[i] == 'u')     writeu_buffer(bf, va_arg(va, unsigned long));
            else if (str[i] == 'l')     writei_buffer(bf, va_arg(va, long));
            else if (str[i] == 's')     writes_buffer(bf, va_arg(va, char*));
            else if (str[i] == 'f')     writed_buffer(bf, va_arg(va, double));
            else if (str[i] == 'B')     writeb_buffer(bf, va_arg(va, Buffer*));
            else                        writec_buffer(bf, str[i + 1]); 
        }
        else
        {
            writec_buffer(bf, str[i]);
        }
    }
}
