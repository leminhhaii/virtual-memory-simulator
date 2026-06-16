#include "trace.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int trace_open(TraceFile *trace, const char *path) {
    if (!trace || !path) {
        return -1;
    }
    trace->file = fopen(path, "r");
    if (!trace->file) {
        return -1;
    }
    trace->current_line = 0;
    return 0;
}

int trace_read_next(TraceFile *trace, char *op, uint32_t *addr, int *line_num) {
    if (!trace || !trace->file || !op || !addr || !line_num) {
        return -1;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), trace->file)) {
        trace->current_line++;
        
        // Truncate trailing newlines/returns
        line[strcspn(line, "\r\n")] = '\0';
        
        // Strip out comments
        char *comment_ptr = strchr(line, '#');
        if (comment_ptr) {
            *comment_ptr = '\0';
        }
        
        // Trim leading spaces
        char *ptr = line;
        while (*ptr && isspace((unsigned char)*ptr)) {
            ptr++;
        }
        
        // If line is empty, skip to next line
        if (*ptr == '\0') {
            continue;
        }
        
        // Extract parameters: Op (char), Hex Address (uint32), and search for trailing junk
        char parsed_op;
        unsigned int parsed_addr;
        char extra[128];
        int parsed = sscanf(ptr, " %c %x %127s", &parsed_op, &parsed_addr, extra);
        
        *line_num = trace->current_line;
        
        // Validation: Expect exactly 2 matching items (Op and Addr).
        // If parsed is 3, trailing junk was found. If < 2, the format is invalid.
        if (parsed != 2) {
            return -1; // Syntax error
        }
        
        // Operation validation
        if (parsed_op == 'r' || parsed_op == 'R') {
            *op = 'R';
        } else if (parsed_op == 'w' || parsed_op == 'W') {
            *op = 'W';
        } else {
            return -1; // Invalid operation code
        }
        
        *addr = (uint32_t)parsed_addr;
        return 1; // Successfully parsed next operation
    }
    
    if (ferror(trace->file)) {
        return -1; // I/O read failure
    }
    
    return 0; // EOF reached
}

void trace_close(TraceFile *trace) {
    if (trace && trace->file) {
        fclose(trace->file);
        trace->file = NULL;
    }
}
