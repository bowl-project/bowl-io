#include "io.h"

static BowlFunctionEntry io_functions[] = {
    { .name = "io:read", .documentation = "", .function = io_read },
    { .name = "io:write", .documentation = "", .function = io_write },
    { .name = "io:print", .documentation = "", .function = io_print },
    { .name = "io:scan", .documentation = "", .function = io_scan }
};

BowlValue bowl_module_initialize(BowlStack stack, BowlValue library) {
    BowlStackFrame frame = BOWL_ALLOCATE_STACK_FRAME(stack, library, NULL, NULL);
    return bowl_register_all(&frame, frame.registers[0], io_functions, sizeof(io_functions) / sizeof(io_functions[0]));
}

BowlValue bowl_module_finalize(BowlStack stack, BowlValue library) {
    return NULL;
}

BowlValue io_read(BowlStack stack) {
    BowlValue string;

    BOWL_STACK_POP_VALUE(stack, &string);
    BOWL_ASSERT_TYPE(string, BowlStringValue);

    char *path = unicode_to_string(&string->string.codepoints[0], string->string.length);
    if (path == NULL) {
        return bowl_exception_out_of_heap;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        BowlValue result = bowl_format_exception(stack, "failed to open file '%s' in function '%s'", path, __FUNCTION__).value;
        free(path);
        return result;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        BowlValue result = bowl_format_exception(stack, "failed to read file '%s' in function '%s'", path, __FUNCTION__).value;
        free(path);
        return result;
    }

    const u64 bytes = (u64) ftell(file);

    if (bytes == (u64) -1L) {
        fclose(file);
        BowlValue result = bowl_format_exception(stack, "failed to read file '%s' in function '%s'", path, __FUNCTION__).value;
        free(path);
        return result;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        BowlValue result = bowl_format_exception(stack, "failed to read file '%s' in function '%s'", path, __FUNCTION__).value;
        free(path);
        return result;
    }

    u8 *const content = malloc(bytes * sizeof(u8));
    if (content == NULL) {
        free(path);
        fclose(file);
        return bowl_exception_out_of_heap;
    }

    if (fread(&content[0], sizeof(u8), bytes, file) != bytes) {
        fclose(file);
        free(content);
        BowlValue result = bowl_format_exception(stack, "failed to read file '%s' in function '%s'", path, __FUNCTION__).value;
        free(path);
        return result;
    }
    
    fclose(file);
    free(path);
    
    BowlResult result = bowl_string_utf8(stack, content, bytes);
    free(content);
    
    if (result.failure) {
        return result.exception;
    }

    BOWL_STACK_PUSH_VALUE(stack, result.value);
    return NULL;
}

BowlValue io_write(BowlStack stack) {
    BowlValue path;
    BowlValue string;

    BOWL_STACK_POP_VALUE(stack, &path);
    BOWL_ASSERT_TYPE(path, BowlStringValue);

    BOWL_STACK_POP_VALUE(stack, &string);
    BOWL_ASSERT_TYPE(string, BowlStringValue);

    char *cpath = unicode_to_string(&path->string.codepoints[0], path->string.length);
    if (cpath == NULL) {
        return bowl_exception_out_of_heap;
    }

    FILE *file = fopen(cpath, "wb+");

    if (file == NULL) {
        BowlValue result = bowl_format_exception(stack, "failed to write file '%s' in function '%s'", cpath, __FUNCTION__).value;
        free(cpath);
        return result;
    }

    char *bytes = unicode_to_string(&string->string.codepoints[0], string->string.length);
    if (bytes == NULL) {
        fclose(file);
        free(cpath);
        return bowl_exception_out_of_heap;
    }

    const u64 size = strlen(bytes);
    if (fwrite(bytes, sizeof(char), size, file) != size) {
        fclose(file);
        BowlValue result = bowl_format_exception(stack, "failed to write file '%s' in function '%s'", cpath, __FUNCTION__).value;
        free(bytes);
        free(cpath);
        return result;
    }

    free(cpath);
    free(bytes);
    fflush(file);
    fclose(file);

    return NULL;
}

BowlValue io_print(BowlStack stack) {
    BowlValue value;

    BOWL_STACK_POP_VALUE(stack, &value);
    BOWL_ASSERT_TYPE(value, BowlStringValue);

    char *string = unicode_to_string(&value->string.codepoints[0], value->string.length);
    if (string == NULL) {
        return bowl_exception_out_of_heap;
    }

    const u64 size = strlen(string);
    if (fwrite(string, sizeof(char), size, stdout) != size) {
        free(string);
        return bowl_format_exception(stack, "io exception in function '%s'", __FUNCTION__).value;
    }

    fflush(stdout);

    return NULL;
}

BowlValue io_scan(BowlStack stack) {
    BowlStackFrame frame = BOWL_ALLOCATE_STACK_FRAME(stack, NULL, NULL, NULL);
    u64 capacity = 1024;

    BOWL_TRY(&frame.registers[0], bowl_allocate(&frame, BowlStringValue, capacity * sizeof(u32)));
    frame.registers[0]->string.length = 0;

    do {
        // is there enough space to store the next codepoint?
        if (frame.registers[0]->string.length >= capacity) {
            capacity *= 2;
            BOWL_TRY(&frame.registers[1], bowl_allocate(&frame, BowlStringValue, capacity * sizeof(u32)));
            memcpy(&(frame.registers[1]->string.codepoints[0]), &(frame.registers[0]->string.codepoints[0]), frame.registers[0]->string.length * sizeof(u32));
            frame.registers[1]->string.length = frame.registers[0]->string.length;
            frame.registers[0] = frame.registers[1];
            frame.registers[1] = NULL;
        }

        // read the next codepoint from stdin
        u32 state = UNICODE_UTF8_STATE_ACCEPT;
        u32 codepoint;

        do {
            u8 byte;

            if (fread(&byte, sizeof(u8), 1, stdin) != 1) {
                if (!feof(stdin)) {
                    return bowl_format_exception(&frame, "io exception in function '%s' (failed to read)", __FUNCTION__).value;
                } else if (state != UNICODE_UTF8_STATE_ACCEPT) {
                    return bowl_format_exception(&frame, "io exception in function '%s' (incomplete UTF-8 sequence)", __FUNCTION__).value;
                } else {
                    BOWL_STACK_PUSH_VALUE(&frame, frame.registers[0]);
                    return NULL;
                }
            }
            
            if (unicode_utf8_decode(&state, &codepoint, byte) == UNICODE_UTF8_STATE_ACCEPT) {
                if (codepoint == '\n') {
                    // do not write the \n to the buffer and quit scanning
                    BOWL_STACK_PUSH_VALUE(&frame, frame.registers[0]);
                    return NULL;
                } else {
                    frame.registers[0]->string.codepoints[frame.registers[0]->string.length++] = codepoint;
                }
            } else if (state == UNICODE_UTF8_STATE_REJECT) {
                frame.registers[0]->string.codepoints[frame.registers[0]->string.length++] = UNICODE_REPLACEMENT_CHARACTER;
            }
        } while (state != UNICODE_UTF8_STATE_ACCEPT && state != UNICODE_UTF8_STATE_REJECT);
    } while (true);

    return bowl_format_exception(&frame, "io exception in function '%s' (failed to read)", __FUNCTION__).value;
}
