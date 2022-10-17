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

minim_object *quote_symbol;
minim_object *define_symbol;
minim_object *setb_symbol;
minim_object *if_symbol;
minim_object *lambda_symbol;
minim_object *begin_symbol;
minim_object *cond_symbol;
minim_object *else_symbol;
minim_object *let_symbol;
minim_object *and_symbol;
minim_object *or_symbol;

minim_object *empty_env;
minim_object *global_env;


//
//  Constructors
//

minim_object *make_char(int c) {
    minim_char_object *o = GC_alloc(sizeof(minim_char_object));
    o->type = MINIM_CHAR_TYPE;
    o->value = c;
    return ((minim_object *) o);
}

minim_object *make_fixnum(long v) {
    minim_fixnum_object *o = GC_alloc(sizeof(minim_fixnum_object));
    o->type = MINIM_FIXNUM_TYPE;
    o->value = v;
    return ((minim_object *) o);
}

minim_object *make_symbol(const char *s) {
    minim_symbol_object *o = GC_alloc(sizeof(minim_symbol_object));
    int len = strlen(s);
    
    o->type = MINIM_SYMBOL_TYPE;
    o->value = GC_alloc((len + 1) * sizeof(char));
    strncpy(o->value, s, len + 1);
    return ((minim_object *) o);
}

minim_object *make_string(const char *s) {
    minim_string_object *o = GC_alloc(sizeof(minim_string_object));
    int len = strlen(s);
    
    o->type = MINIM_STRING_TYPE;
    o->value = GC_alloc((len + 1) * sizeof(char));
    strncpy(o->value, s, len + 1);
    return ((minim_object *) o);
}

minim_object *make_pair(minim_object *car, minim_object *cdr) {
    minim_pair_object *o = GC_alloc(sizeof(minim_pair_object));
    o->type = MINIM_PAIR_TYPE;
    o->car = car;
    o->cdr = cdr;
    return ((minim_object *) o);
}

//
//  Extra constructors
//

minim_object *make_assoc(minim_object *xs, minim_object *ys) {
    minim_object *assoc, *assoc_it;

    if (minim_is_null(xs) && minim_is_null(ys))
        return minim_null;
    
    if (minim_is_null(xs) != minim_is_null(ys)) {
        fprintf(stderr, "association lists require even lists\n");
        exit(1);
    }

    assoc = make_pair(make_pair(minim_car(xs), minim_car(ys)), minim_null);
    assoc_it = assoc;
    // while (!minim_is_null(xs = minim_cdr(xs))) {
    //     if (minim_is_null(ys))
    // }


    return assoc;
}

//
//  Forward declarations
//

minim_object *read_object(FILE *in);
void write_object(FILE *out, minim_object *o);

//
//  Standard library
//



// minim_object *is_null_proc(minim_object **arguments) {
//     return 
// }

//
//  Parsing
//

int is_delimeter(int c) {
    return isspace(c) || c == EOF || c == '(' || c == ')' || c == '"' || c == ';';
}

int is_symbol_char(int c) {
    return isalpha(c) || c == '*' || c == '/' ||
           c == '<' || c == '>' || c == '=' || c == '?' || c == '!';
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
        c = getc(in);
        if (c != *s) {
            fprintf(stderr, "unexpected character: '%c'\n", c);
            exit(1);
        }
        ++s;
    }
}

void skip_whitespace(FILE *in) {
    int c;

    while ((c = getc(in)) != EOF) {
        if (isspace(c))
            continue;
        else if (c == ';') {
            // comment: ignore until newline
            while (((c = getc(in)) != EOF) && (c != '\n'));
        }
        
        // too far
        ungetc(c, in);
        break;
    }
}

