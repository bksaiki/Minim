// alloc.c: object allocation

#include "minim.h"

obj Msymbol(const char *s) {
    obj x;
    size_t l;

    x = GC_malloc(Msymbol_size);
    obj_type(x) = SYMBOL_OBJ_TYPE;

    l = strlen(s);
    Msymbol_value(x) = GC_malloc_atomic(l + 1);
    strncpy(Msymbol_value(x), s, l + 1);

    return x;
}

obj Mfixnum(iptr v) {
    obj x = GC_malloc(Mfixnum_size);
    obj_type(x) = FIXNUM_OBJ_TYPE;
    Mfixnum_value(x) = v;
    return x;
}

obj Mchar(int c) {
    obj x = GC_malloc(Mchar_size);
    obj_type(x) = CHAR_OBJ_TYPE;
    Mchar_value(x) = c;
    return x;
}

obj Mstring(const char* s) {
    obj x = GC_malloc(Mstring_size);
    obj_type(x) = STRING_OBJ_TYPE;
    Mstring_length(x) = strlen(s);
    Mstring_value(x) = GC_malloc_atomic(Mstring_length(x) + 1);
    strncpy(Mstring_value(x), s, Mstring_length(x) + 1);
    return x;
}

obj Mcons(obj car, obj cdr) {
    obj x = GC_malloc(Mcons_size);
    obj_type(x) = CONS_OBJ_TYPE;
    Mcar(x) = car;
    Mcdr(x) = cdr;
    return x;
}

obj Mvector(uptr len, obj init) {
    obj x;
    uptr i;

    if (len == 0) {
        return Memptyvec;
    } else {
        x = GC_malloc(Mvector_size(len));
        obj_type(x) = VECTOR_OBJ_TYPE;
        Mvector_len(x) = len;
        if (init != NULL) {
            for (i = 0; i < len; i++)
                Mvector_ref(x, i) = init;
        }

        return x;
    }
}

obj Mprim(void *fn, iptr arity, const char *name) {
    obj x = GC_malloc(Mprim_size);
    obj_type(x) = PRIM_OBJ_TYPE;
    Mprim_specialp(x) = 0;
    Mprim_value(x) = fn;
    Mprim_arity(x) = arity;
    Mprim_name(x) = Mintern(name);
    return x;
}

static obj formals_to_arity(obj formals) {
    iptr arity;

    for (arity = 0; Mconsp(formals); formals = Mcdr(formals), ++arity);
    return Mfixnum(Mnullp(formals) ? arity : (-arity - 1));
}

obj Mclosure(obj env, obj formals, obj body) {
    obj x = GC_malloc(Mclosure_size);
    obj_type(x) = CLOSURE_OBJ_TYPE;
    Mclosure_casep(x) = 0;
    Mclosure_env(x) = env;
    Mclosure_name(x) = Mfalse;
    Mclosure_body(x) = Mlist1(body);
    Mclosure_formals(x) = Mlist1(formals);
    Mclosure_arity(x) = Mlist1(formals_to_arity(formals));
    return x;
}

obj Mclosure2(obj env, obj clauses) {
    obj x, b_tl, f_tl, a_tl;
    
    x = GC_malloc(Mclosure_size);
    obj_type(x) = CLOSURE_OBJ_TYPE;
    Mclosure_casep(x) = 1;
    Mclosure_env(x) = env;
    Mclosure_name(x) = Mfalse;
    
    if (Mnullp(clauses)) {
        Mclosure_body(x) = Mnull;
        Mclosure_formals(x) = Mnull;
        Mclosure_arity(x) = Mnull;
    } else {
        Mclosure_body(x) = b_tl = Mlist1(Mcadar(clauses));
        Mclosure_formals(x) = f_tl = Mlist1(Mcaar(clauses));
        Mclosure_arity(x) = a_tl = Mlist1(formals_to_arity(Mcaar(clauses)));
        for (clauses = Mcdr(clauses); !Mnullp(clauses); clauses = Mcdr(clauses)) {
            Mcdr(b_tl) = Mlist1(Mcadar(clauses));
            Mcdr(f_tl) = Mlist1(Mcaar(clauses));
            Mcdr(a_tl) = Mlist1(formals_to_arity(Mcaar(clauses)));
            b_tl = Mcdr(b_tl);
            f_tl = Mcdr(f_tl);
            a_tl = Mcdr(a_tl);
        }

    }

    return x;
}

