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

#include "eplay.h"

#include <Elementary.h>
#include <Ecore.h>
#include <Evas.h>
#include <Eeze.h>

#include <dirent.h>

static char* itc_text_get(void *data, Evas_Object *obj, const char *source)
{
    // printf("%s:%i:\n", __FUNCTION__, __LINE__);
    return elm_entry_utf8_to_markup(ecore_file_file_get(data)); /* NOTE this will be free()'d by the caller */
}

static void itc_del(void *data, Evas_Object *obj)
{
    // printf("%s:%i:\n", __FUNCTION__, __LINE__);
    eina_stringshare_del(data);
}

static Eina_Bool itc_state_get(void *data, Evas_Object *obj, const char *source)
{
    // printf("%s:%i:\n", __FUNCTION__, __LINE__);
    return EINA_FALSE;
}

static Evas_Object * itc_icon_file_get(void *data, Evas_Object *obj, const char *source)
{
    Evas_Object *ic;

    // printf("%s:%i:\n", __FUNCTION__, __LINE__);

    if (strcmp(source, "elm.swallow.icon")) return NULL;

    ic = elm_icon_add(obj);
    elm_icon_standard_set(ic, "file");

    evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
    return ic;
}

static Elm_Genlist_Item_Class * create_itc(Elm_Gen_Item_Content_Get_Cb cb)
{
    Elm_Genlist_Item_Class *itc = elm_genlist_item_class_new();

    itc->item_style = "default";
    itc->func.text_get = itc_text_get;
    itc->func.state_get = itc_state_get;
    itc->func.del = itc_del;
    itc->func.content_get = cb;

    return itc;
}

static void populate_list(struct eplay* ep)
{
    Evas_Object* fs = ep->win;
    Eina_List *files = NULL, *dirs = NULL;
    //Eina_List *files = NULL;
    // //Eina_File_Direct_Info *file;
    // const char *file;
    // Eina_Iterator *it;
    const char *entry;

    // if (!ecore_file_is_dir(ep->current_path)) return;

    // it = eina_file_ls(ep->current_path);
    // if (!it) return;

    // elm_genlist_clear(fs);

    // EINA_ITERATOR_FOREACH (it, file)
    // {

    //     // const char *filename;

    //     // if (file->path[file->name_start] == '.') continue;

    //     // printf("%s\n", file->path);

    //     // filename = eina_stringshare_add(file->path);
    //     // if (file->type == EINA_FILE_DIR)
    //     //     dirs = eina_list_append(dirs, filename);
    //     // else
    //     //     files = eina_list_append(files, filename);
    //     files = eina_list_append(files, file);
    // }
    // eina_iterator_free(it);

    if (!ecore_file_is_dir(ep->current_path)) return;

    elm_genlist_clear(fs);

    DIR *dirp = opendir(ep->current_path);
    struct dirent* ent;
    char buf[1024];

    while ((ent = readdir(dirp)))
    {
        if (ent->d_name[0] == '.') continue;
        snprintf(buf, sizeof(buf), "%s/%s", ep->current_path, ent->d_name);

        const char *filename = eina_stringshare_add(buf);
        if (ent->d_type == DT_DIR)
            dirs = eina_list_append(dirs, filename);
        else
            files = eina_list_append(files, filename);
    }

    closedir(dirp);

    files = eina_list_sort(files, eina_list_count(files), EINA_COMPARE_CB(strcoll));

    dirs = eina_list_sort(dirs, eina_list_count(dirs), EINA_COMPARE_CB(strcoll));

    EINA_LIST_FREE(dirs, entry)
    {
        elm_genlist_item_append(fs, ep->itc_dir, entry, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);
    }

    EINA_LIST_FREE(files, entry)
    {
        elm_genlist_item_append(fs, ep->itc_file, entry, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);
    }
}

static
void update_path(struct eplay *ep, const char* path)
{
    strncpy(ep->current_path, path, sizeof(ep->current_path));
    populate_list(ep);
}

static void item_sel_cb(void *data, Evas_Object *obj, void *event_info)
{
    printf("sel item data [%p] on genlist obj [%p], item pointer [%p]\n", data, obj, event_info);
    const char* file = elm_object_item_data_get(event_info);
    struct eplay* ep = data;
    if (ecore_file_is_dir(file))
    {
        update_path(ep, file);
    }
    else
    {
        eplay_play(ep, file);
        evas_object_focus_set(ep->progress, EINA_TRUE);
        evas_object_hide(ep->win);
        eplay_hide_overlay(ep);
    }
}

