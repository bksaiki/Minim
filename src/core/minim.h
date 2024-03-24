/*
    Public header file for Minim
*/

#ifndef _MINIM_H_
#define _MINIM_H_

#include <setjmp.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "../gc/gc.h"
#include "build/config.h"

// Config

#if defined (__GNUC__)
#define MINIM_GCC     1
#define NORETURN    __attribute__ ((noreturn))
#else
#error "compiler not supported"
#endif

// System constants (assumes 64-bits)

typedef uint8_t         mbyte;
typedef int             mchar;
typedef long            mfixnum;
typedef uintptr_t       muptr;
typedef unsigned long   msize;
typedef void            *mobj;

#define ptr_size        8
#define PTR_ADD(p, d)   ((void *) ((muptr) (p)) + (d))

// Forward

typedef struct minim_thread     minim_thread;

// Constants

#define MINIM_VERSION      "0.3.5"

#define RECORD_FIELD_MAX            32767
#define SYMBOL_MAX_LEN              4096

#define INIT_VALUES_BUFFER_LEN      10
#define ENVIRONMENT_VECTOR_MAX      6

// Object types

typedef enum {
    /* Primitive types */
    MINIM_OBJ_SPECIAL,
    MINIM_OBJ_CHAR,
    MINIM_OBJ_FIXNUM,
    MINIM_OBJ_SYMBOL,
    MINIM_OBJ_STRING,
    MINIM_OBJ_PAIR,
    MINIM_OBJ_VECTOR,
    MINIM_OBJ_CLOSURE,
    MINIM_OBJ_PORT,

    /* Composite types */
    MINIM_OBJ_RECORD,
    MINIM_OBJ_BOX,
    MINIM_OBJ_HASHTABLE,
    MINIM_OBJ_SYNTAX,

    /* Runtime types */
    MINIM_OBJ_ENV,
    MINIM_OBJ_CONTINUATION,
    MINIM_OBJ_CODE,
} mobj_type;

typedef mobj (*mprim_proc)(int, mobj *);

// Objects

#define minim_type(o)           (*((mbyte*) o))

// Intermediates
// +------------+ 
// |    type    | [0, 1)
// +------------+
extern mobj minim_null;
extern mobj minim_true;
extern mobj minim_false;
extern mobj minim_eof;
extern mobj minim_void;
extern mobj minim_empty_vec;
extern mobj minim_base_rtd;
extern mobj minim_values;

#define minim_nullp(x)        ((x) == minim_null)
#define minim_truep(x)        ((x) == minim_true)
#define minim_falsep(x)       ((x) == minim_false)
#define minim_eofp(x)         ((x) == minim_eof)
#define minim_voidp(x)        ((x) == minim_void)
#define minim_valuesp(x)      ((x) == minim_values)

#define minim_empty_vecp(x)   ((x) == minim_empty_vec)
#define minim_base_rtdp(x)    ((x) == minim_base_rtd)

#define minim_boolp(o)          ((o) == minim_true || (o) == minim_false)
#define minim_not(o)            ((o) == minim_false ? minim_true : minim_false)

// Characters
// +------------+
// |    type    | [0, 1)
// |     c     |  [4, 8)
// +------------+
#define minim_char_size         ptr_size
#define minim_charp(o)          (minim_type(o) == MINIM_OBJ_CHAR)
#define minim_char(o)           (*((char *) PTR_ADD(o, 4)))

// Fixnum
// +------------+
// |    type    | [0, 1)
// |     v      | [8, 16)
// +------------+ 
#define minim_fixnum_size       (2 * ptr_size)
#define minim_fixnump(o)        (minim_type(o) == MINIM_OBJ_FIXNUM)
#define minim_fixnum(o)         (*((mfixnum*) PTR_ADD(o, ptr_size)))