obj Minput_file_port(FILE *f) {
    obj x = GC_malloc(Mport_size);
    obj_type(x) = PORT_OBJ_TYPE;
    Mport_flags(x) = PORT_FLAG_READ;
    Mport_file(x) = f;
    Mport_count(x) = fseek(f, 0, SEEK_CUR);
    return x;
}

obj Moutput_file_port(FILE *f) {
    obj x = GC_malloc(Mport_size);
    obj_type(x) = PORT_OBJ_TYPE;
    Mport_flags(x) = 0x0;
    Mport_file(x) = f;
    Mport_count(x) = fseek(f, 0, SEEK_CUR);
    return x;
}

obj Minput_string_port(obj s) {
    obj x = GC_malloc(Mport_size);
    obj_type(x) = PORT_OBJ_TYPE;
    Mport_flags(x) = PORT_FLAG_READ | PORT_FLAG_STR;
    Mport_buffer(x) = s;
    Mport_count(x) = 0;
    return x;
}

obj Mnull_continuation(obj env) {
    obj x = GC_malloc(Mcontinuation_null_size);
    obj_type(x) = CONTINUATON_OBJ_TYPE;
    Mcontinuation_type(x) = NULL_CONT_TYPE;
    Mcontinuation_immutablep(x) = 0;
    Mcontinuation_capturedp(x) = 0;
    Mcontinuation_prev(x) = Mnull;
    Mcontinuation_env(x) = env;
    return x;
}

obj Mapp_continuation(obj prev, obj env, obj args, uptr idx) {
    obj x = GC_malloc(Mcontinuation_app_size);
    obj_type(x) = CONTINUATON_OBJ_TYPE;
    Mcontinuation_type(x) = APP_CONT_TYPE;
    Mcontinuation_immutablep(x) = 0;
    Mcontinuation_capturedp(x) = 0;
    Mcontinuation_prev(x) = prev;
    Mcontinuation_env(x) = env;
    Mcontinuation_app_it(x) = args;
    Mcontinuation_app_idx(x) = idx;
    return x;
}

obj Mcond_continuation(obj prev, obj env, obj ift, obj iff) {
    obj x = GC_malloc(Mcontinuation_cond_size);
    obj_type(x) = CONTINUATON_OBJ_TYPE;
    Mcontinuation_type(x) = COND_CONT_TYPE;
    Mcontinuation_immutablep(x) = 0;
    Mcontinuation_capturedp(x) = 0;
    Mcontinuation_prev(x) = prev;
    Mcontinuation_env(x) = env;
    Mcontinuation_cond_ift(x) = ift;
    Mcontinuation_cond_iff(x) = iff;
    return x;
}

obj Mseq_continuation(obj prev, obj env, obj seq) {
    obj x = GC_malloc(Mcontinuation_seq_size);
    obj_type(x) = CONTINUATON_OBJ_TYPE;
    Mcontinuation_type(x) = SEQ_CONT_TYPE;
    Mcontinuation_immutablep(x) = 0;
    Mcontinuation_capturedp(x) = 0;
    Mcontinuation_prev(x) = prev;
    Mcontinuation_env(x) = env;
    Mcontinuation_seq_value(x) = seq;
    return x;
}

obj Mlet_continuation(obj prev, obj env, obj bindings, obj body) {
    obj x = GC_malloc(Mcontinuation_let_size);
    obj_type(x) = CONTINUATON_OBJ_TYPE;
    Mcontinuation_type(x) = LET_CONT_TYPE;
    Mcontinuation_immutablep(x) = 0;
    Mcontinuation_capturedp(x) = 0;
    Mcontinuation_prev(x) = prev;
    Mcontinuation_env(x) = env;
    Mcontinuation_let_env(x) = env_extend(env);
    Mcontinuation_let_bindings(x) = bindings;
    Mcontinuation_let_body(x) = body;
    return x;   
}

obj Msetb_continuation(obj prev, obj env, obj name) {
    obj x = GC_malloc(Mcontinuation_setb_size);
    obj_type(x) = CONTINUATON_OBJ_TYPE;
    Mcontinuation_type(x) = SETB_CONT_TYPE;
    Mcontinuation_immutablep(x) = 0;
    Mcontinuation_capturedp(x) = 0;
    Mcontinuation_prev(x) = prev;
    Mcontinuation_env(x) = env;
    Mcontinuation_setb_name(x) = name;
    return x;
}