minim_object *read_char(FILE *in) {
    int c;

    c = getc(in);
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

minim_object *read_pair(FILE *in) {
    minim_object *car, *cdr;
    int c;

    skip_whitespace(in);
    c = getc(in);
    if (c == ')') {
        // empty list
        return minim_null;
    }

    ungetc(c, in);
    car = read_object(in);

    skip_whitespace(in);
    c = fgetc(in);
    if (c == '.') {
        // improper list
        peek_expected_delimeter(in);
        cdr = read_object(in);
        skip_whitespace(in);

        c = getc(in);
        if (c != ')') {
            fprintf(stderr, "missing ')' to terminate pair");
            exit(1);
        }

        return make_pair(car, cdr);
    } else {
        // list
        ungetc(c, in);
        cdr = read_pair(in);
        return make_pair(car, cdr);
    }
}

minim_object *read_object(FILE *in) {
    char buffer[SYMBOL_MAX_LEN];
    long num;
    int i;
    short sign;
    char c;
    
    skip_whitespace(in);
    c = getc(in);

    if (c == '#') {
        // special value
        c = getc(in);
        switch (c) {
        case 't':
            return minim_true;
        case 'f':
            return minim_false;
        case '\\':
            return read_char(in);
        }
    } else if (isdigit(c) || ((c == '-' || c == '+') && isdigit(peek_char(in)))) {
        // number
        num = 0;
        sign = 1;

        // optional sign
        if (c == '-') {
            sign = -1;
        } else if (c != '+') {
            ungetc(c, in);
        }

        // magnitude
        while (isdigit(c = getc(in))) {
            num = (num * 10) + (c - '0');
        }

        // compose and check for delimeter
        num *= sign;
        if (!is_delimeter(c)) {
            fprintf(stderr, "expected a delimeter\n");
            exit(1);
        }

        ungetc(c, in);
        return make_fixnum(num);
    } else if (is_symbol_char(c) || ((c == '+' || c == '-') && is_delimeter(peek_char(in)))) {
        // symbol
        i = 0;

        while (is_symbol_char(c) || isdigit(c) || c == '+' || c == '-') {
            if (i < SYMBOL_MAX_LEN - 1) {
                buffer[i++] = c;
            } else {
                fprintf(stderr, "symbol is too long, max allowed %d characters", SYMBOL_MAX_LEN);
                exit(1);
            }
            c = getc(in);
        }

        if (!is_delimeter(c)) {
            fprintf(stderr, "expected a delimeter\n");
            exit(1);
        }

        ungetc(c, in);
        return make_symbol(buffer);
    } else if (c == '"') {
        // string
        i = 0;
        while ((c = getc(in)) != '"') {
            if (c == '\\') {
                c = getc(in);
                if (c == 'n') {
                    c = '\n';
                } else if (c == 't') {
                    c = '\t';
                } else if (c == '\\') {
                    c = '\\';
                } else {
                    fprintf(stderr, "unknown escape character: %c\n", c);
                    exit(1);
                }
            } else if (c == EOF) {
                fprintf(stderr, "non-terminated string literal\n");
                exit(1);
            }

            if (i < SYMBOL_MAX_LEN - 1) {
                buffer[i++] = c;
            } else {
                fprintf(stderr, "string is too long, max allowed %d characters", SYMBOL_MAX_LEN);
                exit(1);
            }
        }

        buffer[i] = '\0';
        return make_string(buffer);
    } else if (c == '(') {
        // empty list or pair
        return read_pair(in);
    } else if (c == '\'') {
        // quoted expression
        return make_pair(quote_symbol, make_pair(read_object(in), minim_null));
    } else if (c == EOF) {
        return NULL;
    } else {
        fprintf(stderr, "unexpected input: %c\n", c);
        exit(1);
    }

    fprintf(stderr, "unreachable");
    exit(1);
}

//
//  Environments
//
//  environments are a list of frames
//  frames are list of bindings associating names to values
//

minim_object *make_env_frame(minim_object *vars, minim_object *vals) {
    return make_assoc(vars, vals);
}

minim_object *extend_env(minim_object *vars,
                         minim_object *vals,
                         minim_object *base_env) {
    return make_pair(make_env_frame(vars, vals), base_env);
}

minim_object *setup_env() {
    return extend_env(minim_null, minim_null, empty_env);
}

void populate_env(minim_object *env) {
    
}

minim_object *make_env() {
    minim_object *env;

    env = setup_env();
    populate_env(env);
    return env;
}

//
//  Evaluation
//

int is_pair_starting_with(minim_object *maybe, minim_object *key) {
    return minim_is_pair(maybe) && minim_is_symbol(minim_car(maybe)) && minim_car(maybe) == key;
}

int is_quoted(minim_object *expr) {
    return is_pair_starting_with(expr, quote_symbol);
}

minim_object *unpack_quote(minim_object *quote) {
    return minim_cadr(quote);
}

minim_object *eval_expr(minim_object *expr, minim_object *env) {
    minim_object *procedure;
    minim_object *arguments;
    minim_object *result;

loop:

    if (minim_is_true(expr) || minim_is_false(expr) ||
        minim_is_fixnum(expr) ||
        minim_is_char(expr) ||
        minim_is_string(expr)) {
        // self-evaluating
        return expr;
    } else if (minim_is_symbol(expr)) {
        // variable
        fprintf(stderr, "Unimplemented\n");
        exit(1);
    } else if (is_quoted(expr)) {
        return unpack_quote(expr);
    }

    fprintf(stderr, "Unimplemented\n");
    exit(1);
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
        int c = ((minim_char_object *) o)->value;
        switch (c) {
        case '\n':
            fprintf(out, "#\\newline");
            break;
        case ' ':
            fprintf(out, "#\\space");
            break;
        default:
            fprintf(out, "#\\%c", c);
        }
        break;
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

    quote_symbol = make_symbol("quote");
    define_symbol = make_symbol("define");
    setb_symbol = make_symbol("set!");
    if_symbol = make_symbol("if");
    lambda_symbol = make_symbol("lambda");
    begin_symbol = make_symbol("cond");
    else_symbol = make_symbol("else");
    let_symbol = make_symbol("let");
    and_symbol = make_symbol("and");
    or_symbol = make_symbol("or");

    empty_env = minim_null;
    global_env = make_env();
}

//
//  REPL
//

int main(int argv, char **argc) {
    volatile int stack_top;
    minim_object *expr;

    printf("Minim Bootstrap Interpreter v%s\n", MINIM_BOOT_VERSION);

    GC_init(((void*) &stack_top));
    minim_boot_init();
    
    while (1) {
        printf("> ");
        expr = read_object(stdin);
        if (expr == NULL) break;

        write_object(stdout, eval_expr(expr, global_env));
        printf("\n");
    }

    return 0;
}
