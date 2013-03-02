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
#include <Eina.h>
#include <Ecore_Evas.h>

#include <gst/gst.h>

#include <alsa/asoundlib.h>

#include <stdbool.h>
#include <stdint.h>

struct drm_buffer
{
    struct omap_bo *bo;
    uint32_t fb_id;
    int width, height;
    void *data;
};

struct eplay
{
    struct omap_device* dev;
    int drm_fd;
    uint32_t c_id;
    uint32_t crtc;
    uint32_t planes[2];
    bool show_overlay;
    int current_ov_buffer;
    struct drm_buffer overlay[2];
    struct drm_buffer bg;

    Ecore_Evas* ee;
    Evas_Object* win;
    Evas_Object* progress;
    Evas_Object* slider;
    Ecore_Timer* timer;

    Eina_List* input_handler;

    struct xkb_context* xkb;
    struct xkb_state* xkb_state;
    struct xkb_keymap* xkb_keymap;
    int xkb_control_mask;
    int xkb_alt_mask;
    int xkb_shift_mask;

    char current_path[PATH_MAX];
    Elm_Genlist_Item_Class *itc_dir;
    Elm_Genlist_Item_Class *itc_file;

    GstElement *playbin;
    gint64 duration;

    snd_mixer_t *mixer;
    snd_mixer_elem_t *mixer_elem;
    long mixer_elem_min;
    long mixer_elem_max;

    struct udev *udev;
    struct udev_monitor *disk_monitor;
    int udev_fd;
    Ecore_Fd_Handler* udev_handler;
    Eina_List* mount_list;
};

void eplay_shutdown(struct eplay* ep);

bool eplay_setup_drm(struct eplay* ep);
void eplay_cleanup_drm(struct eplay* ep);
bool eplay_show_overlay(struct eplay* ep);
void eplay_hide_overlay(struct eplay* ep);
void* eplay_switch_overlay_buffer(void *data, void *dest_buffer);

bool eplay_setup_input(struct eplay* ep);
void eplay_cleanup_input(struct eplay* ep);

bool eplay_setup_gui(struct eplay* ep);
void eplay_cleanup_gui(struct eplay* ep);
void eplay_refresh_browser(struct eplay* ep);

bool eplay_setup_gstreamer(struct eplay* ep);
void eplay_cleanup_gstreamer(struct eplay* ep);

void eplay_play(struct eplay* ep, const char* file);
double eplay_seek(struct eplay* ep, int offset);
void eplay_set_playing(struct eplay* ep, bool play);
bool eplay_is_playing(struct eplay* ep);
double eplay_get_progress(struct eplay* ep);
void eplay_switch_audio(struct eplay* ep);

bool eplay_setup_mixer(struct eplay* ep);
void eplay_cleanup_mixer(struct eplay*ep);
long eplay_get_volume(struct eplay *ep);
void eplay_set_volume(struct eplay *ep, long val);

bool eplay_setup_udev(struct eplay* ep);
void eplay_cleanup_udev(struct eplay* ep);
