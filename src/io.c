#include "io.h"

static IOFunctionEntry io_functions[] = {
    { .name = "io:read", .function = io_read },
    { .name = "io:write", .function = io_write },
    { .name = "io:print", .function = io_print },
    { .name = "io:scan", .function = io_scan }
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
        return lime_format_exception(stack, "failed to open file '%s' in function '%s'", buffer, __FUNCTION__).value;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return lime_format_exception(stack, "failed to read file '%s' in function '%s'", buffer, __FUNCTION__).value;
    }

    const u64 bytes = (u64) ftell(file);

    if (bytes == (u64) -1L) {
        fclose(file);
        return lime_format_exception(stack, "failed to read file '%s' in function '%s'", buffer, __FUNCTION__).value;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return lime_format_exception(stack, "failed to read file '%s' in function '%s'", buffer, __FUNCTION__).value;
    }

    LIME_TRY(&string, lime_allocate(stack, LimeStringValue, bytes));
    string->string.length = bytes;
    if (fread(string->string.bytes, sizeof(u8), bytes, file) != bytes) {
        fclose(file);
        return lime_format_exception(stack, "failed to read file '%s' in function '%s'", buffer, __FUNCTION__).value;
    }

    fclose(file);
    LIME_STACK_PUSH_VALUE(stack, string);
    return NULL;
}

LimeValue io_write(LimeStack stack) {
    LimeValue path;
    LimeValue string;
    char buffer[4096];

    LIME_STACK_POP_VALUE(stack, &path);
    LIME_ASSERT_TYPE(path, LimeStringValue);

    LIME_STACK_POP_VALUE(stack, &string);
    LIME_ASSERT_TYPE(string, LimeStringValue);

    memcpy(buffer, path->string.bytes, path->string.length);
    buffer[path->string.length] = '\0';

    FILE *file = fopen(&buffer[0], "w+");

    if (file == NULL) {
        return lime_format_exception(stack, "failed to write file '%s' in function '%s'", buffer, __FUNCTION__).value;
    }

    const u64 length = string->string.length;
    if (fwrite(&string->string.bytes[0], sizeof(string->string.bytes[0]), length, file) != length) {
        fclose(file);
        return lime_format_exception(stack, "failed to write file '%s' in function '%s'", buffer, __FUNCTION__).value;
    }

    fclose(file);

    return NULL;
}

LimeValue io_print(LimeStack stack) {
    LimeValue value;

    LIME_STACK_POP_VALUE(stack, &value);
    LIME_ASSERT_TYPE(value, LimeStringValue);

    const u64 length = value->string.length;
    if (fwrite(&value->string.bytes[0], sizeof(value->string.bytes[0]), length, stdout) != length) {
        return lime_format_exception(stack, "io exception in function '%s'", __FUNCTION__).value;
    }

    fflush(stdout);

    return NULL;
}

LimeValue io_scan(LimeStack stack) {
    LimeStackFrame frame = LIME_ALLOCATE_STACK_FRAME(stack, NULL, NULL, NULL);
    u8 buffer[4096];
    register u64 length;
    register bool stop = false;

    do {
        char *const result = fgets((char *) &buffer[0], sizeof(buffer) / sizeof(buffer[0]), stdin);

        if (result == NULL) {
            if (feof(stdin)) {
                buffer[0] = '\0';
                stop = true;
                length = 0;
            } else {
                return lime_format_exception(&frame, "io exception in function '%s'", __FUNCTION__).value;
            }
        } else {
            length = strlen((char *) &buffer[0]);
            if (feof(stdin)) {
                stop = true;
            } else if (buffer[length - 1] == '\n') {
                // remove the new line character
                buffer[--length] = '\0';
                stop = true;
            }
        }

        if (frame.registers[0] == NULL) {
            // create the initial string (in most cases this is most probably sufficient)
            LIME_TRY(&frame.registers[0], lime_string(&frame, &buffer[0], length));
        } else {
            // create a new string that is large enough to contain the old one and the buffer
            const u64 old_length = frame.registers[0]->string.length;
            LIME_TRY(&frame.registers[1], lime_allocate(&frame, LimeStringValue, (length + old_length) * sizeof(u8)));
            memcpy(&frame.registers[1]->string.bytes[0], &frame.registers[0]->string.bytes[0], old_length * sizeof(u8));
            memcpy(&frame.registers[1]->string.bytes[old_length], &buffer[0], length * sizeof(u8));
            frame.registers[1]->string.length = length + old_length;
            frame.registers[0] = frame.registers[1];
            frame.registers[1] = NULL;
        }
    } while (!stop);

    LIME_STACK_PUSH_VALUE(&frame, frame.registers[0]);

    return NULL;
}
