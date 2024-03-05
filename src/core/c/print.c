// print.c: printer

#include "../minim.h"

static void write_pair(FILE *out, mobj p, int quote, int display) {
    write_object2(out, minim_car(p), quote, display);
    if (minim_consp(minim_cdr(p))) {
        fputc(' ', out);
        write_pair(out, minim_cdr(p), quote, display);
    } else if (minim_nullp(minim_cdr(p))) {
        return;
    } else {
        fprintf(out, " . ");
        write_object2(out, minim_cdr(p), quote, display);
    }
}

void write_object2(FILE *out, mobj o, int quote, int display) {
    minim_thread *th;
    mobj it;
    char *str;
    long stashc, i;

    if (minim_nullp(o)) {
        if (!quote) fputc('\'', out);
        fprintf(out, "()");
    } else if (minim_truep(o)) {
        fprintf(out, "#t");
    } else if (minim_falsep(o)) {
        fprintf(out, "#f");
    } else if (minim_eofp(o)) {
        fprintf(out, "#<eof>");
    } else if (minim_voidp(o)) {
        fprintf(out, "#<void>");
    } else if (minim_base_rtdp(o)) {
        fprintf(out, "#<base-record-type>");
    } else {
        switch (minim_type(o)) {
        case MINIM_OBJ_CHAR:
            if (display) {
                fputc(minim_char(o), out);
            } else {
                switch (minim_char(o)) {
                case '\n':
                    fprintf(out, "#\\newline");
                    break;
                case ' ':
                    fprintf(out, "#\\space");
                    break;
                default:
                    fprintf(out, "#\\%c", minim_char(o));
                }
            }
            break;
        case MINIM_OBJ_FIXNUM:
            fprintf(out, "%ld", minim_fixnum(o));
            break;
        case MINIM_OBJ_SYMBOL:
            if (!quote) fputc('\'', out);
            fprintf(out, "%s", minim_symbol(o));
            break;
        case MINIM_OBJ_STRING:
            str = minim_string(o);
            if (display) {
                fprintf(out, "%s", str);
            } else {
                fputc('"', out);
                while (*str != '\0') {
                    switch (*str) {
                    case '\n':
                        fprintf(out, "\\n");
                        break;
                    case '\t':
                        fprintf(out, "\\t");
                        break;
                    case '\\':
                        fprintf(out, "\\\\");
                        break;
                    default:
                        fputc(*str, out);
                        break;
                    }
                    ++str;
                }
                fputc('"', out);
            }
            break;
        case MINIM_OBJ_PAIR:
            if (!quote) fputc('\'', out);
            fputc('(', out);
            write_pair(out, o, 1, display);
            fputc(')', out);
            break;
        case MINIM_OBJ_VECTOR:
            if (!quote) fputc('\'', out);
            if (minim_vector_len(o) == 0) {
                fputs("#()", out);
            } else {
                fputs("#(", out);
                write_object2(out, minim_vector_ref(o, 0), 1, display);
                for (long i = 1; i < minim_vector_len(o); ++i) {
                    fputc(' ', out);
                    write_object2(out, minim_vector_ref(o, i), 1, display);
                }
                fputc(')', out);
            }
            break;
        case MINIM_OBJ_BOX:
            if (!quote) fputc('\'', out);
            fputs("#&", out);
            write_object2(out, minim_unbox(o), 1, display);
            break;
        case MINIM_OBJ_PRIM:
            fprintf(out, "#<procedure:%s>", minim_prim_name(o));
            break;
        case MINIM_OBJ_PRIM2:
            fprintf(out, "#<procedure:%s>", minim_prim2_name(o));
            break;
        case MINIM_OBJ_CLOSURE:
            if (minim_closure_name(o) != NULL)
                fprintf(out, "#<procedure:%s>", minim_closure_name(o));
            else
                fprintf(out, "#<procedure>");
            break;
        case MINIM_OBJ_NATIVE_CLOSURE:
            if (minim_native_closure_name(o) != NULL)
                fprintf(out, "#<procedure:%s>", minim_native_closure_name(o));
            else
                fprintf(out, "#<procedure>");
            break;
        case MINIM_OBJ_PORT:
            if (minim_port_readp(o))
                fprintf(out, "#<input-port>");
            else
                fprintf(out, "#<output-port>");
            break;
        case MINIM_OBJ_HASHTABLE:
            if (minim_hashtable_count(o) == 0) {
                fprintf(out, "#<hashtable>");
            } else {
                fprintf(out, "#<hashtable");
                for (i = 0; i < minim_hashtable_alloc(o); ++i) {
                    it = minim_hashtable_bucket(o, i);
                    if (it) {
                        for (; !minim_nullp(it); it = minim_cdr(it)) {
                            fputs(" (", out);
                            write_object2(out, minim_caar(it), 1, display);
                            fputs(" . ", out);
                            write_object2(out, minim_cdar(it), 1, display);
                            fputc(')', out);
                        }
                    }
                }
                fputc('>', out);
            }
            break;
        case MINIM_OBJ_RECORD:
            if (minim_record_rtd(o) == minim_base_rtd) {
                // record type descriptor
                fprintf(out, "#<record-type:%s>", minim_symbol(record_rtd_name(o)));
            } else {
                // record value
                th = current_thread();
                stashc = stash_call_args();

                push_call_arg(o);
                push_call_arg(Moutput_port(out));   // TODO: problematic
                push_call_arg(Mcons(Mfixnum(quote), Mfixnum(display)));
                call_with_args(record_write_proc(th), global_env(th));

                prepare_call_args(stashc);
            }
            break;
        case MINIM_OBJ_SYNTAX:
            fprintf(out, "#<syntax:");
            write_object2(out, strip_syntax(o), 1, display);
            fputc('>', out);
            break;
        case MINIM_OBJ_PATTERN:
            fprintf(out, "#<pattern>");
            break;
        case MINIM_OBJ_ENV:
            fprintf(out, "#<environment>");
            break;
        default:
            fprintf(out, "#<garbage:%p>", o);
            break;
        }
    }
}

void write_object(FILE *out, mobj o) {
    write_object2(out, o, 0, 0);
}