// Symbols
// +------------+
// |    type    | [0, 1)
// |    str     | [8, 16)
// +------------+
#define minim_symbol_size       (2 * ptr_size)
#define minim_symbolp(o)        (minim_type(o) == MINIM_OBJ_SYMBOL)
#define minim_symbol(o)         (*((char **) PTR_ADD(o, ptr_size)))

// String
// +------------+
// |    type    | [0, 1)
// |    str     | [8, 16)
// +------------+ 
#define minim_string_size       (2 * ptr_size)
#define minim_stringp(o)        (minim_type(o) == MINIM_OBJ_STRING)
#define minim_string(o)         (*((char **) PTR_ADD(o, ptr_size)))
#define minim_string_ref(o, i)  (minim_string(o)[(i)])

// Pair
// +------------+
// |    type    | [0, 1)
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
// |   content  | [8, 16)
// +------------+
#define minim_box_size      (2 * ptr_size)
#define minim_boxp(o)       (minim_type(o) == MINIM_OBJ_BOX)
#define minim_unbox(o)      (*((mobj*) PTR_ADD(o, ptr_size)))

// Closure
// +------------+
// |    type    | [0, 1)
// |    env     | [8, 16)
// |    code    | [16, 24)
// |    name    | [24, 32)
// +------------+
#define minim_closure_size          (4 * ptr_size)
#define minim_closurep(o)           (minim_type(o) == MINIM_OBJ_CLOSURE)
#define minim_closure_env(o)        (*((mobj*) PTR_ADD(o, ptr_size)))
#define minim_closure_code(o)       (*((mobj*) PTR_ADD(o, 2 * ptr_size)))
#define minim_closure_name(o)       (*((mobj*) PTR_ADD(o, 3 * ptr_size)))

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
#define PORT_FLAG_STR               0x4
#define minim_port_openp(o)         (minim_port_flags(o) & PORT_FLAG_OPEN)
#define minim_port_readp(o)         (minim_port_flags(o) & PORT_FLAG_READ)
#define minim_port_strp(o)          (minim_port_flags(o) & PORT_FLAG_STR)

#define minim_input_portp(x)        (minim_portp(x) && minim_port_readp(x))
#define minim_output_portp(x)       (minim_portp(x) && !minim_port_readp(x))
#define minim_string_portp(x)       (minim_portp(x) && minim_port_strp(x))

#define minim_port_set(o, f) \
    minim_port_flags(o) |= (f);
#define minim_port_unset(o, f) \
    minim_port_flags(o) &= ~(f);

// Syntax
// +------------+
// |    type    | [0, 1)
// |     e      | [8, 16)
// |    loc     | [16, 24)
// +------------+
#define minim_syntax_size       (3 * ptr_size)
#define minim_syntaxp(o)        (minim_type(o) == MINIM_OBJ_SYNTAX)
#define minim_syntax_e(o)       (*((mobj*) PTR_ADD(o, ptr_size)))
#define minim_syntax_loc(o)     (*((mobj*) PTR_ADD(o, 2 * ptr_size)))

// Records: RTD/values
// +------------+
// |    type    | [0, 1)
// |   fieldc   | [4, 8)
// |    rtd     | [8, 16)
// |   fields   | [16, 16 + 8 * N)
// |    ...     |
// +------------+
#define minim_record_size(n)        (ptr_size * (2 + (n)))
#define minim_recordp(o)            (minim_type(o) == MINIM_OBJ_RECORD)
#define minim_record_count(o)       (*((int*) PTR_ADD(o, 4)))
#define minim_record_rtd(o)         (*((mobj*) PTR_ADD(o, ptr_size)))
#define minim_record_ref(o, i)      (((mobj*) PTR_ADD(o, 2 * ptr_size))[i])

