/*
    Records
*/

#include "../minim.h"

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
    |          Opaque?         |
    +--------------------------+
    |          Sealed?         |
    +--------------------------+
    |  Protocol (constructor)  |
    +--------------------------+
    |          Accessor        |
    +--------------------------+
    |          Mutator         |
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

    Field descriptors have the following possible forms:
    
    '(immutable <name> <accessor-name>)
    '(mutable <name> <accessor-name> <mutator-name>)
    '(immutable <name>)
    '(mutable <name>)
    <name>

    The third and forth forms are just shorthand for
    
    '(immutable <name> <rtd-name>-<name>)
    '(mutable <name> <rtd-name>-<name> <rtd-name>-<name>-set!)

    respectively. The fifth form is just an abbreviation for

    '(immutable <name>)

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

minim_object *make_record(minim_object *rtd, int fieldc) {
    minim_record_object *o = GC_alloc(sizeof(minim_record_object));
    o->type = MINIM_RECORD_TYPE;
    o->rtd = rtd;
    o->fields = GC_calloc(fieldc, sizeof(minim_object*));
    o->fieldc = fieldc;
    return ((minim_object *) o);
}
