/*
    A small interpreter for bootstrapping Minim.
    Supports the most basic operations
*/

#include "../minim.h"
#include "boot.h"
#include "intern.h"

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
intern_table *symbols;

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

minim_object *make_prim_proc(minim_prim_proc_t proc) {
    minim_prim_proc_object *o = GC_alloc(sizeof(minim_prim_proc_object));
    o->type = MINIM_PRIM_PROC_TYPE;
    o->fn = proc;
    return ((minim_object *) o);
}

minim_object *make_closure_proc(minim_object *args, minim_object *body, minim_object *env) {
    minim_closure_proc_object *o = GC_alloc(sizeof(minim_closure_proc_object));
    o->type = MINIM_CLOSURE_PROC_TYPE;
    o->args = args;
    o->body = body;
    o->env = env;
    return ((minim_object *) o);
}

//
//  Extra constructors
//

// Makes an association list.
// Unsafe: only iterates on `xs`.
minim_object *make_assoc(minim_object *xs, minim_object *ys) {
    minim_object *assoc, *it;

    if (minim_is_null(xs))
        return minim_null;

    assoc = make_pair(make_pair(minim_car(xs), minim_car(ys)), minim_null);
    it = assoc;
    while (!minim_is_null(xs = minim_cdr(xs))) {
        ys = minim_cdr(ys);
        minim_cdr(it) = make_pair(make_pair(minim_car(xs), minim_car(ys)), minim_null);
        it = minim_cdr(it);
    }

    return assoc;
}

//
//  Forward declarations
//

minim_object *read_object(FILE *in);
void write_object(FILE *out, minim_object *o);
minim_object *eval_expr(minim_object *expr, minim_object *env);
minim_object *make_env();

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
        buffer[i] = '\0';
        return intern_symbol(symbols, buffer);
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
//  environments ::= (<frame0> <frame1> ...)
//  frames       ::= ((<var0> . <val1>) (<var1> . <val1>) ...)
//

minim_object *make_frame(minim_object *vars, minim_object *vals) {
    return make_assoc(vars, vals);
}

minim_object *env_define_var(minim_object *env, minim_object *var, minim_object *val) {
    minim_object *frame = minim_car(env);
    for (minim_object *it = frame; !minim_is_null(it); it = minim_cdr(it)) {
        minim_object *frame_var = minim_caar(frame);
        minim_object *frame_val = minim_cdar(frame);
        if (var == frame_var) {
            minim_cdar(frame) = val;
            return frame_val;
        }
    }

    minim_car(env) = make_pair(make_pair(var, val), frame);
    return NULL;
}

minim_object *env_set_var(minim_object *env, minim_object *var, minim_object *val) {
    while (!minim_is_null(env)) {
        minim_object *frame = minim_car(env);
        while (!minim_is_null(frame)) {
            minim_object *frame_var = minim_caar(frame);
            minim_object *frame_val = minim_cdar(frame);
            if (var == frame_var) {
                minim_cdar(frame) = val;
                return frame_val;
            }

            frame = minim_cdr(frame);
        }

        env = minim_cdr(env);
    }

    fprintf(stderr, "unbound variable: %s\n", minim_symbol(var));
    exit(1);
}

minim_object *env_lookup_var(minim_object *env, minim_object *var) {
    while (!minim_is_null(env)) {
        minim_object *frame = minim_car(env);
        while (!minim_is_null(frame)) {
            minim_object *frame_var = minim_caar(frame);
            minim_object *frame_val = minim_cdar(frame);
            if (var == frame_var)
                return frame_val;

            frame = minim_cdr(frame);
        }

        env = minim_cdr(env);
    }

    fprintf(stderr, "unbound variable: %s\n", minim_symbol(var));
    exit(1);
}

minim_object *extend_env(minim_object *vars,
                         minim_object *vals,
                         minim_object *base_env) {
    return make_pair(make_frame(vars, vals), base_env);
}

minim_object *setup_env() {
    return extend_env(minim_null, minim_null, empty_env);
}   

//
//  Primitive library
//