// Hashtables
// +------------+
// |    type    | [0, 1)
// |   buckets  | [8, 16)
// |  alloc_ptr | [16, 24)
// |    count   | [24, 32)
// +------------+
#define minim_hashtable_size            (4 * ptr_size)
#define minim_hashtablep(o)             (minim_type(o) == MINIM_OBJ_HASHTABLE)
#define minim_hashtable_buckets(o)      (*((mobj*) PTR_ADD(o, ptr_size)))
#define minim_hashtable_bucket(o, i)    (minim_vector_ref(minim_hashtable_buckets(o), i))
#define minim_hashtable_alloc_ptr(o)    (*((msize**) PTR_ADD(o, 2 * ptr_size)))
#define minim_hashtable_alloc(o)        (*(minim_hashtable_alloc_ptr(o)))
#define minim_hashtable_count(o)        (*((msize*) PTR_ADD(o, 3 * ptr_size)))

// Environment
// +------------+
// |    type    | [0, 1)
// |    prev    | [8, 16)
// |   bindings | [16, 24)
// +------------+
#define minim_env_size          (3 * ptr_size)
#define minim_envp(o)           (minim_type(o) == MINIM_OBJ_ENV)
#define minim_env_prev(o)       (*((mobj*) PTR_ADD(o, ptr_size)))
#define minim_env_bindings(o)   (*((mobj*) PTR_ADD(o, 2 * ptr_size)))

// Continuations
// +------------+
// |    type    | [0, 8)
// |    prev    | [8, 16)
// |    pc      | [16, 24)
// |    env     | [24, 32)
// |    sfp     | [32, 40)
// |    cp      | [40, 48)
// |    ac      | [48, 56)
// +------------+

#define continuation_size           (7 * ptr_size)
#define minim_continuationp(o)      (minim_type(o) == MINIM_OBJ_CONTINUATION)
#define continuation_prev(c)        (*((mobj*) PTR_ADD(c, ptr_size)))
#define continuation_pc(c)          (*((mobj*) PTR_ADD(c, 2 * ptr_size)))
#define continuation_env(c)         (*((mobj*) PTR_ADD(c, 3 * ptr_size)))
#define continuation_sfp(c)         (*((mobj**) PTR_ADD(c, 4 * ptr_size)))
#define continuation_cp(c)          (*((mobj*) PTR_ADD(c, 5 * ptr_size)))
#define continuation_ac(c)          (*((size_t*) PTR_ADD(c, 6 * ptr_size)))

// Code
// +------------+
// |   type     | [0, 1)
// |   size     | [8, 16)
// |   arity    | [24, 32)
// |   reloc    | [32, 40)
// |   instrs   | [40, ...) 
// |   ...      |
// +------------+
#define minim_code_header_size      4
#define minim_code_size(n)          ((minim_code_header_size * ptr_size) + (n * ptr_size) + ptr_size)
#define minim_codep(o)              (minim_type(o) == MINIM_OBJ_CODE)
#define minim_code_len(o)           (*((size_t*) PTR_ADD(o, ptr_size)))
#define minim_code_arity(o)         (*((mobj*) PTR_ADD(o, 2 * ptr_size)))
#define minim_code_reloc(o)         (*((mobj*) PTR_ADD(o, 3 * ptr_size)))
#define minim_code_it(o)            ((mobj *) PTR_ADD(o, 4 * ptr_size))
#define minim_code_ref(o, i)        (minim_code_it(o)[i]) 

// Special objects

extern mobj begin_symbol;
extern mobj define_values_symbol;
extern mobj if_symbol;
extern mobj lambda_symbol;
extern mobj let_values_symbol;
extern mobj letrec_values_symbol;
extern mobj quote_symbol;
extern mobj quote_syntax_symbol;
extern mobj setb_symbol;
extern mobj values_symbol;

