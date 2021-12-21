#ifndef IO_H
#define IO_H

#include <bowl/api.h>

typedef struct {
    char *name;
    BowlFunction function;
} IOFunctionEntry;

BowlValue io_read(BowlStack stack);

BowlValue io_write(BowlStack stack);

BowlValue io_print(BowlStack stack);

BowlValue io_scan(BowlStack stack);

#endif
