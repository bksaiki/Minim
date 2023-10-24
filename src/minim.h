// minim.h: main header file
//
// All the relevant definitions

#ifndef _MINIM_H_
#define _MINIM_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "gc.h"

//
//  Compiler configuration
//

#if defined (__GNUC__)
#define MINIM_GCC     1
#define NORETURN    __attribute__ ((noreturn))
#else
#error "compiler not supported"
#endif

#if INTPTR_MAX == INT64_MAX
#define MINIM_64
#elif INTPTR_MAX == INT32_MAX
#error "compilation for 32 bits not supported"
#else
#error "unknown pointer size or missing macros"
#endif

//
//  Version
//

#define MINIM_VERSION   "0.4.0"

//
//  Constants / Limits
//

#define INIT_READ_BUFFER_LEN    16

//
//  Types
//

typedef unsigned char mbyte;
typedef unsigned int mchar;
typedef long mfixnum;
typedef unsigned long msize;
typedef long iptr;
typedef unsigned long uptr;
typedef void *mobj;

#define ptr_size        sizeof(uptr)
#define PTR_ADD(p, d)   ((void *) ((uptr) (p)) + (d))

//
//  Objects
//

typedef enum {
    MINIM_OBJ_NULL,
    MINIM_OBJ_TRUE,
    MINIM_OBJ_FALSE,
    MINIM_OBJ_EOF,
    MINIM_OBJ_VOID,
    MINIM_OBJ_CHAR,
    MINIM_OBJ_FIXNUM,
    MINIM_OBJ_SYMBOL,
    MINIM_OBJ_STRING,
    MINIM_OBJ_PAIR,
    MINIM_OBJ_VECTOR,
    MINIM_OBJ_BOX,
    MINIM_OBJ_PORT,
} mobj_enum;

struct _mobj {
    mbyte type;
};

#define minim_type(o)           (((struct _mobj *) (o))->type)

// Intermediates
// +------------+ 
// |    type    | [0, 1)
// +------------+
extern mobj minim_null;
extern mobj minim_true;
extern mobj minim_false;
extern mobj minim_eof;
extern mobj minim_void;

#define minim_nullp(o)          ((o) == minim_null)
#define minim_truep(o)          ((o) == minim_true)
#define minim_falsep(o)         ((o) == minim_false)
#define minim_eofp(o)           ((o) == minim_eof)
#define minim_voidp(o)          ((o) == minim_void)

#define minim_boolp(o)          ((o) == minim_true || (o) == minim_false)

// Characters
// +------------+
// |    type    | [0, 1)
// |     c     |  [1, 4)
// +------------+
#define minim_char_size         sizeof(mchar)
#define minim_charp(o)          (minim_type(o) == MINIM_OBJ_CHAR)
#define minim_char(o)           (*((mchar *) PTR_ADD(o, 1)))

// Symbols
// +------------+
// |    type    | [0, 1)
// |    ...     |
// |    str     | [8, 16)
// +------------+
#define minim_symbol_size       (2 * ptr_size)
#define minim_symbolp(o)        (minim_type(o) == MINIM_OBJ_SYMBOL)
#define minim_symbol(o)         (*((mchar **) PTR_ADD(o, ptr_size)))

// String
// +------------+
// |    type    | [0, 1)
// |    ...     |
// |    str     | [8, 16)
// +------------+ 
#define minim_string_size       (2 * ptr_size)
#define minim_stringp(o)        (minim_type(o) == MINIM_OBJ_STRING)
#define minim_string(o)         (*((mchar **) PTR_ADD(o, ptr_size)))

// Fixnum
// +------------+
// |    type    | [0, 1)
// |    ...     |
// |     v      | [8, 16)
// +------------+ 
#define minim_fixnum_size       (2 * ptr_size)
#define minim_fixnump(o)        (minim_type(o) == MINIM_OBJ_FIXNUM)
#define minim_fixnum(o)         (*((mfixnum*) PTR_ADD(o, ptr_size)))

// Pair
// +------------+
// |    type    | [0, 1)
// |    ...     |
// |    car     | [8, 16)
// |    cdr     | [16, 24)
// +------------+ 
#define minim_cons_size         (3 * ptr_size)
#define minim_consp(o)          (minim_type(o) == MINIM_OBJ_PAIR)
#define minim_car(o)            (*((mobj*) PTR_ADD(o, ptr_size)))
#define minim_cdr(o)            (*((mobj*) PTR_ADD(o, 2 * ptr_size)))