extern mobj apply_symbol;
extern mobj bind_symbol;
extern mobj bind_values_symbol;
extern mobj bind_values_top_symbol;
extern mobj brancha_symbol;
extern mobj branchf_symbol;
extern mobj ccall_symbol;
extern mobj clear_frame_symbol;
extern mobj check_arity_symbol;
extern mobj check_stack_symbol;
extern mobj do_apply_symbol;
extern mobj do_eval_symbol;
extern mobj do_raise_symbol;
extern mobj do_rest_symbol;
extern mobj do_values_symbol;
extern mobj do_with_values_symbol;
extern mobj get_arg_symbol;
extern mobj get_env_symbol;
extern mobj literal_symbol;
extern mobj lookup_symbol;
extern mobj make_closure_symbol;
extern mobj make_env_symbol;
extern mobj pop_symbol;
extern mobj pop_env_symbol;
extern mobj push_symbol;
extern mobj push_env_symbol;
extern mobj rebind_symbol;
extern mobj ret_symbol;
extern mobj set_proc_symbol;
extern mobj save_cc_symbol;

// Complex predicates

#define minim_procp(x)  (minim_closurep(x))

// Constructors

mobj Mchar(mchar c);
mobj Mfixnum(long v);
mobj Msymbol(const char *s);
mobj Mstring(const char *s);
mobj Mstring2(long len, mchar c);
mobj Mcons(mobj car, mobj cdr);
mobj Mvector(long len, mobj init);
mobj Mrecord(mobj rtd, int fieldc);
mobj Mbox(mobj x);
mobj Mhashtable(size_t size_hint);
mobj Mclosure(mobj env, mobj code);
mobj Minput_port(FILE *stream);
mobj Moutput_port(FILE *stream);
mobj Msyntax(mobj e, mobj loc);
mobj Menv(mobj prev);
mobj Menv2(mobj prev, size_t size);
mobj Mcontinuation(mobj prev, mobj pc, mobj env, minim_thread *th);
mobj Mcode(size_t size);

#define Mlist1(a)                   Mcons(a, minim_null)
#define Mlist2(a, b)                Mcons(a, Mlist1(b))
#define Mlist3(a, b, c)             Mcons(a, Mlist2(b, c))
#define Mlist4(a, b, c, d)          Mcons(a, Mlist3(b, c, d))
#define Mlist5(a, b, c, d, e)       Mcons(a, Mlist4(b, c, d, e))
#define Mlist6(a, b, c, d, e, f)    Mcons(a, Mlist5(b, c, d, e, f))

// Object operations

int minim_eqp(mobj a, mobj b);
int minim_equalp(mobj a, mobj b);

int minim_listp(mobj x);
long list_length(mobj xs);
long improper_list_length(mobj xs);
void list_set_tail(mobj xs, mobj ys);

mobj nullp_proc(mobj x);
mobj voidp_proc(mobj x);
mobj eofp_proc(mobj x);
mobj boolp_proc(mobj x);
mobj symbolp_proc(mobj x);
mobj fixnump_proc(mobj x);
mobj charp_proc(mobj x);
mobj stringp_proc(mobj x);
mobj consp_proc(mobj x);
mobj listp_proc(mobj x);
mobj vectorp_proc(mobj x);
mobj procp_proc(mobj x);
mobj input_portp_proc(mobj x);
mobj output_portp_proc(mobj x);
mobj string_portp_proc(mobj x);
mobj boxp_proc(mobj x);
mobj hashtablep_proc(mobj x);
mobj recordp_proc(mobj x);
mobj record_rtdp_proc(mobj x);
mobj record_valuep_proc(mobj x);
mobj syntaxp_proc(mobj x);

mobj not_proc(mobj x);
mobj eq_proc(mobj x, mobj y);
mobj equal_proc(mobj x, mobj y);

#define NUL_CHAR        ((mchar) 0x00)      // null
#define BEL_CHAR        ((mchar) 0x07)      // alarm / bell
#define BS_CHAR         ((mchar) 0x08)      // backspace
#define HT_CHAR         ((mchar) 0x09)      // horizontal tab
#define LF_CHAR         ((mchar) 0x0A)      // line feed
#define VT_CHAR         ((mchar) 0x0B)      // vertical tab
#define FF_CHAR         ((mchar) 0x0C)      // form feed (page break)
#define CR_CHAR         ((mchar) 0x0D)      // carriage return
#define ESC_CHAR        ((mchar) 0x1B)      // escape
#define SP_CHAR         ((mchar) 0x20)      // space
#define DEL_CHAR        ((mchar) 0x7F)      // delete

