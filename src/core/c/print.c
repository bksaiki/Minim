// print.c: printer

#include "../minim.h"

static void write_char(FILE *out, mobj o) {
    mchar c = minim_char(o);

    fputs("#\\", out);
    if (c == '\0') fputs("nul", out);
    else if (c == BEL_CHAR) fputs("alarm", out);
    else if (c == BS_CHAR) fputs("backspace", out);
    else if (c == '\t') fputs("tab", out);
    else if (c == '\n') fputs("newline", out);
    else if (c == VT_CHAR) fputs("vtab", out);
    else if (c == FF_CHAR) fputs("page", out);
    else if (c == CR_CHAR) fputs("return", out);
    else if (c == ESC_CHAR) fputs("esc", out);
    else if (c == ' ') fputs("space", out);
    else if (c == DEL_CHAR) fputs("delete", out);
    else fputc(c, out);
}

static void write_pair(FILE *out, mobj o) {
    fputc('(', out);
loop:
    write_object(out, minim_car(o));
    if (minim_consp(minim_cdr(o))) {
        // middle of a (proper/improper) list
        fputc(' ', out);
        o = minim_cdr(o);
        goto loop;
    } else if (!minim_nullp(minim_cdr(o))) {
        // end of an improper list
        fputs(" . ", out);
        write_object(out, minim_cdr(o));
    }

    fputc(')', out);
}

static void write_vector(FILE *out, mobj o) {
    if (minim_vector_len(o) == 0) {
        fputs("#()", out);
    } else {
        fputs("#(", out);
        write_object(out, minim_vector_ref(o, 0));
        for (long i = 1; i < minim_vector_len(o); i++) {
            fputc(' ', out);
            write_object(out, minim_vector_ref(o, i));
        }
        fputc(')', out);
    }
}

static void write_box(FILE *out, mobj o) {
    fputs("#&", out);
    write_object(out, minim_unbox(o));
}

static void write_closure(FILE *out, mobj o) {
    if (minim_closure_name(o)) {
        fprintf(out, "#<procedure:%s>", minim_closure_name(o));
    } else {
        fputs("#<procedure>", out);
    }
}

static void write_record(FILE *out, mobj o) {
    if (minim_record_rtd(o) == minim_base_rtd) {
        fprintf(out, "#<record-type:%s>", minim_symbol(record_rtd_name(o)));
    } else {
        fprintf(out, "#<%s>", minim_symbol(record_rtd_name(minim_record_rtd(o))));
    }
}

void write_object(FILE *out, mobj o) {
    if (minim_nullp(o)) fputs("()", out);
    else if (minim_truep(o)) fputs("#t", out);
    else if (minim_falsep(o)) fputs("#f", out);
    else if (minim_eofp(o)) fputs("#<eof>", out);
    else if (minim_voidp(o)) fputs("#<void>", out);
    else if (minim_base_rtdp(o)) fputs("#<base-record-type>", out);
    else if (minim_charp(o)) write_char(out, o);
    else if (minim_fixnump(o)) fprintf(out, "%ld", minim_fixnum(o));
    else if (minim_symbolp(o)) fprintf(out, "%s", minim_symbol(o));
    else if (minim_stringp(o)) fprintf(out, "\"%s\"", minim_string(o));
    else if (minim_consp(o)) write_pair(out, o);
    else if (minim_vectorp(o)) write_vector(out, o);
    else if (minim_boxp(o)) write_box(out, o);
    else if (minim_primp(o)) fprintf(out, "#<procedure:%s>", minim_prim_name(o));
    else if (minim_closurep(o)) write_closure(out, o);
    else if (minim_input_portp(o)) fputs("#<input-port>", out);
    else if (minim_output_portp(o)) fputs("#<output-port>", out);
    else if (minim_hashtablep(o)) fputs("#<hashtable>", out);
    else if (minim_recordp(o)) write_record(out, o);
    else if (minim_syntaxp(o)) fputs("#<syntax>", out);
    else if (minim_envp(o)) fputs("#<environment>", out);
    else if (minim_jitp(o)) fputs("#<code>", out);
    else fputs("#<garbage>", out);
}
