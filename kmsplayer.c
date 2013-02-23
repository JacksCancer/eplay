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


static void set_stereo(GstBin* bin)
{
    GstIterator* iter = gst_bin_iterate_recurse(bin);
    gpointer elem;

    while (GST_ITERATOR_OK == gst_iterator_next(iter, &elem))
    {
        GstElementFactory *factory = gst_element_get_factory(GST_ELEMENT(elem));
        if (factory)
        {
            gchar* name = NULL;
            g_object_get(factory, "name", &name, NULL);
            if (name && strcmp(name, "a52dec") == 0)
            {
                //g_object_set(elem, "lfe", true, NULL);
                g_object_set(elem, "drc", true, NULL);
                g_object_set(elem, "mode", 2, NULL);
                printf("%s: setting mode=2\n", name);
                g_free(name);
            }
        }

        g_object_unref(elem);
    }
}

void eplay_switch_audio(struct eplay* ep)
{
    gint naudio = 0;
    gint current = 0;
    g_object_get(ep->playbin, "n-audio", &naudio, NULL);
    g_object_get(ep->playbin, "current-audio", &current, NULL);
    if (naudio > 0)
    {
        current = (current + 1) % naudio;
        printf("audio: %i/%i\n", current, naudio);
        g_object_set(ep->playbin, "current-audio", current, NULL);
    }
}

void eplay_play(struct eplay* ep, const char* file)
{
    gchar *uri;
    GstFormat format = GST_FORMAT_TIME;

    printf("playing '%s'\n", file);

    uri = gst_filename_to_uri(file, NULL);

    gst_element_set_state(ep->playbin, GST_STATE_NULL);
    if (gst_element_get_state(ep->playbin, NULL, NULL, GST_CLOCK_TIME_NONE) == GST_STATE_CHANGE_SUCCESS)
    {
        g_object_set(ep->playbin, "uri", uri, NULL);

        gst_element_set_state(ep->playbin, GST_STATE_PAUSED);

        if (gst_element_get_state(ep->playbin, NULL, NULL, GST_CLOCK_TIME_NONE) == GST_STATE_CHANGE_SUCCESS)
            set_stereo(GST_BIN(ep->playbin));

        gst_element_set_state(ep->playbin, GST_STATE_PLAYING);
        if (gst_element_get_state(ep->playbin, NULL, NULL, GST_CLOCK_TIME_NONE) == GST_STATE_CHANGE_SUCCESS)
        {
            if (!gst_element_query_duration(ep->playbin, &format, &ep->duration))
            {
                ep->duration = 0;
                printf("failed to query duration\n");
            }
            else
            {
                printf("duration: %llu\n", ep->duration);
            }
        }
    }
    
    g_free(uri);
}

static void stopped(void *data)
{
    struct eplay* ep = (struct eplay*) data;
    eplay_show_overlay(ep);
    printf("Stopped\n");
}

static GstBusSyncReply bus_call(GstBus * bus, GstMessage * msg, gpointer data)
{
    struct eplay* ep = (struct eplay*) data;

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_EOS:
            printf("End-of-stream\n");
            ecore_main_loop_thread_safe_call_async(stopped, ep);
            break;
        case GST_MESSAGE_ERROR:
        {
            gchar *debug;
            GError *err;

            gst_message_parse_error(msg, &err, &debug);
            g_free(debug);

            printf("Error: %s\n", err->message);
            g_error_free(err);

            break;
        }
        default:
            //printf("type: %i\n", GST_MESSAGE_TYPE(msg));
            break;
    }

    gst_message_unref(msg);

    return GST_BUS_DROP;
}

static gint64 get_position(struct eplay* ep)
{
    gint64 value = 0;
    GstFormat format = GST_FORMAT_TIME;   
    if (!gst_element_query_position(ep->playbin, &format, &value))
    {
        printf("failed to query position\n");
    }
    return value;
}


double eplay_seek(struct eplay* ep, int offset)
{
    
    double progress = 0.0;

    gst_element_set_state(ep->playbin, GST_STATE_PAUSED);
    if (gst_element_get_state(ep->playbin, NULL, NULL, GST_CLOCK_TIME_NONE) == GST_STATE_CHANGE_SUCCESS)
    {
        GstFormat format = GST_FORMAT_TIME;
        gint64 value = get_position(ep);
        if (ep->duration)
        {
            progress = (double)value / ep->duration;
        }
        printf("pos: %llu, %f\n", value, progress);

        if (gst_element_seek_simple(ep->playbin, format, (GstSeekFlags) (GST_SEEK_FLAG_SEGMENT | GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_FLUSH), value + GST_SECOND * offset))
        {
            printf("failed to seek\n");
        }

        //gst_element_set_state(ep->playbin, GST_STATE_PLAYING);

        // seek_event = gst_event_new_seek (1.0, GST_FORMAT_TIME,
//                 (GstSeekFlags) (GST_SEEK_FLAG_SEGMENT | GST_SEEK_FLAG_FLUSH),
//                 GST_SEEK_TYPE_SET, start_time,
//                 GST_SEEK_TYPE_NONE, stop_time);
    }
    return progress;
}

bool eplay_is_playing(struct eplay* ep)
{
    GstState state = GST_STATE_NULL;
    if (!gst_element_get_state(ep->playbin, &state, NULL, GST_CLOCK_TIME_NONE) == GST_STATE_CHANGE_SUCCESS)
        printf("failed to get state\n");

    return state == GST_STATE_PLAYING;
}


void eplay_set_playing(struct eplay* ep, bool playing)
{
    if (!gst_element_set_state(ep->playbin, playing ? GST_STATE_PLAYING : GST_STATE_PAUSED))
        printf("failed to set state\n");
}

double eplay_get_progress(struct eplay* ep)
{
    double progress = 0.0;
    gint64 value = get_position(ep);
    if (ep->duration)
        progress = (double)value / ep->duration;
    return progress;
}

bool eplay_setup_gstreamer(struct eplay* ep)
{
    GstBus *bus;
    GstElement *kmssink;

    gst_init(NULL, NULL);

    ep->playbin = gst_element_factory_make("playbin2", NULL);
    if (!ep->playbin) {
        printf("'playbin2' gstreamer plugin missing\n");
        return false;
    }

    kmssink = gst_element_factory_make("kmssink", NULL);
    if (!kmssink) {
        printf("'kmssink' gstreamer plugin missing\n");
        return false;
    }

    g_object_set(ep->playbin, "video-sink", kmssink, NULL);
    g_object_set(kmssink, "scale", 1, NULL);
    g_object_set(kmssink, "crtc-id", ep->crtc, NULL);
    g_object_set(kmssink, "plane-id", ep->planes[0], NULL);

    bus = gst_element_get_bus(ep->playbin);
    gst_bus_set_sync_handler(bus, bus_call, ep);
    g_object_unref(bus);

    //ecore_main_loop_glib_integrate();
    return true;
}

void eplay_cleanup_gstreamer(struct eplay *ep)
{
    gst_element_set_state(ep->playbin, GST_STATE_NULL);
    g_object_unref(ep->playbin);
    gst_deinit();
}