mobj char_to_integer(mobj x);
mobj integer_to_char(mobj x);

mobj fx_neg(mobj x);
mobj fx2_add(mobj x, mobj y);
mobj fx2_sub(mobj x, mobj y);
mobj fx2_mul(mobj x, mobj y);
mobj fx2_div(mobj x, mobj y);
mobj fx_remainder(mobj x, mobj y);
mobj fx_modulo(mobj x, mobj y);

mobj fx2_eq(mobj x, mobj y);
mobj fx2_gt(mobj x, mobj y);
mobj fx2_lt(mobj x, mobj y);
mobj fx2_ge(mobj x, mobj y);
mobj fx2_le(mobj x, mobj y);

mobj make_string(mobj len, mobj init);
mobj string_length(mobj s);
mobj string_ref(mobj s, mobj idx);
mobj string_set(mobj s, mobj idx, mobj c);
mobj number_to_string(mobj n);
mobj string_to_number(mobj s);
mobj symbol_to_string(mobj s);
mobj string_to_symbol(mobj s);
mobj list_to_string(mobj xs);
mobj string_to_list(mobj s);

mobj car_proc(mobj x);
mobj cdr_proc(mobj x);
mobj caar_proc(mobj x);
mobj cadr_proc(mobj x);
mobj cdar_proc(mobj x);
mobj cddr_proc(mobj x);

mobj caaar_proc(mobj x);
mobj caadr_proc(mobj x);
mobj cadar_proc(mobj x);
mobj caddr_proc(mobj x);
mobj cdaar_proc(mobj x);
mobj cdadr_proc(mobj x);
mobj cddar_proc(mobj x);
mobj cdddr_proc(mobj x);

mobj caaaar_proc(mobj x);
mobj caaadr_proc(mobj x);
mobj caadar_proc(mobj x);
mobj caaddr_proc(mobj x);
mobj cadaar_proc(mobj x);
mobj cadadr_proc(mobj x);
mobj caddar_proc(mobj x);
mobj cadddr_proc(mobj x);

mobj cdaaar_proc(mobj x);
mobj cdaadr_proc(mobj x);
mobj cdadar_proc(mobj x);
mobj cdaddr_proc(mobj x);
mobj cddaar_proc(mobj x);
mobj cddadr_proc(mobj x);
mobj cdddar_proc(mobj x);
mobj cddddr_proc(mobj x);

mobj set_car_proc(mobj p, mobj x);
mobj set_cdr_proc(mobj p, mobj x);

mobj make_list_proc(mobj len, mobj init);
mobj length_proc(mobj xs);
mobj list_reverse(mobj xs);
mobj list_append2(mobj xs, mobj ys);

mobj make_vector(mobj len, mobj init);
mobj vector_length(mobj v);
mobj vector_ref(mobj v, mobj idx);
mobj vector_set(mobj v, mobj idx, mobj x);
mobj vector_fill(mobj v, mobj x);
mobj vector_to_list(mobj v);
mobj list_to_vector(mobj xs);

mobj box_proc(mobj x);
mobj unbox_proc(mobj x);
mobj box_set_proc(mobj x, mobj v);

mobj current_input_port();
mobj current_output_port();
mobj current_error_port();
mobj open_input_file(mobj name);
mobj open_output_file(mobj name);
mobj open_input_string(mobj str);
mobj open_output_string();
mobj close_input_port(mobj port);
mobj close_output_port(mobj port);
mobj get_output_string(mobj port);

mobj read_proc(mobj port);
mobj read_char_proc(mobj port);
mobj peek_char_proc(mobj port);
mobj char_readyp_proc(mobj port);

