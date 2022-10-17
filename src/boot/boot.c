/*
    A small interpreter for bootstrapping Minim.
    Supports the most basic operations
*/

#include "../minim.h"
#include "boot.h"

//
//  Special objects
//

minim_object *minim_null;
minim_object *minim_true;
minim_object *minim_false;
minim_object *minim_eof;
minim_object *minim_void;

//
//  Object initialization
//

minim_object *make_char(int c) {
    minim_char_object *o = GC_alloc(sizeof(minim_char_object));
    o->type = MINIM_CHAR_TYPE;
    o->value = c;
    return ((minim_object *) o);
}

//
//  Forward declarations
//

minim_object *read_object(FILE *in);
void write_object(FILE *out, minim_object *o);

//
//  Parsing
//

int is_delimeter(int c) {
    return isspace(c) || c == EOF || c == '(' || c == ')' || c == '"' || c == ';';
}

int peek_char(FILE *in) {
    int c = getc(in);
    ungetc(c, in);
    return c;
}

void peek_expected_delimeter(FILE *in) {
    if (!is_delimeter(peek_char(in))) {
        fprintf(stderr, "expected a delimeter\n");
        exit(1);
    }
}

void read_expected_string(FILE *in, const char *s) {
    int c;

    while (*s != '\0') {
        c = fgetc(in);
        if (c != *s) {
            fprintf(stderr, "unexpected character: '%c'\n", c);
            exit(1);
        }
        ++s;
    }
}

void skip_whitespace(FILE *in) {
    int c;

    while ((c = fgetc(in)) != EOF) {
        if (isspace(c))
            continue;
        else if (c == ';') {
            // comment: ignore until newline
            while (((c = fgetc(in)) != EOF) && (c != '\n'));
        }
        
        // too far
        ungetc(c, in);
        break;
    }
}

minim_object *read_char(FILE *in) {
    int c;

    c = fgetc(in);
    switch (c) {
    case EOF:
        fprintf(stderr, "incomplete character literal\n");
        break;
    case 's':
        if (peek_char(in) == 'p') {
            read_expected_string(in, "pace");
            peek_expected_delimeter(in);
            return make_char(' ');
        }
        break;
    case 'n':
        if (peek_char(in) == 'e') {
            read_expected_string(in, "ewline");
            peek_expected_delimeter(in);
            return make_char('\n');
        }
        break;
    default:
        break;
    }

    peek_expected_delimeter(in);
    return make_char(c);
}

minim_object *read_object(FILE *in) {
    char c;
    
    skip_whitespace(in);
    c = fgetc(in);

    if (c == '#') {
        // special value
        c = fgetc(in);
        switch (c) {
        case 't':
            return minim_true;
        case 'f':
            return minim_false;
        case '\\':
            return read_char(in);
        }
    }


    return minim_null;   
}

//
//  Printing
//

void write_pair(FILE *out, minim_pair_object *p) {
    write_object(out, p->car);
    if (p->cdr->type == MINIM_PAIR_TYPE) {
        fputc(' ', out);
        write_pair(out, ((minim_pair_object *) p->cdr));
    } else if (p->cdr->type == MINIM_NULL_TYPE) {
        return;
    } else {
        fprintf(out, " . ");
        write_object(out, p->cdr);
    }
}

void write_object(FILE *out, minim_object *o) {
    switch (o->type) {
    case MINIM_NULL_TYPE:
        fprintf(out, "()");
        break;
    case MINIM_TRUE_TYPE:
        fprintf(out, "#t");
        break;
    case MINIM_FALSE_TYPE:
        fprintf(out, "#f");
        break;
    case MINIM_EOF_TYPE:
        fprintf(out, "#<eof>");
        break;
    case MINIM_VOID_TYPE:
        fprintf(out, "#<void>");
        break;

    case MINIM_SYMBOL_TYPE:
        fprintf(out, "%s", ((minim_symbol_object *) o)->value);
        break;
    case MINIM_FIXNUM_TYPE:
        fprintf(out, "%ld", ((minim_fixnum_object *) o)->value);
        break;
    case MINIM_CHAR_TYPE:
        switch (((minim_char_object *) o)->value) {
        case '\n':
            fprintf(out, "newline");
            break;
        case ' ':
            fprintf(out, "space");
            break;
        default:
            fprintf(out, "\\%c", ((minim_char_object *) o)->value);
        }
    case MINIM_STRING_TYPE:
        char *str = ((minim_string_object *) o)->value;
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
        break;
    case MINIM_PAIR_TYPE:
        fputc('(', out);
        write_pair(out, ((minim_pair_object *) o));
        fputc(')', out);
        break;
    case MINIM_PRIM_PROC_TYPE:
    case MINIM_CLOSURE_PROC_TYPE:
        fprintf(out, "#<procedure>");
        break;
    case MINIM_OUTPUT_PORT_TYPE:
        fprintf(out, "#<output-port>");
        break;
    case MINIM_INPUT_PORT_TYPE:
        fprintf(out, "#<input-port>");
        break;
    
    default:
        fprintf(stderr, "cannot write unknown object");
        exit(1);
    }
}

//
//  Interpreter initialization
//

void minim_boot_init() {
    minim_null = GC_alloc(sizeof(minim_object));
    minim_true = GC_alloc(sizeof(minim_object));
    minim_false = GC_alloc(sizeof(minim_object));
    minim_eof = GC_alloc(sizeof(minim_object));
    minim_void = GC_alloc(sizeof(minim_object));

    minim_null->type = MINIM_NULL_TYPE;
    minim_true->type = MINIM_TRUE_TYPE;
    minim_false->type = MINIM_FALSE_TYPE;
    minim_eof->type = MINIM_EOF_TYPE;
    minim_void->type = MINIM_VOID_TYPE;
}

//
//  REPL
//

int main(int argv, char **argc) {
    volatile int stack_top;
    minim_object *expr;

    printf("Minim bootstrap interpreter (v%s)\n", MINIM_BOOT_VERSION);

    GC_init(((void*) &stack_top));
    minim_boot_init();
    
    while (1) {
        printf("> ");
        expr = read_object(stdin);
        if (expr == NULL) break;

        write_object(stdout, expr);
        printf("\n");
    }

    return 0;
}
