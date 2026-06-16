#ifndef TRACE_H
#define TRACE_H

#include <stdio.h>
#include <stdint.h>

typedef struct {
    FILE *file;
    int current_line;
} TraceFile;

int trace_open(TraceFile *trace, const char *path);
int trace_read_next(TraceFile *trace, char *op, uint32_t *addr, int *line_num);
void trace_close(TraceFile *trace);

#endif // TRACE_H