mobj put_char_proc(mobj port, mobj ch);
mobj put_string_proc(mobj port, mobj ch, mobj start, mobj end);
mobj flush_output_proc(mobj port);
mobj newline_proc(mobj port);

mobj fasl_read_proc(mobj port);
mobj fasl_write_proc(mobj x, mobj port);

mobj make_hashtable(mobj size);
mobj hashtable_copy(mobj ht);
mobj hashtable_size(mobj ht);
mobj hashtable_size_set(mobj ht, mobj size);
mobj hashtable_length(mobj ht);
mobj hashtable_ref(mobj ht, mobj h);
mobj hashtable_set(mobj ht, mobj h, mobj cells);
mobj hashtable_cells(mobj ht);
mobj hashtable_keys(mobj ht);
mobj hashtable_clear(mobj ht);

mobj eq_hashtable_find(mobj ht, mobj k);
mobj eq_hashtable_find2(mobj ht, mobj k, size_t h);
void eq_hashtable_set(mobj ht, mobj k, mobj v);

size_t eq_hash(mobj o);
mobj eq_hash_proc(mobj x);
mobj equal_hash_proc(mobj x);

mobj make_rtd(mobj name, mobj parent, mobj uid, mobj sealedp, mobj opaquep, mobj fields);
mobj rtd_name(mobj rtd);
mobj rtd_parent(mobj rtd);
mobj rtd_uid(mobj rtd);
mobj rtd_sealedp(mobj rtd);
mobj rtd_opaquep(mobj rtd);
mobj rtd_length(mobj rtd);
mobj rtd_fields(mobj rtd);
mobj rtd_field_mutablep(mobj rtd, mobj idx);

mobj default_record_equal_proc();
mobj default_record_equal_set_proc(mobj proc);
mobj default_record_hash_proc();
mobj default_record_hash_set_proc(mobj proc);

mobj make_record(mobj rtd, mobj fields);
mobj record_rtd_proc(mobj rec);
mobj record_ref_proc(mobj rec, mobj idx);
mobj record_set_proc(mobj rec, mobj idx, mobj x);

mobj procedure_arity_proc(mobj proc);
mobj procedure_name_proc(mobj proc);
mobj procedure_rename_proc(mobj proc, mobj id);

mobj syntax_e_proc(mobj stx);
mobj syntax_loc_proc(mobj stx);
mobj strip_syntax(mobj o);
mobj to_syntax(mobj o);
mobj syntax_to_list(mobj stx);

mobj environmentp_proc(mobj o);
mobj interaction_environment();
mobj empty_environment();
mobj environment_proc();
mobj environment_names(mobj env);
mobj extend_environment(mobj env);
mobj environment_variable_ref(mobj env, mobj k, mobj fail);
mobj environment_variable_set(mobj env, mobj k, mobj v);

mobj code_to_instrs(mobj code);

mobj load_proc(mobj fname);
mobj exit_proc(mobj code);
mobj version_proc();
mobj command_line_proc();
mobj current_directory_proc();
mobj current_directory_set_proc(mobj path);

mobj boot_error_proc(mobj who, mobj msg, mobj args);
mobj c_error_handler_proc();
mobj c_error_handler_set_proc(mobj proc);
mobj error_handler_proc();
mobj error_handler_set_proc(mobj proc);

NORETURN void minim_error(const char *name, const char *msg);
NORETURN void minim_error1(const char *name, const char *msg, mobj x);
NORETURN void minim_error2(const char *name, const char *msg, mobj x, mobj y);
NORETURN void minim_error3(const char *name, const char *msg, mobj x, mobj y, mobj z);

// Stack segment
// +--------------+
// |     prev     | [0, 8)
// |     size     | [8, 16)
// |     ...      | [16, ...)
// +--------------+

