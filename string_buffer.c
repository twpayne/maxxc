#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "maxxc.h"

    string_buffer_t *
string_buffer_new(void)
{
    string_buffer_t *string_buffer = alloc(sizeof(string_buffer));
    string_buffer->capacity = 16;
    string_buffer->string = alloc(string_buffer->capacity);
    string_buffer->string[0] = 0;
    string_buffer->length = 0;
    return string_buffer;
}

    void
string_buffer_free(string_buffer_t *string_buffer)
{
    if (string_buffer) {
	free(string_buffer->string);
	free(string_buffer);
    }
}

    void
string_buffer_append(string_buffer_t *string_buffer, const char *s, int len)
{
    if (string_buffer->length + len + 1 > string_buffer->capacity) {
	string_buffer->capacity = 2 * string_buffer->capacity;
	string_buffer->string = realloc(string_buffer->string, string_buffer->capacity);
	if (!string_buffer->string)
	    DIE("realloc", errno);
    }
    memcpy(string_buffer->string + string_buffer->length, s, len);
    string_buffer->length += len;
    string_buffer->string[string_buffer->length] = 0;
}

    const char *
string_buffer_string(const string_buffer_t *string_buffer)
{
    return string_buffer->string;
}

    void
string_buffer_reset(string_buffer_t *string_buffer)
{
    string_buffer->length = 0;
    string_buffer->string[0] = 0;
}
