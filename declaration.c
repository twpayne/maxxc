#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <expat.h>

#include "maxxc.h"

typedef struct {
    int state;
    string_buffer_t *radius;
    declaration_t *declaration;
} state_t;

    static void
declaration_push_turnpoint(declaration_t *declaration, const turnpoint_t *turnpoint)
{
    if (declaration->nturnpoints == declaration->turnpoints_capacity) {
        declaration->turnpoints_capacity = declaration->turnpoints_capacity ? 2 * declaration->turnpoints_capacity : 4;
        declaration->turnpoints = realloc(declaration->turnpoints, declaration->turnpoints_capacity * sizeof(turnpoint_t));
        if (!declaration->turnpoints)
            DIE("realloc", errno);
    }
    declaration->turnpoints[declaration->nturnpoints] = *turnpoint;
    ++declaration->nturnpoints;
}

    static void XMLCALL
declaration_start_element_handler(void *userData, const XML_Char *name, const XML_Char **atts)
{
    state_t *state = userData;

    if (state->state == 0) {
        if (!strcmp(name, "rtept")) {
            turnpoint_t turnpoint;
            turnpoint.coord.sin_lat = 0.0;
            turnpoint.coord.cos_lat = 0.0;
            turnpoint.coord.lon = 0.0;
            turnpoint.radius = 400.0;
            for (int i = 0; atts[i]; i += 2) {
                if (!strcmp(atts[i], "lat")) {
                    char *endptr = 0;
                    errno = 0;
                    double deg_lat = strtod(atts[i + 1], &endptr);
                    if (*endptr || errno)
                        DIE("strtod", errno ? errno : EINVAL);
                    double lat = M_PI * deg_lat / 180.0;
                    turnpoint.coord.sin_lat = sin(lat);
                    turnpoint.coord.cos_lat = cos(lat);
                } else if (!strcmp(atts[i], "lon")) {
                    char *endptr = 0;
                    errno = 0;
                    double deg_lon = strtod(atts[i + 1], &endptr);
                    if (*endptr || errno)
                        DIE("strtod", errno ? errno : EINVAL);
                    turnpoint.coord.lon = M_PI * deg_lon / 180.0;
                }
            }
            declaration_push_turnpoint(state->declaration, &turnpoint);
            ++state->state;
        }
    } else if (state->state == 1) {
        if (!strcmp(name, "extensions"))
            ++state->state;
    } else if (state->state == 2) {
        if (!strcmp(name, "radius")) {
            string_buffer_reset(state->radius);
            ++state->state;
        }
    }
}

    static void XMLCALL
declaration_character_data_handler(void *userData, const XML_Char *s, int len)
{
    state_t *state = userData;

    if (state->state == 3)
        string_buffer_append(state->radius, s, len);
}

    static void XMLCALL
declaration_end_element_handler(void *userData, const XML_Char *name)
{
    state_t *state = userData;

    switch (state->state) {
        case 1:
            if (!strcmp(name, "rtept"))
                --state->state;
            break;
        case 2:
            if (!strcmp(name, "extensions"))
                --state->state;
            break;
        case 3:
            if (!strcmp(name, "radius")) {
                char *endptr = 0;
                errno = 0;
                double radius = strtod(string_buffer_string(state->radius), &endptr);
                while (isspace(*endptr))
                    ++endptr;
                if (*endptr || errno)
                    DIE("strtod", errno ? errno : EINVAL);
                state->declaration->turnpoints[state->declaration->nturnpoints - 1].radius = radius;
                --state->state;
            }
            break;
    }
}

    declaration_t *
declaration_new_from_file(FILE *file)
{
    state_t state;
    state.state = 0;
    state.radius = string_buffer_new();
    state.declaration = alloc(sizeof(declaration_t));

    XML_Parser p = XML_ParserCreate("UTF-8");
    XML_SetUserData(p, &state);
    XML_SetStartElementHandler(p, declaration_start_element_handler);
    XML_SetCharacterDataHandler(p, declaration_character_data_handler);
    XML_SetEndElementHandler(p, declaration_end_element_handler);
    while (1) {
        void *buffer = XML_GetBuffer(p, 4096);
        if (!buffer)
            DIE("XML_GetBuffer", errno);
        size_t size = fread(buffer, 1, 4096, file);
        if (size == 0 && ferror(file))
            DIE("fread", errno);
        if (!XML_ParseBuffer(p, size, size == 0))
            DIE("XML_ParseBuffer", errno);
        if (!size)
            break;
    }
    XML_ParserFree(p);

    string_buffer_free(state.radius);

    return state.declaration;
}

    void
declaration_free(declaration_t *declaration)
{
    if (declaration) {
        free(declaration->turnpoints);
        free(declaration);
    }
}
