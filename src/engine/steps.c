#include "steps.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "arena.h"

void steps_init(Steps *s) { memset(s, 0, sizeof *s); }

void steps_clear(Steps *s) { s->n = 0; s->depth = 0; }

void steps_push(Steps *s) { if (s) s->depth++; }
void steps_pop(Steps *s) { if (s && s->depth > 0) s->depth--; }

void steps_add(Steps *s, const char *title, const char *note, const char *math_fmt, ...)
{
    if (!s) return;
    if (s->n == s->cap) {
        int nc = s->cap ? s->cap * 2 : 16;
        Step *nv = amalloc(sizeof(Step) * (size_t)nc);
        if (s->n) memcpy(nv, s->v, sizeof(Step) * (size_t)s->n);
        s->v = nv;
        s->cap = nc;
    }
    Step *st = &s->v[s->n++];
    st->title = astrdup(title ? title : "");
    st->note = astrdup(note ? note : "");
    st->depth = s->depth;
    if (math_fmt && *math_fmt) {
        char small[512];
        va_list ap, ap2;
        va_start(ap, math_fmt);
        va_copy(ap2, ap);
        int len = vsnprintf(small, sizeof small, math_fmt, ap);
        va_end(ap);
        if (len < 0) {
            st->math = astrdup("");
        } else if ((size_t)len < sizeof small) {
            st->math = astrndup(small, (size_t)len);
        } else {
            char *m = amalloc((size_t)len + 1);
            vsnprintf(m, (size_t)len + 1, math_fmt, ap2);
            st->math = m;
        }
        va_end(ap2);
    } else {
        st->math = astrdup("");
    }
}
