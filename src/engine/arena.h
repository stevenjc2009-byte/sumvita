/* Bump allocator for engine scratch memory.
 *
 * Every Expr node, string and temporary made while running one calculation is
 * allocated from the CURRENT arena. The UI resets the arena after copying the
 * result out as text, so the engine never frees individual nodes.
 *
 * Graph mode keeps its own arena per plotted function (arena_new / arena_use).
 */
#ifndef ENGINE_ARENA_H
#define ENGINE_ARENA_H

#include <stddef.h>

typedef struct Arena Arena;

Arena *arena_new(size_t block_size);   /* block_size 0 = default (256 KB) */
void   arena_free(Arena *a);
void   arena_reset(Arena *a);          /* drop everything, keep the first block */
size_t arena_used(const Arena *a);

/* The arena that engine allocations go to. A default one exists at start-up. */
Arena *arena_current(void);
Arena *arena_use(Arena *a);            /* returns the previous arena */

void *amalloc(size_t n);               /* zeroed, 8-byte aligned; aborts on OOM */
char *astrdup(const char *s);
char *astrndup(const char *s, size_t n);
char *asprintf_a(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