obj Mcallcc_continuation(obj prev, obj env, obj winders, obj *ab, uptr aa, uptr ac) {
    obj x;
    
    x = GC_malloc(Mcontinuation_callcc_size);
    obj_type(x) = CONTINUATON_OBJ_TYPE;
    Mcontinuation_type(x) = CALLCC_CONT_TYPE;
    Mcontinuation_immutablep(x) = 0;
    Mcontinuation_capturedp(x) = 0;
    Mcontinuation_prev(x) = prev;
    Mcontinuation_env(x) = env;
    Mcontinuation_callcc_winders(x) = winders;

    // copy argument buffer (and metadata)
    Mcontinuation_callcc_aa(x) = aa;
    Mcontinuation_callcc_ac(x) = ac;
    Mcontinuation_callcc_ab(x) = GC_malloc(Mcontinuation_callcc_aa(x) * sizeof(obj));
    memcpy(Mcontinuation_callcc_ab(x), ab, Mcontinuation_callcc_ac(x) * sizeof(obj));

    return x;
}

obj Mcallwv_continuation(obj prev, obj env, obj producer, obj consumer) {
    obj x = GC_malloc(Mcontinuation_callwv_size);
    obj_type(x) = CONTINUATON_OBJ_TYPE;
    Mcontinuation_type(x) = CALLWV_CONT_TYPE;
    Mcontinuation_immutablep(x) = 0;
    Mcontinuation_capturedp(x) = 0;
    Mcontinuation_prev(x) = prev;
    Mcontinuation_env(x) = env;
    Mcontinuation_callwv_producer(x) = producer;
    Mcontinuation_callwv_consumer(x) = consumer;
    return x;
}

obj Mdynwind_continuation(obj prev, obj env, obj pre, obj val, obj post) {
    obj x = GC_malloc(Mcontinuation_dynwind_size);
    obj_type(x) = CONTINUATON_OBJ_TYPE;
    Mcontinuation_type(x) = DYNWIND_CONT_TYPE;
    Mcontinuation_immutablep(x) = 0;
    Mcontinuation_capturedp(x) = 0;
    Mcontinuation_prev(x) = prev;
    Mcontinuation_env(x) = env;
    Mcontinuation_dynwind_pre(x) = pre;
    Mcontinuation_dynwind_val(x) = val;
    Mcontinuation_dynwind_post(x) = post;
    Mcontinuation_dynwind_state(x) = DYNWIND_NEW;
    return x;
}

obj Mwinders_continuation(obj prev, obj env, obj winders) {
    obj x = GC_malloc(Mcontinuation_winders_size);
    obj_type(x) = CONTINUATON_OBJ_TYPE;
    Mcontinuation_type(x) = WINDERS_CONT_TYPE;
    Mcontinuation_immutablep(x) = 0;
    Mcontinuation_capturedp(x) = 0;
    Mcontinuation_prev(x) = prev;
    Mcontinuation_env(x) = env;
    Mcontinuation_winders_it(x) = winders;
    Mcontinuation_winders_values(x) = Mfalse;
    return x;
}

obj Mthread_context(void) {
    obj x = GC_malloc(Mtc_size);
    obj_type(x) = THREAD_OBJ_TYPE;
    Mtc_cc(x) = Mnull;
    Mtc_wnd(x) = Mnull;
    Mtc_env(x) = empty_env();
    Mtc_vb(x) = GC_malloc(INIT_VALUES_BUFFER_LEN * sizeof(obj));
    Mtc_va(x) = INIT_VALUES_BUFFER_LEN;
    Mtc_vc(x) = 0;
    Mtc_ab(x) = GC_malloc(INIT_ARGS_BUFFER_LEN * sizeof(obj));
    Mtc_aa(x) = INIT_ARGS_BUFFER_LEN;
    Mtc_ac(x) = 0;
    Mtc_ip(x) = Minput_file_port(stdin);
    Mtc_op(x) = Moutput_file_port(stdout);
    Mtc_ep(x) = Moutput_file_port(stderr);
    return x;
}

obj Mmodule(obj path, obj subpath, obj body) {
    obj x = GC_malloc(Mmodule_size);
    obj_type(x) = MODULE_OBJ_TYPE;
    Mmodule_livep(x) = 0;
    Mmodule_path(x) = path;
    Mmodule_subpath(x) = subpath;
    Mmodule_body(x) = body;
    Mmodule_env(x) = Mfalse;
    return x;
}
