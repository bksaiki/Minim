// env.c: environments

#include "minim.h"

obj empty_env(void) {
    return Mcons(Mnull, Mnull); 
}

obj env_extend(obj env) {
    return Mcons(Mnull, env);
}

obj env_find(obj env, obj k) {
    for (; !Mnullp(env); env = Mcdr(env)) {
        for (obj r = Mcar(env); !Mnullp(r); r = Mcdr(r)) {
            if (Mcaar(r) == k)
                return Mcar(r);
        }
    }

    return Mfalse;
}

void env_insert(obj env, obj k, obj v) {
    Mcar(env) = Mcons(Mcons(k, v), Mcar(env));
}

void env_import(obj dst, obj src) {
    return env_import_prefix(dst, src, Mfalse);
}

static obj symbol_append(obj s1, obj s2) {
    char *s;
    uptr n;
    
    n = snprintf(NULL, 0, "%s%s", Msymbol_value(s1), Msymbol_value(s2));
    s = GC_malloc_atomic((n + 1) * sizeof(char));
    snprintf(s, n + 1, "%s%s", Msymbol_value(s1), Msymbol_value(s2));

    return Mintern(s);
}

void env_import_prefix(obj dst, obj src, obj prefix) {
    obj r, key, cell;

    for (; !Mnullp(src); src = Mcdr(src)) {
        for (r = Mcar(src); !Mnullp(r); r = Mcdr(r)) {
            if (Mfalsep(prefix)) {
                key = Mcaar(r);
            } else {
                key = symbol_append(prefix, Mcaar(r));
            }

            cell = env_find(dst, key);
            if (Mfalsep(cell)) {
                // no current binding => insert new one
                env_insert(dst, key, Mcdar(r));
            } else {
                // current binding => overwrite it
                Mcdr(cell) = Mcdar(r);
            }
        }
    }
}
