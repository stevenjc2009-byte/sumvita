/* Sumvita - the ultimate calculator for PS Vita. Boot and main loop. */
#include <psp2/display.h>
#include <psp2/kernel/processmgr.h>
#include <vita2d.h>

#include "ui/app.h"
#include "ui/font.h"
#include "ui/input.h"
#include "ui/log.h"
#include "ui/mlayout.h"
#include "ui/osk.h"
#include "ui/perf.h"
#include "ui/redraw.h"
#include "ui/settings.h"
#include "ui/theme.h"

/* newlib heap (extended memory mode, ATTRIBUTE2=12): engine arenas, layouts */
int _newlib_heap_size_user = 128 * 1024 * 1024;

int main(void)
{
	log_init();
	log_printf("Sumvita %s boot", SUMVITA_VERSION);

	vita2d_init();
	vita2d_set_clear_color(COL_BG);
	vita2d_set_vblank_wait(1);
	font_init();
	if (!font_ok())
		log_printf("font: no regular face loaded");
	input_init();
	if (osk_init() < 0)
		log_printf("osk: init failed");
	settings_load();
	app_init();
	log_set_sync(tour_active());
	log_printf("ready: confirm button is %s", input_confirm_is_cross() ? "Cross" : "Circle");

	input_t in;
	while (!app_quit_requested()) {
		input_poll(&in);
		tour_input(&in);
		app_update(&in);
		mlc_new_frame();
		log_tick();
		/* ---- idle skip (v1.2): nothing can have changed, so keep the last frame ---- */
		if (!redraw_needed(&in)) {
			sceDisplayWaitVblankStart();
			continue;
		}
		/* ---- end idle skip ---- */
		vita2d_start_drawing();
		vita2d_clear_screen();
		perf_frame_begin();
		app_draw();
		perf_frame_end();
		vita2d_end_drawing();
		vita2d_swap_buffers();
	}

	log_printf("exit");
	log_close();
	osk_shutdown();
	/* no vita2d_fini(): it hangs on Vita3K, and the process is going anyway */
	sceKernelExitProcess(0);
	return 0;
}
