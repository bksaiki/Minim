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
#define PRIM_TABLE_SIZE    509

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
    MINIM_OBJ_SPECIAL,
    MINIM_OBJ_CHAR,
    MINIM_OBJ_FIXNUM,
    MINIM_OBJ_SYMBOL,
    MINIM_OBJ_STRING,
    MINIM_OBJ_PAIR,
    MINIM_OBJ_VECTOR,
    MINIM_OBJ_BOX,
    MINIM_OBJ_PORT,
    MINIM_OBJ_CLOSURE
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
#define minim_string_ref(o, i)  (minim_string(o)[(i)])

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
#define minim_cddr(o)       minim_cdr(minim_cdr(o))

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

// Closure
// +------------+
// |    type    | [0, 1)
// |    env     | [8, 16)
// |    code    | [16, 24)
// +------------+
#define minim_closure_size         (3 * ptr_size)
#define minim_closure_env_offset   (ptr_size)
#define minim_closure_code_offset  (2 * ptr_size)

#define minim_closurep(o)          (minim_type(o) == MINIM_OBJ_CLOSURE)
#define minim_closure_env(o)       (*((mobj*) PTR_ADD(o, minim_closure_env_offset)))
#define minim_closure_code(o)      (*((void**) PTR_ADD(o, minim_closure_code_offset)))

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
mobj Mclosure(mobj env, mobj code);
mobj Mport(FILE *f, mbyte flags);

#define Mlist1(a)                   Mcons(a, minim_null)
#define Mlist2(a, b)                Mcons(a, Mlist1(b))
#define Mlist3(a, b, c)             Mcons(a, Mlist2(b, c))
#define Mlist4(a, b, c, d)          Mcons(a, Mlist3(b, c, d))
#define Mlist5(a, b, c, d, e)       Mcons(a, Mlist4(b, c, d, e))
#define Mlist6(a, b, c, d, e, f)    Mcons(a, Mlist5(b, c, d, e, f))

//
//  Equality
//

int minim_eqp(mobj x, mobj y);
int minim_equalp(mobj x, mobj y);

//
//  Pairs and lists
//

#define for_each(l, stmts...) {    \
    while (!minim_nullp(l)) { \
        stmts ; \
        l = minim_cdr(l); \
    } \
}

int listp(mobj o);
size_t list_length(mobj l);
mobj list_reverse(mobj l);
mobj list_ref(mobj l, mobj i);
mobj list_member(mobj x, mobj l);
mobj list_append(mobj x, mobj y);

//
//  Vectors
//

mobj list_to_vector(mobj o);

//
//  Numbers
//

mobj fix_neg(mobj x);
mobj fix_add(mobj x, mobj y);
mobj fix_sub(mobj x, mobj y);
mobj fix_mul(mobj x, mobj y);
mobj fix_div(mobj x, mobj y);
mobj fix_rem(mobj x, mobj y);

//
//  Characters
//

#define NUL_CHAR        ((char) 0x00)       // null
#define BEL_CHAR        ((char) 0x07)       // alarm / bell
#define BS_CHAR         ((char) 0x08)       // backspace
#define HT_CHAR         ((char) 0x09)       // horizontal tab
#define LF_CHAR         ((char) 0x0A)       // line feed
#define VT_CHAR         ((char) 0x0B)       // vertical tab
#define FF_CHAR         ((char) 0x0C)       // form feed (page break)
#define CR_CHAR         ((char) 0x0D)       // carriage return
#define ESC_CHAR        ((char) 0x1B)       // escape
#define SP_CHAR         ((char) 0x20)       // space
#define DEL_CHAR        ((char) 0x7F)       // delete

//
//  Strings
//

mchar *mstr(const char *s);
size_t mstrlen(const mchar *s);
int mstrcmp(const mchar *s1, const mchar *s2);

mobj fixnum_to_string(mobj x);
mobj string_append(mobj x, mobj y);
mobj string_to_symbol(mobj x);
mobj symbol_to_string(mobj x);

//
//  Numbers
//

#define minim_zerop(o)      (minim_fixnum(o) == 0)
#define minim_negativep(o)  (minim_fixnum(o) < 0)

//
//  I/O
//

mobj open_input_file(const char *f);
mobj open_output_file(const char *f);
void close_port(mobj p);

mobj read_object(mobj ip);
void write_object(mobj op, mobj o);

//
//  Errors
//

NORETURN void fatal_exit();
NORETURN void error(const char *who, const char *why);
NORETURN void error1(const char *who, const char *why, mobj);

//
//  System
//

char* get_current_dir();
void *alloc_page(size_t size);
int make_page_executable(void* page, size_t size);

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
//  Primitives
//

void prim_table_init();
mobj lookup_prim(const mchar *name);
void register_prim(const char *name, void *fn);

//
//  Globals
//

// core syntax
extern mobj define_values_sym;
extern mobj letrec_values_sym;
extern mobj let_values_sym;
extern mobj values_sym;
extern mobj lambda_sym;
extern mobj begin_sym;
extern mobj if_sym;
extern mobj quote_sym;
extern mobj setb_sym;
extern mobj void_sym;

// extended syntax
extern mobj define_sym;
extern mobj let_sym;
extern mobj letrec_sym;
extern mobj cond_sym;
extern mobj and_sym;
extern mobj or_sym;
extern mobj foreign_proc_sym;

typedef struct _M_thread {
    mobj *input_port;
    mobj *output_port;
    mobj *working_dir;
} mthread;

#define th_input_port(th)  ((th)->input_port)
#define th_output_port(th) ((th)->output_port)   
#define th_working_dir(th) ((th)->working_dir)

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
    // primitive table
    mobj primlist;
} M_glob;

#define get_thread()        (M_glob.thread)

void minim_init();

//
//  Bootstrapping
//

mobj expand_expr(mobj e);
mobj expand_top(mobj e);
mobj compile_module(mobj op, mobj name, mobj es);
void *install_module(mobj cstate);

#endif