#define minim_caar(o)       minim_car(minim_car(o))
#define minim_cadr(o)       minim_car(minim_cdr(o))
#define minim_cdar(o)       minim_cdr(minim_car(o))
#define minim_cddr(o)       minim_cdr(minim_car(o))

// Vector
// +------------+
// |    type    | [0, 1)
// |    ...     |
// |   length   | [8, 16)
// |    elts    | [16, 16 + 8 * N)
// +------------+ 
#define minim_vector_size(n)        (ptr_size * (2 + (n)))
#define minim_vectorp(o)            (minim_type(o) == MINIM_OBJ_VECTOR)
#define minim_vector_len(o)         (*((msize*) PTR_ADD(o, ptr_size)))
#define minim_vector_ref(o, i)      (((mobj*) PTR_ADD(o, 2 * ptr_size))[i])

// Box
// +------------+
// |    type    | [0, 1)
// |    ...     |
// |   content  | [8, 16)
// +------------+
#define minim_box_size      (2 * ptr_size)
#define minim_boxp(o)       (minim_type(o) == MINIM_OBJ_BOX)
#define minim_unbox(o)      (*((mobj*) PTR_ADD(o, ptr_size)))

// Port
// +------------+
// |    type    | [0, 1)
// |    flags   | [1, 2)
// |     fp     | [8, 16)
// +------------+
#define minim_port_size         (2 * ptr_size)
#define minim_portp(o)          (minim_type(o) == MINIM_OBJ_PORT)
#define minim_port_flags(o)     (*((mbyte*) PTR_ADD(o, 1)))
#define minim_port(o)           (*((FILE**) PTR_ADD(o, ptr_size)))

#define PORT_FLAG_OPEN              0x1
#define PORT_FLAG_READ              0x2
#define minim_port_openp(o)         (minim_port_flags(o) & PORT_FLAG_OPEN)
#define minim_port_readp(o)         (minim_port_flags(o) & PORT_FLAG_READ)

#define minim_inportp(o)        (minim_portp(o) && minim_port_readp(o))
#define minim_outportp(o)       (minim_portp(o) && !minim_port_readp(o))

#define minim_port_set(o, f) \
    minim_port_flags(o) |= (f);
#define minim_port_unset(o, f) \
    minim_port_flags(o) &= ~(f);

//
//  Constructors
//

mobj Mchar(mchar c);
mobj Msymbol(const mchar *s);
mobj Mstring(const char *s);
mobj Mstring2(mchar *s);
mobj Mfixnum(mfixnum v);
mobj Mcons(mobj car, mobj cdr);
mobj Mvector(msize n, mobj v);
mobj Mbox(mobj x);
mobj Mport(FILE *f, mbyte flags);

#define Mlist1(a)           Mcons(a, minim_null)
#define Mlist2(a, b)        Mcons(a, Mcons(b, minim_null))
#define Mlist3(a, b, c)     Mcons(a, Mcons(b, Mcons(c, minim_null)))
#define Mlist4(a, b, c, d)  Mcons(a, Mcons(b, Mcons(c, Mcons(d, minim_null))))

//
//  Pairs and lists
//

size_t list_length(mobj o);
mobj list_reverse(mobj o);

//
//  Vectors
//

mobj list_to_vector(mobj o);

//
//  Wide-string library
//

mchar *mstr(const char *s);
size_t mstrlen(const mchar *s);
int mstrcmp(const mchar *s1, const mchar *s2);

//
//  I/O
//

mobj read_object(mobj ip);
void write_object(mobj op, mobj o);

//
//  Errors
//

NORETURN void fatal_exit();
NORETURN void error(const char *who, const char *why);
NORETURN void error1(const char *who, const char *why, mobj);

//
//  Interner
//

typedef struct _ibucket {
    mobj sym;
    struct _ibucket *next;
} ibucket;

void intern_table_init();
mobj intern(const char *s);
mobj intern_str(const mchar *s);

//
//  Globals
//

typedef struct _M_thread {
    mobj *input_port;
    mobj *output_port;
} mthread;

extern struct _M_globals {
    // objects
    mobj null_string;
    mobj null_vector;
    // thread
    mthread *thread;
    // interner
    ibucket **oblist;
    size_t *oblist_len_ptr;
    size_t oblist_count;
} M_glob;

#define get_thread()        (M_glob.thread)
#define get_input_port(th)  ((th)->input_port)
#define get_output_port(th) ((th)->output_port)   

void minim_init();

#endif
