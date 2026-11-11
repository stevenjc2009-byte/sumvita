#include "arena.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARENA_DEFAULT_BLOCK (256u * 1024u)

typedef struct Block {
    struct Block *next;
    size_t size;
    size_t used;
    unsigned char data[];
} Block;

struct Arena {
    Block *head;        /* newest block first */
    size_t block_size;
    size_t total_used;
};

static Arena *g_current;

static Block *block_new(size_t size)
{
    Block *b = malloc(sizeof(Block) + size);
    if (!b) {
        fprintf(stderr, "arena: out of memory (%zu bytes)\n", size);
        abort();
    }
    b->next = NULL;
    b->size = size;
    b->used = 0;
    return b;
}

Arena *arena_new(size_t block_size)
{
    Arena *a = calloc(1, sizeof(Arena));
    if (!a) abort();
    a->block_size = block_size ? block_size : ARENA_DEFAULT_BLOCK;
    a->head = block_new(a->block_size);
    return a;
}

void arena_free(Arena *a)
{
    if (!a) return;
    Block *b = a->head;
    while (b) {
        Block *n = b->next;
        free(b);
        b = n;
    }
    if (g_current == a) g_current = NULL;
    free(a);
}

void arena_reset(Arena *a)
{
    if (!a) return;
    Block *b = a->head;
    /* keep the oldest block (last in the list), free the rest */
    while (b && b->next) {
        Block *n = b->next;
        free(b);
        b = n;
    }
    a->head = b;
    if (b) b->used = 0;
    a->total_used = 0;
}

size_t arena_used(const Arena *a) { return a ? a->total_used : 0; }

Arena *arena_current(void)
{
    if (!g_current) g_current = arena_new(0);
    return g_current;
}

Arena *arena_use(Arena *a)
{
    Arena *prev = arena_current();
    g_current = a;
    return prev;
}

void *amalloc(size_t n)
{
    Arena *a = arena_current();
    n = (n + 7u) & ~(size_t)7u;
    if (n == 0) n = 8;
    Block *b = a->head;
    if (!b || b->used + n > b->size) {
        size_t sz = n > a->block_size ? n : a->block_size;
        Block *nb = block_new(sz);
        nb->next = a->head;
        a->head = nb;
        b = nb;
    }
    void *p = b->data + b->used;
    b->used += n;
    a->total_used += n;
    memset(p, 0, n);
    return p;
}

char *astrndup(const char *s, size_t n)
{
    char *d = amalloc(n + 1);
    memcpy(d, s, n);
    d[n] = '\0';
    return d;
}

char *astrdup(const char *s) { return astrndup(s, strlen(s)); }

char *asprintf_a(const char *fmt, ...)
{
    char small[256];
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(small, sizeof small, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return astrdup(""); }
    if ((size_t)n < sizeof small) { va_end(ap2); return astrndup(small, (size_t)n); }
    char *d = amalloc((size_t)n + 1);
    vsnprintf(d, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return d;
}
