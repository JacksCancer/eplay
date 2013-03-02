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

#include <Ecore.h>
#include <Ecore_Input_Evas.h>
#include <Eeze.h>

#include <linux/input.h>

#include <xkbcommon/xkbcommon.h>

#include <fcntl.h>
#include <unistd.h>

#define MOD_SHIFT_MASK 0x01
#define MOD_ALT_MASK 0x02
#define MOD_CONTROL_MASK 0x04

static const char* keysyms[];

static Eina_Bool
handle_input_event(void *data, Ecore_Fd_Handler *handler)
{
    struct input_event ev;
    struct eplay* ep = data;
    int fd = ecore_main_fd_handler_fd_get(handler);

    if (ecore_main_fd_handler_active_get(handler, ECORE_FD_ERROR))
    {
        printf("An error has occurred. Stop watching this fd.\n");
        close(fd);
        ep->input_handler = eina_list_remove(ep->input_handler, handler);
        return ECORE_CALLBACK_CANCEL;
    }

    while (read(fd, &ev, sizeof ev) == sizeof ev)
    {
        //printf("%s:%i\n", __FUNCTION__, __LINE__);

        switch (ev.type)
        {
        case EV_REL:
            switch (ev.code)
            {
            case REL_WHEEL:
                break;

            case REL_HWHEEL:
                break;
            }
            break;

        case EV_KEY:
            {
                int space = 128;
                Ecore_Event_Key *e = calloc(1, sizeof(*e) + space);
                xkb_keycode_t keycode = ev.code + 8;
                xkb_keysym_t keysym;
                char* buffer;
                int size;
                xkb_mod_mask_t mask;


                //printf("key: %u, %u, %s\n", ev.code, ev.value, keysyms[ev.code * 7]);
                if ((ev.value | 1) == 1)
                    xkb_state_update_key(ep->xkb_state, keycode, ev.value ? XKB_KEY_DOWN : XKB_KEY_UP);

                mask = xkb_state_serialize_mods(ep->xkb_state, XKB_STATE_DEPRESSED | XKB_STATE_LATCHED);

                e->modifiers = 0;

                if (mask & ep->xkb_control_mask)
                    e->modifiers |= MOD_CONTROL_MASK;
                if (mask & ep->xkb_alt_mask)
                    e->modifiers |= MOD_ALT_MASK;
                if (mask & ep->xkb_shift_mask)
                    e->modifiers |= MOD_SHIFT_MASK;

                printf("modifier: %u, %u\n", e->modifiers, mask);

                keysym = xkb_state_key_get_one_sym(ep->xkb_state, keycode);

                
                buffer = (char*)(e + 1);
                e->string = buffer;
                e->compose = buffer;

                size = xkb_keysym_to_utf8(keysym, buffer, space) + 1;
                space -= size;
                buffer += size;

                e->key = buffer;
                e->keyname = buffer;
                size = xkb_keysym_get_name(keysym, buffer, space) + 1;

                printf("key: %u, %u, %s, %s\n", ev.code, ev.value, e->key, e->compose);

                e->timestamp = ev.time.tv_sec * 1000 + ev.time.tv_usec / 1000;
                e->window = (Ecore_Window)ep->ee;
                e->event_window = (Ecore_Window)ep->ee;

                if (ecore_event_add(ev.value ? ECORE_EVENT_KEY_DOWN: ECORE_EVENT_KEY_UP, e, NULL, NULL))
                    e = NULL;

                free(e);
            }

            break;
        }
    }

    return ECORE_CALLBACK_RENEW;
    //return ECORE_CALLBACK_DONE;
}

bool
eplay_setup_input(struct eplay* ep)
{
    ecore_event_init();
    ecore_event_evas_init();

    ecore_evas_input_event_register(ecore_evas_ews_ecore_evas_get());

    if (eeze_init() < 0)
        return false;

    struct xkb_rule_names rule_names = {
        .rules = "evdev",
        .model = "pc105",
        .layout = "us",
        .variant = "",
        .options = ""
    };

    if ((ep->xkb = xkb_context_new(0)) == NULL ||
        (ep->xkb_keymap = xkb_keymap_new_from_names(ep->xkb, &rule_names, XKB_MAP_COMPILE_PLACEHOLDER)) == NULL ||
        (ep->xkb_state = xkb_state_new(ep->xkb_keymap)) == NULL)
    {
        fprintf(stderr, "failed to compile keymap\n");
        return false;
    }

    ep->xkb_control_mask = 1 << xkb_map_mod_get_index(ep->xkb_keymap, "Control");
    ep->xkb_alt_mask = 1 << xkb_map_mod_get_index(ep->xkb_keymap, "Mod1");
    ep->xkb_shift_mask = 1 << xkb_map_mod_get_index(ep->xkb_keymap, "Shift");

    const char *sys;
    Eina_List *l, *sysdevs = eeze_udev_find_by_filter("input", NULL, NULL);

    printf("input devices:\n");
    EINA_LIST_FOREACH(sysdevs, l, sys)
    {
        const char* devnode = eeze_udev_syspath_get_devpath(sys);
        if (devnode)
        {
            printf("input: %s\n", devnode);
            int fd = open(devnode, O_RDONLY | O_NONBLOCK);
            if (fd < 0)
            {
                perror(devnode);
                continue;
            }

            Ecore_Fd_Handler* handler = ecore_main_fd_handler_add(fd, ECORE_FD_READ | ECORE_FD_ERROR, handle_input_event, ep, NULL, NULL);

            if (handler)
            {
                ep->input_handler = eina_list_append(ep->input_handler, handler);
            }
        }
    }

    return true;
}

void
eplay_cleanup_input(struct eplay* ep)
{
    Ecore_Fd_Handler* handler;
    Eina_List *l;
    EINA_LIST_FOREACH(ep->input_handler, l, handler)
    {
        close(ecore_main_fd_handler_fd_get(handler));
        ecore_main_fd_handler_del(handler);
    }

    eina_list_free(ep->input_handler);

    if (ep->xkb_state)
        xkb_state_unref(ep->xkb_state);
    if (ep->xkb_keymap)
        xkb_map_unref(ep->xkb_keymap);
    if (ep->xkb)
        xkb_context_unref(ep->xkb);

    eeze_shutdown();
    ecore_event_evas_shutdown();
    ecore_event_shutdown();
}