static
void menu_control_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
    struct eplay *ep = data;
    Evas_Event_Key_Down *ev = event_info;

    if (strcmp(ev->keyname, "Escape") == 0 || strcmp(ev->keyname, "Tab") == 0)
    {
        evas_object_focus_set(ep->progress, EINA_TRUE);
        evas_object_hide(ep->win);
    }
    else if (strcmp(ev->keyname, "BackSpace") == 0)
    {
        char *parent = ecore_file_dir_get(ep->current_path);
        printf("path: %s\n", parent);
        update_path(ep, parent);
        free(parent);
    }
    else if (strcmp(ev->keyname, "End") == 0 &&
        evas_key_modifier_is_set(ev->modifiers, "Alt") &&
         evas_key_modifier_is_set(ev->modifiers, "Control"))
    {
        eplay_shutdown(ep);
    }
}

static
void delete_timer(struct eplay* ep)
{
    if (ep->timer) ecore_timer_del(ep->timer);
    ep->timer = NULL;
}

static
void control_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
    struct eplay *ep = data;
    Evas_Event_Key_Down *ev = event_info;

    bool playing = eplay_is_playing(ep);

    if (strcmp(ev->keyname, "Left") == 0)
    {
        elm_progressbar_value_set(obj, eplay_seek(ep, -5));
        eplay_show_overlay(ep);
    }
    else if (strcmp(ev->keyname, "Right") == 0)
    {
        elm_progressbar_value_set(obj, eplay_seek(ep, 5));
        eplay_show_overlay(ep);
    }
    else if (strcmp(ev->keyname, "Up") == 0)
    {
        elm_progressbar_value_set(obj, eplay_seek(ep, -30));
        eplay_show_overlay(ep);
    }
    else if (strcmp(ev->keyname, "Down") == 0)
    {
        elm_progressbar_value_set(obj, eplay_seek(ep, 30));
        eplay_show_overlay(ep);
    }
    else if (strcmp(ev->keyname, "space") == 0)
    {
        eplay_set_playing(ep, !playing);
        elm_progressbar_value_set(obj, eplay_get_progress(ep));
        delete_timer(ep);
        if (playing)
            eplay_show_overlay(ep);
        else
            eplay_hide_overlay(ep);
    }
    else if (strcmp(ev->keyname, "Escape") == 0 || strcmp(ev->keyname, "Tab") == 0)
    {
        evas_object_show(ep->win);
        evas_object_focus_set(ep->win, EINA_TRUE);
    }
    else if (strcmp(ev->keyname, "a") == 0)
    {
        eplay_switch_audio(ep);
    }
}

static
Eina_Bool timer_cb(void *data)
{
    struct eplay *ep = data;
    ep->timer = NULL;
    eplay_hide_overlay(ep);
    return ECORE_CALLBACK_CANCEL;
}

static
void set_overlay_timeout(struct eplay* ep)
{
    delete_timer(ep);
    ecore_timer_add(3.0, timer_cb, ep);
}

static
void volume_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
    struct eplay *ep = data;
    Evas_Event_Key_Down *ev = event_info;
    long vol = eplay_get_volume(ep);
    bool show = ep->show_overlay;

    if (strcmp(ev->keyname, "XF86AudioLowerVolume") == 0)
    {
        eplay_set_volume(ep, vol - 1);
        eplay_show_overlay(ep);
    }
    else if (strcmp(ev->keyname, "XF86AudioRaiseVolume") == 0)
    {
        eplay_set_volume(ep, vol + 1);
        eplay_show_overlay(ep);
    }
    else if (strcmp(ev->keyname, "XF86AudioMute") == 0)
    {
        //elm_progressbar_value_set(obj, eplay_seek(ep, -20));
        printf("todo\n");
        eplay_show_overlay(ep);
    }

    if (!show)
    {
        set_overlay_timeout(ep);
    }

    elm_slider_value_set(obj, (double)eplay_get_volume(ep));
}

void eplay_refresh_browser(struct eplay* ep)
{
    populate_list(ep);
}