#define stack_seg_default_size      (1024 * 1024)
#define stack_seg_header_size       (2 * ptr_size)
#define stack_seg_prev(s)           (*((mobj *) (s)))
#define stack_seg_size(s)           (*((size_t*) PTR_ADD(s, ptr_size)))
#define stack_seg_base(s)           PTR_ADD(s, 2 * ptr_size)
#define stack_seg_end(s)            PTR_ADD(stack_seg_base(s), stack_seg_size(s))

mobj Mstack_segment(mobj prev, size_t size);

// I/O

mobj read_object(FILE *in);
void write_object(FILE *out, mobj o);

// FASL

typedef enum {
    FASL_NULL = 0,
    FASL_TRUE,
    FASL_FALSE,
    FASL_EOF,
    FASL_VOID,
    FASL_SYMBOL,
    FASL_STRING,
    FASL_FIXNUM,
    FASL_CHAR,
    FASL_PAIR,
    FASL_EMPTY_VECTOR,
    FASL_VECTOR,
    FASL_BASE_RTD,
    FASL_RECORD,
} fasl_type;

mobj read_fasl(FILE *out);
void write_fasl(FILE *out, mobj o);

// JIT compiler

mobj compile_expr(mobj expr);
mobj compile_prim(const char *who, void *fn, mobj arity);

mobj compile_apply(mobj name);
mobj compile_call_with_values(mobj name);
mobj compile_current_environment(mobj name);
mobj compile_eval(mobj name);
mobj compile_identity(mobj name);
mobj compile_raise(mobj name);
mobj compile_values(mobj name);
mobj compile_void(mobj name);

// Interpreter

void reserve_stack(minim_thread *th, size_t argc);
void set_arg(minim_thread *th, size_t i, mobj x);
mobj get_arg(minim_thread *th, size_t i);

void check_expr(mobj expr);
mobj eval_expr(mobj expr, mobj env);

// Environments
mobj setup_env();
mobj make_env();

void env_define_var_no_check(mobj env, mobj var, mobj val);
mobj env_define_var(mobj env, mobj var, mobj val);
mobj env_set_var(mobj env, mobj var, mobj val);
mobj env_lookup_var(mobj env, mobj var);
int env_var_is_defined(mobj env, mobj var, int recursive);

extern mobj empty_env;

// Records

/*
    Records come in two flavors:
     - record type descriptor
     - record value

    Record type descriptors have the following structure:
    
    +--------------------------+
    |  RTD pointer (base RTD)  | (rtd)
    +--------------------------+
    |           Name           | (fields[0])
    +--------------------------+
    |          Parent          | (fields[1])
    +--------------------------+
    |            UID           |  ...
    +--------------------------+
    |          Sealed?         |
    +--------------------------+
    |          Opaque?         |
    +--------------------------+
    |  Protocol (constructor)  |
    +--------------------------+
    |    Field 0 Descriptor    |
    +--------------------------+
    |    Field 1 Descriptor    |
    +--------------------------+
    |                          |
                ...
    |                          |
    +--------------------------+
    |    Field N Descriptor    |
    +--------------------------+

    Field descriptors have the form:
    
        '(immutable <name>)
        '(mutable <name>)

    Any other descriptor is invalid.

    There is always a unique record type descriptor during runtime:
    the base record type descriptor. It cannot be accessed during runtime
    and serves as the "record type descriptor" of all record type descriptors.

    Record values have the following structure:

    +--------------------------+
    |        RTD pointer       |
    +--------------------------+
    |          Field 0         |
    +--------------------------+
    |          Field 1         |
    +--------------------------+
    |                          |
                ...
    |                          |
    +--------------------------+
    |          Field N         |
    +--------------------------+
*/

#define record_rtd_min_size         6
#define record_rtd_name(rtd)        minim_record_ref(rtd, 0)
#define record_rtd_parent(rtd)      minim_record_ref(rtd, 1)
#define record_rtd_uid(rtd)         minim_record_ref(rtd, 2)
#define record_rtd_sealed(rtd)      minim_record_ref(rtd, 3)
#define record_rtd_opaque(rtd)      minim_record_ref(rtd, 4)
#define record_rtd_protocol(rtd)    minim_record_ref(rtd, 5)
#define record_rtd_fieldc(rtd)      (minim_record_count(rtd) - record_rtd_min_size)
#define record_rtd_field(rtd, i)    minim_record_ref(rtd, (record_rtd_min_size + (i)))

