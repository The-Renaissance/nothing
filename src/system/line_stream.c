#include "system/stacktrace.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include "line_stream.h"
#include "lt.h"
#include "lt_adapters.h"
#include "system/nth_alloc.h"
#include "system/log.h"
#include "system/str.h"

struct LineStream
{
    Lt *lt;
    FILE *stream;
    char *buffer;
    size_t capacity;
    bool unfinished;
};

// TODO(#905): create_line_stream probably does not need mode
//   Because LineStream interface doesn't even have anything
//   for writing to files. So we can just hardcode the mode
//   inside of the ctor.
LineStream *create_line_stream(const char *filename,
                               const char *mode,
                               size_t capacity)
{
    trace_assert(filename);
    trace_assert(mode);

    Lt *lt = create_lt();

    LineStream *line_stream = PUSH_LT(
        lt,
        nth_calloc(1, sizeof(LineStream)),
        free);
    if (line_stream == NULL) {
        RETURN_LT(lt, NULL);
    }
    line_stream->lt = lt;

    line_stream->stream = PUSH_LT(
        lt,
        fopen(filename, mode),
        fclose_lt);
    if (line_stream->stream == NULL) {
        log_fail("Could not open file '%s': %s\n", filename, strerror(errno));
        RETURN_LT(lt, NULL);
    }

    line_stream->buffer = PUSH_LT(
        lt,
        nth_calloc(1, sizeof(char) * capacity),
        free);
    if (line_stream->buffer == NULL) {
        RETURN_LT(lt, NULL);
    }

    line_stream->capacity = capacity;
    line_stream->unfinished = false;

    return line_stream;
}

void destroy_line_stream(LineStream *line_stream)
{
    trace_assert(line_stream);

    RETURN_LT0(line_stream->lt);
}


const char *line_stream_next_chunk(LineStream *line_stream)
{
    trace_assert(line_stream);

    const char *s = fgets(line_stream->buffer,
                          (int) line_stream->capacity,
                          line_stream->stream);
    if (s == NULL) {
        return NULL;
    }
    size_t n = strlen(s);
    line_stream->unfinished = s[n - 1] != '\n';

    return s;
}

const char *line_stream_next(LineStream *line_stream)
{
    trace_assert(line_stream);

    while (line_stream->unfinished) {
        line_stream_next_chunk(line_stream);
    }

    return line_stream_next_chunk(line_stream);
}

char *line_stream_collect_n_lines(LineStream *line_stream, size_t n)
{
    char *result = string_append(NULL, "");
    for (size_t i = 0; i < n; ++i) {
        const char *line = line_stream_next(line_stream);
        if (line == NULL) {
            free(result);
            return NULL;
        }

        result = string_append(result,line);
    }

    return result;
}

char *line_stream_collect_until_end(LineStream *line_stream)
{
    char *result = string_append(NULL, "");
    const char *line = line_stream_next(line_stream);

    /* TODO(#906): line_stream_collect_until_end does not distinguish between EOF and error during reading */
    while (line != NULL) {
        result = string_append(result, line);
        line = line_stream_next(line_stream);
    }

    return result;
}
