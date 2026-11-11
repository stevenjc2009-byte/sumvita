/* result.c - owned copies of engine results. calc_run allocates in the current
 * arena; we switch to a private arena, copy everything out, then reset it. */
#include "ui/result.h"

#include "engine/arena.h"
#include "ui/log.h"
#include "ui/note_text.h"

#include <psp2/kernel/processmgr.h>
#include <stdlib.h>
#include <string.h>

static Arena *g_arena;

/* step titles and notes: words with ASCII maths bits shown as the maths line shows them */
static char *pretty(const char *s)
{
	char *d = note_text(s);
	return d ? d : calloc(1, 1);
}

static char *dup(const char *s)
{
	char *d = strdup(s ? s : "");
	return d ? d : calloc(1, 1);
}

void res_init(Res *r)
{
	memset(r, 0, sizeof *r);
	r->error_pos = -1;
	r->action = ACT_AUTO;
}

void res_free(Res *r)
{
	free(r->label);
	free(r->input);
	free(r->echo);
	free(r->answer);
	free(r->alt);
	for (int i = 0; i < r->nsteps; i++) {
		free(r->steps[i].title);
		free(r->steps[i].note);
		free(r->steps[i].math);
	}
	free(r->steps);
	res_init(r);
}

void res_move(Res *dst, Res *src)
{
	*dst = *src;
	res_init(src);
}

static void copy_out(Res *out, const CalcResult *cr, const char *input, const char *label, int action)
{
	res_free(out);
	out->ok = cr->ok;
	out->action = action;
	out->label = dup(label);
	out->input = dup(input);
	out->echo = dup(cr->input_echo ? cr->input_echo : input);
	out->answer = dup(cr->answer);
	out->alt = dup(cr->answer_alt);
	out->value = cr->value;
	out->has_value = cr->has_value;
	memcpy(out->error, cr->error, sizeof out->error);
	out->error[sizeof out->error - 1] = 0;
	out->error_pos = cr->error_pos;
	if (!cr->ok && !out->error[0])
		strcpy(out->error, "Can't do that one");
	int n = cr->steps.n > 0 && cr->steps.v ? cr->steps.n : 0;
	if (n) {
		out->steps = calloc((size_t)n, sizeof(RStep));
		if (!out->steps)
			n = 0;
	}
	for (int i = 0; i < n; i++) {
		out->steps[i].title = pretty(cr->steps.v[i].title);
		out->steps[i].note = pretty(cr->steps.v[i].note);
		out->steps[i].math = dup(cr->steps.v[i].math);
		out->steps[i].depth = cr->steps.v[i].depth;
	}
	out->nsteps = n;
}

static Arena *begin(void)
{
	if (!g_arena)
		g_arena = arena_new(0);
	Arena *prev = arena_use(g_arena);
	arena_reset(g_arena);
	return prev;
}

static void end(Arena *prev)
{
	arena_reset(g_arena);
	arena_use(prev);
}

void res_run(Res *out, CalcAction a, const char *input, const CalcSettings *s)
{
	uint64_t t0 = sceKernelGetProcessTimeWide();
	Arena *prev = begin();
	CalcResult cr;
	memset(&cr, 0, sizeof cr);
	calc_run(a, input, s, &cr);
	size_t used = arena_used(g_arena);
	copy_out(out, &cr, input, a == ACT_AUTO ? "" : calc_action_name(a), (int)a);
	end(prev);
	log_printf("calc %d '%s' -> ok=%d '%s' alt='%s' steps=%d err='%s' pos=%d (%lu us, arena %lu B)", (int)a,
		   input, out->ok, out->answer, out->alt, out->nsteps, out->error, out->error_pos,
		   (unsigned long)(sceKernelGetProcessTimeWide() - t0), (unsigned long)used);
}

void res_run_tool(Res *out, const Tool *t, const char *const *vals, const CalcSettings *s)
{
	uint64_t t0 = sceKernelGetProcessTimeWide();
	Arena *prev = begin();
	CalcResult cr;
	memset(&cr, 0, sizeof cr);
	cr.error_pos = -1;
	t->run(vals, s, &cr);
	copy_out(out, &cr, "", t->name, -1);
	end(prev);
	log_printf("tool %s -> ok=%d '%s' steps=%d err='%s' (%lu us)", t->id, out->ok, out->answer, out->nsteps,
		   out->error, (unsigned long)(sceKernelGetProcessTimeWide() - t0));
}

void res_clone(Res *dst, const Res *src)
{
	res_init(dst);
	dst->ok = src->ok;
	dst->action = src->action;
	dst->label = dup(src->label);
	dst->input = dup(src->input);
	dst->echo = dup(src->echo);
	dst->answer = dup(src->answer);
	dst->alt = dup(src->alt);
	dst->value = src->value;
	dst->has_value = src->has_value;
	memcpy(dst->error, src->error, sizeof dst->error);
	dst->error_pos = src->error_pos;
	if (src->nsteps > 0) {
		dst->steps = calloc((size_t)src->nsteps, sizeof(RStep));
		if (!dst->steps)
			return;
		for (int i = 0; i < src->nsteps; i++) {
			dst->steps[i].title = dup(src->steps[i].title);
			dst->steps[i].note = dup(src->steps[i].note);
			dst->steps[i].math = dup(src->steps[i].math);
			dst->steps[i].depth = src->steps[i].depth;
		}
		dst->nsteps = src->nsteps;
	}
}