#define record_opaquep(o)     (record_rtd_opaque(minim_record_rtd(o)) == minim_true)
#define record_sealedp(o)     (record_rtd_sealed(minim_record_rtd(o)) == minim_true)

int record_valuep(mobj o);
int record_rtdp(mobj o);

// Symbols

typedef struct intern_table_bucket {
    mobj sym;
    struct intern_table_bucket *next;
} intern_table_bucket;

typedef struct intern_table {
    intern_table_bucket **buckets;
    size_t *alloc_ptr;
    size_t alloc, size;
} intern_table;

intern_table *make_intern_table();
mobj intern_symbol(intern_table *itab, const char *sym);

// Threads

typedef struct minim_thread {
    // thread runtime
    mobj env;           // environment
    mobj continuation;  // continuation
    mobj segment;       // stack segment
    mobj *sfp;          // stack pointer
    mobj cp;            // current procedure
    size_t ac;          // argument counter
    jmp_buf *reentry;   // jmp_buf for re-entry
    // thread parameters
    mobj input_port;
    mobj output_port;
    mobj error_port;
    mobj current_directory;
    mobj command_line;
    mobj record_equal_proc;
    mobj record_hash_proc;
    mobj error_handler;
    mobj c_error_handler;
    // `values` stack
    mobj *values_buffer;
    int values_buffer_size;
    int values_buffer_count;
    // thread id
    int pid;
} minim_thread;

#define global_env(th)                  ((th)->env)
#define current_continuation(th)        ((th)->continuation)
#define current_segment(th)             ((th)->segment)
#define current_sfp(th)                 ((th)->sfp)
#define current_cp(th)                  ((th)->cp)
#define current_ac(th)                  ((th)->ac)
#define current_reentry(th)             ((th)->reentry)

#define input_port(th)                  ((th)->input_port)
#define output_port(th)                 ((th)->output_port)
#define error_port(th)                 ((th)->error_port)
#define current_directory(th)           ((th)->current_directory)
#define command_line(th)                ((th)->command_line)
#define record_equal_proc(th)           ((th)->record_equal_proc)
#define record_hash_proc(th)            ((th)->record_hash_proc)
#define error_handler(th)               ((th)->error_handler)
#define c_error_handler(th)             ((th)->c_error_handler)

#define values_buffer(th)               ((th)->values_buffer)
#define values_buffer_ref(th, i)        ((values_buffer(th))[i])
#define values_buffer_size(th)          ((th)->values_buffer_size)
#define values_buffer_count(th)         ((th)->values_buffer_count)

// Globals

typedef struct minim_globals {;
    minim_thread *current_thread;
    intern_table *symbols;
} minim_globals;

extern minim_globals *globals;
extern size_t bucket_sizes[];

#define current_thread()    (globals->current_thread)
#define intern(s)           (intern_symbol(globals->symbols, s))

// System

char *get_file_dir(const char *realpath);
char* get_current_dir();
void set_current_dir(const char *str);

void *alloc_page(size_t size);
int make_page_executable(void *page, size_t size);
int make_page_write_only(void *page, size_t size);

mobj load_file(const char *fname, mobj env);
mobj load_prelude(mobj env);

NORETURN void minim_shutdown(int code);

// Exceptions

NORETURN void arity_mismatch_exn(mobj proc, size_t actual);
NORETURN void bad_syntax_exn(mobj expr);
NORETURN void bad_type_exn(const char *name, const char *type, mobj x);
NORETURN void result_arity_exn(const char *name, short expected, short actual);

// Primitives

void init_prims(mobj env);

#endif  // _MINIM_H_