bool eplay_setup_gui(struct eplay* ep)
{
    Evas_Object *win, *box, *hbox, *bg, /* *lab, *btn, */*fs;
    //char* home = getenv("HOME");
    char* home = "/media";

    elm_config_focus_highlight_enabled_set(EINA_TRUE);
    
    // new window - do the usual and give it a name (hello) and title (Hello)
    //win = elm_win_util_standard_add("hello", "Hello");
    win = elm_win_add(NULL, "eplay", ELM_WIN_BASIC);

    ep->ee = ecore_evas_ecore_evas_get(evas_object_evas_get(win));
    ecore_evas_input_event_register(ep->ee);

    box = elm_box_add(win);
    evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_win_resize_object_add(win, box);
    evas_object_show(box);

    hbox = elm_box_add(box);
    elm_box_horizontal_set(hbox, EINA_TRUE);
    evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_show(hbox);
    elm_box_pack_end(box, hbox);
    
    fs = elm_genlist_add(hbox);
    elm_genlist_mode_set(fs, ELM_LIST_LIMIT);
    ep->itc_file = create_itc(itc_icon_file_get);
    ep->itc_dir = create_itc(itc_icon_file_get);
    ep->win = fs;
    evas_object_size_hint_weight_set(fs, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(fs, 0.0, EVAS_HINT_FILL);
    evas_object_smart_callback_add(fs, "activated", item_sel_cb, ep);
    //elm_win_resize_object_add(win, fs);
    elm_box_pack_end(hbox, fs);
    evas_object_show(fs);

    ep->slider = elm_slider_add(hbox);
    elm_slider_horizontal_set(ep->slider, EINA_FALSE);
    elm_slider_min_max_set(ep->slider, ep->mixer_elem_min, ep->mixer_elem_max);
    elm_slider_inverted_set(ep->slider, EINA_TRUE);
    elm_slider_value_set(ep->slider, (double)eplay_get_volume(ep));
    evas_object_size_hint_weight_set(ep->slider, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(ep->slider, 1.0, EVAS_HINT_FILL);
    if (!(evas_object_key_grab(ep->slider, "XF86AudioMute", 0, 0, EINA_TRUE) &&
        evas_object_key_grab(ep->slider, "XF86AudioLowerVolume", 0, 0, EINA_TRUE) &&
        evas_object_key_grab(ep->slider, "XF86AudioRaiseVolume", 0, 0, EINA_TRUE)))
        return false;

    elm_box_pack_end(hbox, ep->slider);
    evas_object_show(ep->slider);

    ep->progress = elm_progressbar_add(box);
    evas_object_size_hint_weight_set(ep->progress, EVAS_HINT_EXPAND, 0.1);
    evas_object_size_hint_align_set(ep->progress, EVAS_HINT_FILL, 1.0);
    //evas_object_shaped_set(ep->progress, EINA_FALSE);
    evas_object_color_set(ep->progress, 255, 255, 255, 255);
    //evas_object_render_op_set(ep->progress, EVAS_RENDER_COPY);
    //elm_win_resize_object_add(win, ep->progress);
    elm_box_pack_end(box, ep->progress);
    evas_object_show(ep->progress);

    bg = elm_bg_add(ep->progress);
    elm_bg_color_set(bg, 255, 0, 0);

    evas_object_resize(win, ep->overlay[0].width, ep->overlay[0].height);
    //elm_win_focus_highlight_enabled_set(win, EINA_TRUE);
    evas_object_show(win);

    evas_object_focus_set(fs, EINA_TRUE);

    evas_object_event_callback_add(ep->slider, EVAS_CALLBACK_KEY_DOWN, volume_cb, ep);
    evas_object_event_callback_add(ep->progress, EVAS_CALLBACK_KEY_DOWN, control_cb, ep);
    evas_object_event_callback_add(ep->win, EVAS_CALLBACK_KEY_DOWN, menu_control_cb, ep);

    //elm_object_focus_set(win, EINA_TRUE);

    if (home == NULL) home = "/";

    update_path(ep, home);

    return true;
}

void eplay_cleanup_gui(struct eplay* ep)
{
    delete_timer(ep);
    elm_genlist_item_class_free(ep->itc_file);
    elm_genlist_item_class_free(ep->itc_dir);
}
