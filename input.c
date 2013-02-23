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

#include <fcntl.h>
#include <unistd.h>

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
        Ecore_Event_Key *e = calloc(1, sizeof *e);
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

            printf("key: %u, %u, %s\n", ev.code, ev.value, keysyms[ev.code * 7]);

            e->keyname = keysyms[ev.code * 7];
            e->key = keysyms[ev.code * 7];
            e->compose = keysyms[ev.code * 7 + 3];
            e->string = e->compose;

            e->timestamp = ev.time.tv_sec * 1000 + ev.time.tv_usec / 1000;
            e->window = (Ecore_Window)ep->ee;
            e->event_window = (Ecore_Window)ep->ee;

            if (ecore_event_add(ev.value ? ECORE_EVENT_KEY_DOWN: ECORE_EVENT_KEY_UP, e, NULL, NULL))
                e = NULL;

            break;
        }

        free(e);
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

    eeze_shutdown();
    ecore_event_evas_shutdown();
    ecore_event_shutdown();
}


/* this table was taken from ecore_fb, is the default en layout */
static const char* keysyms[] = {
         "0x00",          "0x00",          "0x00", /**/"",    "",    "",  NULL,/***/
       "Escape",        "Escape",        "Escape", /**/"",    "",    "",  "\x1b",/***/
            "1",        "exclam",             "1", /**/"1",   "!",   "1",  NULL,/***/
            "2",            "at",             "2", /**/"2",   "@",   "2",  "",/***/
            "3",    "numbersign",             "3", /**/"3",   "#",   "3",  "\x1b",/***/
            "4",        "dollar",             "4", /**/"4",   "$",   "4",  "\x1c",/***/
            "5",       "percent",             "5", /**/"5",   "%",   "5",  "\x1d",/***/
            "6",  "asciicircumm",             "6", /**/"6",   "^",   "6",  "\x1e",/***/
            "7",     "ampersand",             "7", /**/"7",   "&",   "7",  "\x1f",/***/
            "8",       "asterisk",            "8", /**/"8",   "*",   "8",  "\x7f",/***/
            "9",     "parenleft",             "9", /**/"9",   "(",   "9",  NULL,/***/
            "0",    "parenright",             "0", /**/"0",   ")",   "0",  NULL,/***/
        "minus",    "underscore",         "minus", /**/"-",   "_",   "-",  NULL,/***/
        "equal",          "plus",         "equal", /**/"=",   "+",   "=",  NULL,/***/
    "BackSpace",     "BackSpace",     "BackSpace", /**/"\010","\010","\010",  NULL,/***/
          "Tab",  "ISO_Left_Tab",           "Tab", /**/"\011","",    "\011",  NULL,/***/
            "q",             "Q",             "Q", /**/"q",   "Q",   "Q",  "\x11",/***/
            "w",             "W",             "W", /**/"w",   "W",   "W",  "\x17",/***/
            "e",             "E",             "E", /**/"e",   "E",   "E",  "\x05",/***/
            "r",             "R",             "R", /**/"r",   "R",   "R",  "\x12",/***/
            "t",             "T",             "T", /**/"t",   "T",   "T",  "\x14",/***/
            "y",             "Y",             "Y", /**/"y",   "Y",   "Y",  "\x19",/***/
            "u",             "U",             "U", /**/"u",   "U",   "U",  "\x15",/***/
            "i",             "I",             "I", /**/"i",   "I",   "I",  "\x09",/***/
            "o",             "O",             "O", /**/"o",   "O",   "O",  "\x0f",/***/
            "p",             "P",             "P", /**/"p",   "P",   "P",  "\x10",/***/
  "bracketleft",     "braceleft",   "bracketleft", /**/"[",   "{",   "[",  "\x1b",/***/
 "bracketright",    "braceright",  "bracketright", /**/"]",   "}",   "]",  "\x1d",/***/
       "Return",        "Return",        "Return", /**/"\015","\015","\015",  NULL,/***/
    "Control_L",     "Control_L",     "Control_L", /**/"",    "",    "",  NULL,/***/
            "a",             "A",             "A", /**/"a",   "A",   "A",  "\x01",/***/
            "s",             "S",             "S", /**/"s",   "S",   "S",  "\x13",/***/
            "d",             "D",             "D", /**/"d",   "D",   "D",  "\x04",/***/
            "f",             "F",             "F", /**/"f",   "F",   "F",  "\x06",/***/
            "g",             "G",             "G", /**/"g",   "G",   "G",  "\x07",/***/
            "h",             "h",             "H", /**/"h",   "H",   "H",  "\x08",/***/
            "j",             "J",             "J", /**/"j",   "J",   "J",  "\x0a",/***/
            "k",             "K",             "K", /**/"k",   "K",   "K",  "\x0b",/***/
            "l",             "L",             "L", /**/"l",   "L",   "L",  "\x0c",/***/
    "semicolon",         "colon",     "semicolon", /**/";",   ":",   ";",  NULL,/***/
   "apostrophe",      "quotedbl",    "apostrophe", /**/"'",   "\"",  "'",  NULL,/***/
        "grave",    "asciitilde",         "grave", /**/"`",   "~",   "`",  "",/***/
      "Shift_L",       "Shift_L",       "Shift_L", /**/"",    "",    "",  NULL,/***/
    "backslash",           "bar",     "backslash", /**/"\\",  "|",   "\\",  "\x1c",/***/
            "z",             "Z",             "Z", /**/"z",   "Z",   "Z",  "\x1a",/***/
            "x",             "X",             "X", /**/"x",   "X",   "X",  "\x18",/***/
            "c",             "C",             "C", /**/"c",   "C",   "C",  "\x03",/***/
            "v",             "V",             "V", /**/"v",   "V",   "V",  "\x16",/***/
            "b",             "B",             "B", /**/"b",   "B",   "B",  "\x02",/***/
            "n",             "N",             "N", /**/"n",   "N",   "N",  "\x0e",/***/
            "m",             "M",             "M", /**/"m",   "M",   "M",  "\x0d",/***/
        "comma",          "less",         "comma", /**/",",   "<",   ",",  NULL,/***/
       "period",       "greater",        "period", /**/".",   ">",   ".",  NULL,/***/
        "slash",      "question",         "slash", /**/"/",   "?",   "/",  "",/***/
      "Shift_R",       "Shift_R",       "Shift_R", /**/"",    "",    "",  NULL,/***/
  "KP_Multiply",   "KP_Multiply",   "KP_Multiply", /**/"",    "*",   "",  NULL,/***/
        "Alt_L",         "Alt_L",         "Alt_L", /**/"",    "",    "",  NULL,/***/
        "space",         "space",         "space", /**/" ",   " ",   " ",  "",/***/
    "Caps_Lock",     "Caps_Lock",     "Caps_Lock", /**/"",    "",    "",  NULL,/***/
           "F1",            "F1",            "F1", /**/"",    "",    "",  NULL,/***/
           "F2",            "F2",            "F2", /**/"",    "",    "",  NULL,/***/
           "F3",            "F3",            "F3", /**/"",    "",    "",  NULL,/***/
           "F4",            "F4",            "F4", /**/"",    "",    "",  NULL,/***/
           "F5",            "F5",            "F5", /**/"",    "",    "",  NULL,/***/
           "F6",            "F6",            "F6", /**/"",    "",    "",  NULL,/***/
           "F7",            "F7",            "F7", /**/"",    "",    "",  NULL,/***/
           "F8",            "F8",            "F8", /**/"",    "",    "",  NULL,/***/
           "F9",            "F9",            "F9", /**/"",    "",    "",  NULL,/***/
          "F10",           "F10",           "F10", /**/"",    "",    "",  NULL,/***/
     "Num_Lock",      "Num_Lock",      "Num_Lock", /**/"",    "",    "",  NULL,/***/
  "Scroll_Lock",   "Scroll_Lock",   "Scroll_Lock", /**/"",    "",    "",  NULL,/***/
      "KP_Home",          "KP_7",       "KP_Home", /**/"",    "7",   "",  NULL,/***/
        "KP_Up",          "KP_8",         "KP_Up", /**/"",    "8",   "",  NULL,/***/
     "KP_Prior",          "KP_9",      "KP_Prior", /**/"",    "9",   "",  NULL,/***/
  "KP_Subtract",   "KP_Subtract",   "KP_Subtract", /**/"",    "",    "",  NULL,/***/
      "KP_Left",          "KP_4",       "KP_Left", /**/"",    "4",   "",  NULL,/***/
     "KP_Begin",          "KP_5",      "KP_Begin", /**/"",    "5",   "",  NULL,/***/
     "KP_Right",          "KP_6",      "KP_Right", /**/"",    "6",   "",  NULL,/***/
       "KP_Add",        "KP_Add",        "KP_Add", /**/"",    "",    "",  NULL,/***/
       "KP_End",          "KP_1",        "KP_End", /**/"",    "1",   "",  NULL,/***/
      "KP_Down",          "KP_2",       "KP_Down", /**/"",    "2",   "",  NULL,/***/
      "KP_Next",          "KP_3",       "KP_Next", /**/"",    "3",   "",  NULL,/***/
    "KP_Insert",          "KP_0",     "KP_Insert", /**/"",    "0",   "",  NULL,/***/
    "KP_Delete",    "KP_Decimal",     "KP_Delete", /**/"",    ".",   "",  NULL,/***/
         "0x54",          "0x54",          "0x54", /**/"",    "",    "",  NULL,/***/
         "0x55",          "0x55",          "0x55", /**/"",    "",    "",  NULL,/***/
         "0x56",          "0x56",          "0x56", /**/"",    "",    "",  NULL,/***/
          "F11",           "F11",           "F11", /**/"",    "",    "",  NULL,/***/
          "F12",           "F12",           "F12", /**/"",    "",    "",  NULL,/***/
         "0x59",          "0x59",          "0x59", /**/"",    "",    "",  NULL,/***/
         "0x5a",          "0x5a",          "0x5a", /**/"",    "",    "",  NULL,/***/
         "0x5b",          "0x5b",          "0x5b", /**/"",    "",    "",  NULL,/***/
         "0x5c",          "0x5c",          "0x5c", /**/"",    "",    "",  NULL,/***/
         "0x5d",          "0x5d",          "0x5d", /**/"",    "",    "",  NULL,/***/
         "0x5e",          "0x5e",          "0x5e", /**/"",    "",    "",  NULL,/***/
         "0x5f",          "0x5f",          "0x5f", /**/"",    "",    "",  NULL,/***/
     "KP_Enter",      "KP_Enter",      "KP_Enter", /**/"\015", "\015", "\015",  NULL,/***/
    "Control_R",     "Control_R",     "Control_R", /**/"",    "",    "",  NULL,/***/
    "KP_Divide",     "KP_Divide",     "KP_Divide", /**/"",    "",    "",  NULL,/***/
        "Print",         "Print",         "Print", /**/"",    "",    "",  NULL,/***/
        "Alt_R",         "Alt_R",         "Alt_R", /**/"",    "",    "",  NULL,/***/
         "0x65",          "0x65",          "0x65", /**/"",    "",    "",  NULL,/***/
         "Home",          "Home",          "Home", /**/"",    "",    "",  NULL,/***/
           "Up",            "Up",            "Up", /**/"",    "",    "",  NULL,/***/
        "Prior",         "Prior",         "Prior", /**/"",    "",    "",  NULL,/***/
         "Left",          "Left",          "Left", /**/"",    "",    "",  NULL,/***/
        "Right",         "Right",         "Right", /**/"",    "",    "",  NULL,/***/
          "End",           "End",           "End", /**/"",    "",    "",  NULL,/***/
         "Down",          "Down",          "Down", /**/"",    "",    "",  NULL,/***/
         "Next",          "Next",          "Next", /**/"",    "",    "",  NULL,/***/
       "Insert",        "Insert",        "Insert", /**/"",    "",    "",  NULL,/***/
       "Delete",        "Delete",        "Delete", /**/"\177","\177","\177",  NULL,/***/
         "0x70",          "0x70",          "0x70", /**/"",    "",    "",  NULL,/***/
"XF86AudioMute", "XF86AudioMute", "XF86AudioMute", /**/"",    "",    "",  NULL,/***/
"XF86AudioLowerVolume", "XF86AudioLowerVolume", "XF86AudioLowerVolume", /**/"",    "",    "",  NULL,/***/
"XF86AudioRaiseVolume", "XF86AudioRaiseVolume", "XF86AudioRaiseVolume", /**/"",    "",    "",  NULL,/***/
         "0x74",          "0x74",          "0x74", /**/"",    "",    "",  NULL,/***/
         "0x75",          "0x75",          "0x75", /**/"",    "",    "",  NULL,/***/
         "0x76",          "0x76",          "0x76", /**/"",    "",    "",  NULL,/***/
        "Pause",         "Pause",         "Pause", /**/"",    "",    "",  NULL,/***/
         "0x78",          "0x78",          "0x78", /**/"",    "",    "",  NULL,/***/
         "0x79",          "0x79",          "0x79", /**/"",    "",    "",  NULL,/***/
         "0x7a",          "0x7a",          "0x7a", /**/"",    "",    "",  NULL,/***/
         "0x7b",          "0x7b",          "0x7b", /**/"",    "",    "",  NULL,/***/
         "0x7c",          "0x7c",          "0x7c", /**/"",    "",    "",  NULL,/***/
      "Super_L",       "Super_L",       "Super_L", /**/"",    "",    "",  NULL,/***/
      "Super_R",       "Super_R",       "Super_R", /**/"",    "",    "",  NULL,/***/
         "0x7f",          "0x7f",          "0x7f", /**/"",    "",    "",  NULL, /***/
};
