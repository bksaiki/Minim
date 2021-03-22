#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "buffer.h"

// *** Initialization / Deleting *** //

void init_buffer(Buffer **pbf)
{
    Buffer *bf = malloc(sizeof(Buffer));

    bf->data = malloc(MINIM_BUFFER_DEFAULT_SIZE * sizeof(char));
    bf->curr = MINIM_BUFFER_DEFAULT_SIZE;
    bf->pos = 0;
    bf->data[0] = '\0';

    *pbf = bf;
}

void copy_buffer(Buffer **pbf, Buffer *src)
{
    Buffer *bf = malloc(sizeof(Buffer));
    
    bf->data = malloc(src->curr * sizeof(char));
    bf->curr = src->curr;
    bf->pos = src->pos;
    memcpy(bf->data, src->data, src->pos + 1);

    *pbf = bf;
}

void free_buffer(Buffer *bf)
{
    if (bf->data) free(bf->data);
    free(bf);
}

// *** Resizing *** //

static void resize_buffer(Buffer *bf, size_t size)
{
    if (size >= bf->curr)
    {
        bf->curr = size + MINIM_BUFFER_STEP_SIZE;
        bf->data = realloc(bf->data, bf->curr * sizeof(char));
    }
}

void trim_buffer(Buffer *bf)
{
    if (bf->pos != bf->curr)
    {
        bf->data = realloc(bf->data, bf->pos + 1);
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
    bf->data = realloc(bf->data, MINIM_BUFFER_DEFAULT_SIZE * sizeof(char));
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
    bool dot = false;

    resize_buffer(bf, bf->pos + 25);
    len = sprintf(bf->data + bf->pos, "%.16g", d);

    for (size_t i = 0; i < len; ++i)
    {
        if (*(bf->data + bf->pos + i) == '.')
            dot = true;
    }

    if (!dot)
    {
        sprintf(bf->data + bf->pos + len, ".0");
        len += 2;
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

// *** Getters *** //

char *get_buffer(Buffer *bf)
{
    return bf->data;
}

char *release_buffer(Buffer *bf)
{
    char *str = bf->data;
    bf->data = NULL;
    return str;
}