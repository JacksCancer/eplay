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

bool eplay_setup_mixer(struct eplay* ep)
{
    const char* card = "default";
    const char* selem_name = "SDT DL";
    const char* cp;
    int ret = 0;
    snd_mixer_selem_id_t *sid;

    if ((cp = "mixer open", ret = snd_mixer_open(&ep->mixer, 0)) ||
        (cp = "mixer attach", ret = snd_mixer_attach(ep->mixer, card)) ||
        (cp = "elem register", ret = snd_mixer_selem_register(ep->mixer, NULL, NULL)) ||
        (cp = "mixer load", ret = snd_mixer_load(ep->mixer)))
    {
        fprintf(stderr, "error at %s: %s\n", cp, snd_strerror(ret));
        return false;
    }

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, selem_name);

    ep->mixer_elem = snd_mixer_find_selem(ep->mixer, sid);

    if (ep->mixer_elem == NULL)
    {
        fprintf(stderr, "could not find %s\n", selem_name);
        return false;
    }

    snd_mixer_selem_get_playback_volume_range(ep->mixer_elem, &ep->mixer_elem_min, &ep->mixer_elem_max);    

    return true;
}

void eplay_cleanup_mixer(struct eplay*ep)
{
    snd_mixer_close(ep->mixer);
}

long eplay_get_volume(struct eplay *ep)
{
    long vol = 0;
    int ret = snd_mixer_selem_get_playback_volume(ep->mixer_elem, SND_MIXER_SCHN_MONO, &vol);
    if (ret) fprintf(stderr, "failed to get volume %s\n", snd_strerror(ret));
    return vol;
}

void eplay_set_volume(struct eplay *ep, long vol)
{
    int ret = snd_mixer_selem_set_playback_volume_all(ep->mixer_elem, vol);
    if (ret) fprintf(stderr, "failed to set volume %s\n", snd_strerror(ret));
}
