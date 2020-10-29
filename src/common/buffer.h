#ifndef _MINIM_BUFFER_H_
#define _MINIM_BUFFER_H_

#include <stddef.h>

#define MINIM_BUFFER_DEFAULT_SIZE       256
#define MINIM_BUFFER_STEP_SIZE          256
#define MINIM_BUFFER_MAX_SIZE           (1 << 31)

typedef struct Buffer
{
    char *data;
    size_t pos;
    size_t curr;
} Buffer;

void init_buffer(Buffer **pbf);
void copy_buffer(Buffer **pbf, Buffer *src);
void free_buffer(Buffer *bf);

void trim_buffer(Buffer *bf);
void clear_buffer(Buffer *bf);
void reset_buffer(Buffer *bf);

void write_buffer(Buffer *bf, void *data, size_t len);
void writes_buffer(Buffer *bf, char *str);
void writec_buffer(Buffer *bf, char c);
void writei_buffer(Buffer *bf, long l);
void writed_buffer(Buffer *bf, double d);
void writeb_buffer(Buffer *bf, Buffer *src);
void writef_buffer(Buffer *bf, const char *format, ...);

char *get_buffer(Buffer *bf);
char *release_buffer(Buffer *bf);


#endif