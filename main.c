/*
 * Copyright 2013 Mathias Fiedler. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <Elementary.h>

#include <Ecore.h>
#include <Ecore_Evas.h>

#include <Evas.h>
#include <Evas_Engine_Buffer.h>

#include "eplay.h"

#include <unistd.h>
#include <sys/reboot.h>


EAPI_MAIN int elm_main(int argc, char **argv);


static struct eplay g_player;

static void
setup_elm(struct eplay* ep)
{
    Evas* e;
    Evas_Engine_Info_Buffer *einfo;
    int w = ep->overlay[0].width;
    int h = ep->overlay[0].height;

    elm_config_engine_set("ews");
    elm_config_scale_set(2.0);
    ecore_evas_ews_engine_set("buffer", NULL);
    ecore_evas_ews_setup(0, 0, w, h);
    // printf("%s:%i\n", __FUNCTION__, __LINE__);
    e = ecore_evas_ews_evas_get();
    // printf("%s:%i: %p\n", __FUNCTION__, __LINE__, e);
    // ep->ee = ecore_evas_ews_ecore_evas_get();

    //printf("ee: %p, %p, %p\n", ep->ee, ecore_evas_get(ep->ee), e);

    einfo = (Evas_Engine_Info_Buffer *)evas_engine_info_get(e);
    einfo->info.depth_type = EVAS_ENGINE_BUFFER_DEPTH_ARGB32;
    einfo->info.dest_buffer = ep->overlay[0].data;
    einfo->info.dest_buffer_row_bytes = w * 4;
    einfo->info.use_color_key = 0;
    einfo->info.alpha_threshold = 0;
    einfo->info.func.new_update_region = NULL;
    einfo->info.func.free_update_region = NULL;
    einfo->info.func.switch_buffer = eplay_switch_overlay_buffer;
    einfo->info.switch_data = ep;
    evas_engine_info_set(e, (Evas_Engine_Info *)einfo);

    ecore_evas_alpha_set(ecore_evas_ews_ecore_evas_get(), EINA_TRUE);
}

static bool s_poweroff = false;

void eplay_shutdown(struct eplay* ep)
{
    s_poweroff = true;
    elm_exit();
}

int
elm_main(int argc, char **argv)
{
    // if (! eplay_setup_udev(&g_player))
    //     return 1;

    if (! eplay_setup_input(&g_player))
        return 1;

    if (! eplay_setup_udev(&g_player))
        return 1;

    if (! eplay_setup_drm(&g_player))
        return 1;

    //memset(g_player.overlay.data, 0xA0, 4 * g_player.overlay.width * g_player.overlay.height);

    setup_elm(&g_player);

    if (eplay_setup_mixer(&g_player) && eplay_setup_gui(&g_player) && eplay_setup_gstreamer(&g_player))
    {
        elm_run(); // run main loop
    }

    elm_shutdown(); // after mainloop finishes running, shutdown

    eplay_cleanup_gstreamer(&g_player);
    eplay_cleanup_gui(&g_player);
    eplay_cleanup_mixer(&g_player);
    eplay_cleanup_drm(&g_player);
    eplay_cleanup_udev(&g_player);
    eplay_cleanup_input(&g_player);
    // eplay_cleanup_udev(&g_player);

    if (s_poweroff)
    {
        printf("shutdown\n");
        system("poweroff");
    }

    return 0; // exit 0 for exit code
}
ELM_MAIN()