minim_object *is_null_proc(minim_object *args) {
    return minim_is_null(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_bool_proc(minim_object *args) {
    minim_object *o = minim_car(args);
    return (minim_is_true(o) || minim_is_false(o)) ? minim_true : minim_false;
}

minim_object *is_symbol_proc(minim_object *args) {
    return minim_is_symbol(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_fixnum_proc(minim_object *args) {
    return minim_is_fixnum(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_char_proc(minim_object *args) {
    return minim_is_char(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_string_proc(minim_object *args) {
    return minim_is_char(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_pair_proc(minim_object *args) {
    return minim_is_pair(minim_car(args)) ? minim_true : minim_false;
}

minim_object *is_procedure_proc(minim_object *args) {
    minim_object *o = minim_car(args);
    return (minim_is_prim_proc(o) || minim_is_closure_proc(o)) ? minim_true : minim_false;
}

minim_object *char_to_integer_proc(minim_object *args) {
    return make_fixnum(minim_char(minim_car(args)));
}

minim_object *integer_to_char_proc(minim_object *args) {
    return make_char(minim_fixnum(minim_car(args)));
}

minim_object *number_to_string_proc(minim_object *args) {
    char buffer[30];
    sprintf(buffer, "%ld", minim_fixnum(minim_car(args)));
    return make_string(buffer);
}

minim_object *string_to_number_proc(minim_object *args) {
    // TODO: unchecked conversion
    return make_fixnum(atoi(minim_string(minim_car(args))));
}

minim_object *symbol_to_string_proc(minim_object *args) {
    return make_string(minim_symbol(minim_car(args)));
}

minim_object *string_to_symbol_proc(minim_object *args) {
    return intern_symbol(symbols, minim_string(minim_car(args)));
}

minim_object *eval_proc(minim_object *args) {
    fprintf(stderr, "eval: should not be called directly");
    exit(1);
}

minim_object *apply_proc(minim_object *args) {
    fprintf(stderr, "eval: should not be called directly");
    exit(1);
}

minim_object *interaction_environment_proc(minim_object *args) {
    return global_env;
}

minim_object *empty_environment_proc(minim_object *args) {
    return setup_env();
}

minim_object *environment_proc(minim_object *args) {
    return make_env();
}

//
//  Evaluation
//

static int is_pair_starting_with(minim_object *maybe, minim_object *key) {
    return minim_is_pair(maybe) && minim_is_symbol(minim_car(maybe)) && minim_car(maybe) == key;
}

static int is_quoted(minim_object *expr) {
    return is_pair_starting_with(expr, quote_symbol);
}

static int is_assignment(minim_object *expr) {
    return is_pair_starting_with(expr, setb_symbol);
}

static int is_definition(minim_object *expr) {
    return is_pair_starting_with(expr, define_symbol);
}

static int is_let(minim_object *expr) {
    return is_pair_starting_with(expr, let_symbol);
}

static int is_if(minim_object *expr) {
    return is_pair_starting_with(expr, if_symbol);
}

static int is_cond(minim_object *expr) {
    return is_pair_starting_with(expr, cond_symbol);
}

static int is_lambda(minim_object *expr) {
    return is_pair_starting_with(expr, lambda_symbol);
}

static int is_begin(minim_object *expr) {
    return is_pair_starting_with(expr, begin_symbol);
}

static int is_and(minim_object *expr) {
    return is_pair_starting_with(expr, and_symbol);
}

static int is_or(minim_object *expr) {
    return is_pair_starting_with(expr, or_symbol);
}

static minim_object *let_vars(minim_object *bindings) {
    return (minim_is_null(bindings) ?
            minim_null :
            make_pair(minim_caar(bindings), let_vars(minim_cdr(bindings))));
}

static minim_object *let_vals(minim_object *bindings) {
    return (minim_is_null(bindings) ?
            minim_null :
            make_pair(minim_car(minim_cdar(bindings)), let_vals(minim_cdr(bindings))));
}

static minim_object *eval_exprs(minim_object *exprs, minim_object *env) {
    if (minim_is_null(exprs)) {
        return minim_null;
    } else {
        return make_pair(eval_expr(minim_car(exprs), env),
                         eval_exprs(minim_cdr(exprs), env));
    }
}

minim_object *eval_expr(minim_object *expr, minim_object *env) {
    minim_object *proc;
    minim_object *args;
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
        return env_lookup_var(env, expr);
    } else if (is_quoted(expr)) {
        return minim_cadr(expr);
    } else if (is_assignment(expr)) {
        env_set_var(env, minim_cadr(expr), eval_expr(minim_car(minim_cddr(expr)), env));
        return minim_void;
    } else if (is_definition(expr)) {
        env_define_var(env, minim_cadr(expr), eval_expr(minim_car(minim_cddr(expr)), env));
        return minim_void;
    } else if (is_let(expr)) {
        env = extend_env(let_vars(minim_cadr(expr)), let_vals(minim_cadr(expr)), env);
        expr = minim_car(minim_cddr(expr));
        goto loop;
    } else if (is_if(expr)) {
        if (minim_is_true(eval_expr(minim_cadr(expr), env)))
            expr = minim_car(minim_cddr(expr));
        else
            expr = minim_cadr(minim_cddr(expr));

        goto loop;
    } else if (is_cond(expr)) {
        expr = minim_cdr(expr);
        while (!minim_is_null(expr)) {
            if (minim_is_true(eval_expr(minim_caar(expr), env))) {
                expr = make_pair(begin_symbol, minim_cdar(expr));
                goto loop;
            }
            expr = minim_cdr(expr);
        }
        return minim_void;
    } else if (is_lambda(expr)) {
        return make_closure_proc(minim_cadr(expr), minim_car(minim_cddr(expr)), env);
    } else if (is_begin(expr)) {
        expr = minim_cdr(expr);
        while (!minim_is_null(minim_cdr(expr))) {
            eval_expr(minim_car(expr), env);
            expr = minim_cdr(expr);
        }

        expr = minim_car(expr);
        goto loop;
    } else if (is_and(expr)) {
        expr = minim_cdr(expr);
        if (minim_is_null(expr))
            return minim_true;

        while (!minim_is_null(minim_cdr(expr))) {
            result = eval_expr(minim_car(expr), env);
            if (minim_is_false(result))
                return result;
            expr = minim_cdr(expr);
        }
        
        expr = minim_car(expr);
        goto loop;
    } else if (is_or(expr)) {
        expr = minim_cdr(expr);
        if (minim_is_null(expr))
            return minim_false;

        while (!minim_is_null(minim_cdr(expr))) {
            result = eval_expr(minim_car(expr), env);
            if (minim_is_true(result))
                return result;
            expr = minim_cdr(expr);
        }
        
        expr = minim_car(expr);
        goto loop;
    } else if (minim_is_pair(expr)) {
        proc = eval_expr(minim_car(expr), env);
        args = eval_exprs(minim_cdr(expr), env);

application:

        if (minim_is_prim_proc(proc)) {
            // special case for `eval`
            if (minim_prim_proc(proc) == eval_proc) {
                expr = minim_car(args);
                env = minim_cadr(args);
                goto loop;
            }

            // special case for `apply`
            if (minim_prim_proc(proc) == apply_proc) {
                proc = minim_car(args);
                args = minim_cdr(args);
                goto application;
            }

            return minim_prim_proc(proc)(args);
        } else if (minim_is_closure_proc(proc)) {
            env = extend_env(minim_closure_args(proc),
                       args,
                       minim_closure_env(proc));
            expr = make_pair(begin_symbol, minim_closure_body(proc));
            goto loop;
        } else {
            fprintf(stderr, "not a procedure\n");
            exit(1);
        }
    } else {
        fprintf(stderr, "cannot evaluate expresion\n");
        exit(1);
    }

    fprintf(stderr, "unreachable\n");
    exit(1);
}

//
//  Printing
//

void write_pair(FILE *out, minim_pair_object *p) {
    write_object(out, minim_car(p));
    if (minim_is_pair(minim_cdr(p))) {
        fputc(' ', out);
        write_pair(out, ((minim_pair_object *) minim_cdr(p)));
    } else if (minim_is_null(minim_cdr(p))) {
        return;
    } else {
        fprintf(out, " . ");
        write_object(out, minim_cdr(p));
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
        fprintf(out, "%s", minim_symbol(o));
        break;
    case MINIM_FIXNUM_TYPE:
        fprintf(out, "%ld", minim_fixnum(o));
        break;
    case MINIM_CHAR_TYPE:
        int c = minim_char(o);
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
        char *str = minim_string(o);
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

#define add_procedure(name, c_name)         \
    env_define_var(env, intern_symbol(symbols, name), make_prim_proc(c_name))

void populate_env(minim_object *env) {
    add_procedure("null?", is_null_proc);
    add_procedure("boolean?", is_bool_proc);
    add_procedure("symbol?", is_bool_proc);
    add_procedure("integer?", is_fixnum_proc);
    add_procedure("char?", is_char_proc);
    add_procedure("string?", is_string_proc);
    add_procedure("procedure?", is_procedure_proc);

    add_procedure("char->integer", char_to_integer_proc);
    add_procedure("integer->char", integer_to_char_proc);
    add_procedure("number->string", number_to_string_proc);
    add_procedure("string->number", string_to_number_proc);
    add_procedure("symbol->string", symbol_to_string_proc);
    add_procedure("string->symbol", string_to_symbol_proc);

    add_procedure("interaction-environment", interaction_environment_proc);
    add_procedure("null-environment", empty_environment_proc);
    add_procedure("environment", environment_proc);

    add_procedure("eval", eval_proc);
    add_procedure("apply", apply_proc);
}

minim_object *make_env() {
    minim_object *env;

    env = setup_env();
    populate_env(env);
    return env;
}

void minim_boot_init() {
    symbols = make_intern_table();

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

    quote_symbol = intern_symbol(symbols, "quote");
    define_symbol = intern_symbol(symbols, "define");
    setb_symbol = intern_symbol(symbols, "set!");
    if_symbol = intern_symbol(symbols, "if");
    lambda_symbol = intern_symbol(symbols, "lambda");
    begin_symbol = intern_symbol(symbols, "begin");
    cond_symbol = intern_symbol(symbols, "cond");
    else_symbol = intern_symbol(symbols, "else");
    let_symbol = intern_symbol(symbols, "let");
    and_symbol = intern_symbol(symbols, "and");
    or_symbol = intern_symbol(symbols, "or");

    empty_env = minim_null;
    global_env = make_env();
}

//
//  REPL
//

int main(int argv, char **argc) {
    volatile int stack_top;
    minim_object *expr, *evaled;

    printf("Minim Bootstrap Interpreter v%s\n", MINIM_BOOT_VERSION);

    GC_init(((void*) &stack_top));
    minim_boot_init();
    
    while (1) {
        printf("> ");
        expr = read_object(stdin);
        if (expr == NULL) break;

        evaled = eval_expr(expr, global_env);
        if (!minim_is_void(evaled)) {
            write_object(stdout, evaled);
            printf("\n");
        }
    }

    return 0;
}
