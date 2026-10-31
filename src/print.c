// print.c: basic printing

#include "minim.h"

static void write_char(FILE *out, obj o) {
    int c = Mchar_value(o);

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

static void write_pair(FILE *out, obj o) {
    fputc('(', out);
loop:
    write_obj(out, Mcar(o));
    if (Mconsp(Mcdr(o))) {
        // middle of a (proper/improper) list
        fputc(' ', out);
        o = Mcdr(o);
        goto loop;
    } else if (!Mnullp(Mcdr(o))) {
        // end of an improper list
        fputs(" . ", out);
        write_obj(out, Mcdr(o));
    }

    fputc(')', out);
}

static void write_vector(FILE *out, obj o) {
    uptr len, i;

    len = Mvector_len(o);
    if (len == 0) {
        fputs("#()", out);
    }  else {
        fputs("#(", out);
        write_obj(out, Mvector_ref(o, 0));
    
        for (i = 1; i < len; i++) {
            fputc(' ', out);
            write_obj(out, Mvector_ref(o, i));
        }

        fputc(')', out);
    }
}

static void write_module(FILE *out, obj o) {
    obj path, subpath;

    path = Mmodule_path(o);
    subpath = Mmodule_subpath(o);
    if (Mfalsep(path) && Mfalsep(subpath)) {
        fputs("#<module>", out);
    } else if (Mfalsep(subpath)) {
        fputs("#<module:", out);
        write_obj(out, path);
        fputc('>', out);
    } else if (Mfalsep(path)) {
        fputs("#<module:#f ", out);
        write_obj(out, subpath);
        fputc('>', out);
    } else {
        fputs("#<module:", out);
        write_obj(out, path);
        fputc(' ', out);
        write_obj(out, subpath);
        fputc('>', out);
    }
}

void write_obj(FILE *out, obj o) {
    if (Mnullp(o)) {
        fputs("()", out);
    } else if (Mtruep(o)) {
        fputs("#t", out);
    } else if (Mfalsep(o)) {
        fputs("#f", out);
    } else if (Mvoidp(o)) {
        fputs("#<void>", out);
    } else if (Meofp(o)) {
        fputs("#<eof>", out);
    } else if (Munboundp(o)) {
        fputs("#<unbound>", out);
    } else if (Msymbolp(o)) {
        fputs(Msymbol_value(o), out);
    } else if (Mvaluesp(o)) {
        fputs("#<mvvalues>", out);
    } else if (Mfixnump(o)) {
        fprintf(out, "%ld", Mfixnum_value(o));
    } else if (Mcharp(o)) {
        write_char(out, o);
    } else if (Mstringp(o)) {
        fprintf(out, "\"%s\"", Mstring_value(o));
    } else if (Mconsp(o)) {
        write_pair(out, o);
    } else if (Mvectorp(o)) {
        write_vector(out, o);
    } else if (Mprimp(o)) {
        fputs("#<procedure:", out);
        write_obj(out, Mprim_name(o));
        fputc('>', out);
    } else if (Mclosurep(o)) {
        if (Mfalsep(Mclosure_name(o))) {
            fputs("#<procedure>", out);
        } else {
            fputs("#<procedure:", out);
            write_obj(out, Mclosure_name(o));
            fputc('>', out);
        }
    } else if (Minput_portp(o)) {
        fputs("#<input-port>", out);
    } else if (Moutput_portp(o)) {
        fputs("#<output-port>", out);
    } else if (Mcontinuationp(o)) {
        fputs("#<procedure>", out);
    } else if (Mmodulep(o)) {
        write_module(out, o);
    } else {
        fputs("#<garbage>", out);
    }
}
