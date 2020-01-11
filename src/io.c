#include "io.h"

static IOFunctionEntry io_functions[] = {
    { .name = "io:read", .function = io_read },
    { .name = "io:write", .function = io_write }
};

LimeValue lime_module_initialize(LimeStack stack, LimeValue library) {
    LimeStackFrame frame = LIME_ALLOCATE_STACK_FRAME(stack, library, NULL, NULL);

    for (u64 i = 0; i < sizeof(io_functions) / sizeof(io_functions[0]); ++i) {
        IOFunctionEntry *const entry = &io_functions[i];
        const LimeValue exception = lime_register_function(&frame, entry->name, frame.registers[0], entry->function);
        if (exception != NULL) {
            return exception;
        }
    }

    return NULL;    
}

LimeValue lime_module_finalize(LimeStack stack, LimeValue library) {
    return NULL;
}

LimeValue io_read(LimeStack stack) {
    LimeValue string;
    char buffer[4096];

    LIME_STACK_POP_VALUE(stack, &string);
    LIME_ASSERT_TYPE(string, LimeStringValue);

    memcpy(buffer, string->string.bytes, string->string.length);
    buffer[string->string.length] = '\0';

    FILE *file = fopen(buffer, "r");

    if (file == NULL) {
        return lime_exception(stack, "failed to open file '%s' in function '%s'", buffer, __FUNCTION__);
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return lime_exception(stack, "failed to read file '%s' in function '%s'", buffer, __FUNCTION__);
    }

    const u64 bytes = (u64) ftell(file);

    if (bytes == (u64) -1L) {
        fclose(file);
        return lime_exception(stack, "failed to read file '%s' in function '%s'", buffer, __FUNCTION__);
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return lime_exception(stack, "failed to read file '%s' in function '%s'", buffer, __FUNCTION__);
    }

    LIME_TRY(&string, lime_allocate(stack, LimeStringValue, bytes));
    string->string.length = bytes;
    if (fread(string->string.bytes, sizeof(u8), bytes, file) != bytes) {
        fclose(file);
        return lime_exception(stack, "failed to read file '%s' in function '%s'", buffer, __FUNCTION__);
    }

    fclose(file);
    LIME_STACK_PUSH_VALUE(stack, string);
    return NULL;
}

LimeValue io_write(LimeStack stack) {
    return lime_exception(stack, "'%s' is not yet implemented", __FUNCTION__);
}
