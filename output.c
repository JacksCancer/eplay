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
#include <libudev.h>
#include <omap_drmif.h>
#include <stdint.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <dce.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>


static bool 
create_drm_buffer(struct omap_device* dev, int fd, struct drm_buffer* buf, uint32_t width, uint32_t height)
{
    bool result = false;
    buf->bo = omap_bo_new(dev, width * height * 4, OMAP_BO_WC);
    if (buf->bo)
    {
        uint32_t fourcc = DRM_FORMAT_ARGB8888;
        uint32_t handles[4] = { omap_bo_handle(buf->bo) };
        uint32_t pitches[4] = { width * 4 };
        uint32_t offsets[4] = { 0 };
        result = 0 == drmModeAddFB2(fd, width, height, fourcc, handles, pitches, offsets, &buf->fb_id, 0);

        if (! result)
            fprintf(stderr, "drmModeAddFB2 failed: %s\n", strerror(errno));

        buf->width = width;
        buf->height = height;
        buf->data = omap_bo_map(buf->bo);
    }
    return result;
}

static void
destroy_drm_buffer(int fd, struct drm_buffer* buf)
{
    if (buf->bo)
    {
        omap_bo_del(buf->bo);
        drmModeRmFB(fd, buf->fb_id);
    }
}

void* eplay_switch_overlay_buffer(void *data, void *dest_buffer)
{
    struct eplay* ep = data;
    ep->current_ov_buffer = dest_buffer == ep->overlay[0].data ? 0 : 1;
    printf("switch %i\n", ep->current_ov_buffer);
    if (ep->show_overlay)
        eplay_show_overlay(ep);
    return ep->overlay[ep->current_ov_buffer^1].data;
}

static bool
match_device(struct udev_device* dev, const char* devpath, const char* modalias)
{
    bool found = false;
    if (modalias)
    {
        const char* prop = udev_device_get_property_value(dev, "MODALIAS");
        printf("modalias: %s\n", prop);
        if (prop && strstr(prop, modalias))
            found = true;
    }

    if (devpath)
    {
        const char* path = udev_device_get_devpath(dev);        
        printf("devpath: %s\n", path);
        if (path && strstr(path, devpath))
            found = true;
    }
    return found;
}

static bool
wait_for_device(struct udev* udev, const char* subsystem, const char* devpath, const char* modalias)
{
    bool found = false;
    struct udev_monitor *mon = udev_monitor_new_from_netlink(udev, "kernel");

    if (mon)
    {
        udev_monitor_filter_add_match_subsystem_devtype(mon, subsystem, NULL);
        udev_monitor_enable_receiving(mon);

        struct udev_device * dev = NULL;

        struct udev_enumerate* udev_enum = udev_enumerate_new(udev);
        if (udev_enum)
        {
            struct udev_list_entry *udev_entry;
            printf("scanning\n");

            udev_enumerate_add_match_subsystem(udev_enum, subsystem);
            udev_enumerate_scan_devices(udev_enum);

            udev_list_entry_foreach(udev_entry, udev_enumerate_get_list_entry(udev_enum))
            {
                const char* path = udev_list_entry_get_name(udev_entry);
                printf("syspath: %s\n", path);
                dev = udev_device_new_from_syspath(udev, path);

                if (dev)
                {
                    found = match_device(dev, devpath, modalias);
                    udev_device_unref(dev);
                    if (found) break;
                }
                else perror(path);
            }

            udev_enumerate_unref(udev_enum);
        }

        if (!found)
        {
            printf("watching\n");

            struct pollfd pfd = {
                .fd = udev_monitor_get_fd(mon),
                .events = POLLIN,
            };

            while (!found && (poll(&pfd, 1, -1) > 0))
            {
                dev = udev_monitor_receive_device(mon);
                if (dev)
                {
                    const char* action = udev_device_get_action(dev);
                    printf("action: %s\n", action);
                    if (action && strcmp(action, "add") == 0)
                        found = match_device(dev, devpath, modalias);

                    udev_device_unref(dev);
                }
            }

            if (!found)
                perror("monitor");
            else // FIXME: waiting for rpmsg dce doesn't seem to be sufficient, wait a bit
                sleep(1);
        }
            
        udev_monitor_unref(mon);
    }
    else perror("udev_monitor_new_from_netlink");
    return found;
}

static bool
wait_for_dce(void)
{
    struct udev *udev = udev_new();
    bool result = false;
    if (udev)
    {
        result = wait_for_device(udev, "rpmsg", NULL, "dce");
        //result = wait_for_device(udev, "drivers", "omapdce", NULL) && result;

        udev_unref(udev);
    }

    return result;
}

bool
eplay_setup_drm(struct eplay* ep)
{
    drmModeRes *resources;
    drmModePlaneRes *plane_resources;
    drmModeConnector *connector;
    drmModeModeInfo *mode = NULL;
    int fd;
    int i;

    wait_for_dce();

    ep->dev = dce_init();

    if (!ep->dev)
    {
        fprintf(stderr, "failed to setup omap_device\n");
        return false;
    }

    ep->drm_fd = fd = dce_get_fd();

    if (fd < 0)
    {
        fprintf(stderr, "invalid fd\n");
        return false;
    }

    resources = drmModeGetResources(fd);

    if (! resources)
    {
        fprintf(stderr, "drmModeGetResources failed: %s\n", strerror(errno));
        return false;
    }

    plane_resources = drmModeGetPlaneResources(fd);
    if (! plane_resources)
    {
        fprintf(stderr, "drmModeGetPlaneResources failed: %s\n", strerror(errno));
        return false;
    }

    for (i = 0; i < resources->count_connectors; i++)
    {
        drmModeEncoder *encoder;
        int crtc_index;
        int nplanes = 0;

        ep->c_id = resources->connectors[i];

        connector = drmModeGetConnector(fd, ep->c_id);

        if (! connector)
            continue;

        encoder = drmModeGetEncoder(fd, connector->encoder_id);
        if (encoder)
        {
            nplanes = 0;

            ep->crtc = encoder->crtc_id;

            drmModeFreeEncoder(encoder);

            for (crtc_index = 0; i < resources->count_crtcs; ++crtc_index)
                if (ep->crtc == resources->crtcs[crtc_index])
                  break;

            for (i = 0; nplanes < 2 && i < (int)plane_resources->count_planes; i++)
            {
                drmModePlane *p = drmModeGetPlane(fd, plane_resources->planes[i]);
                if (p)
                {
                    fprintf(stderr, "id: %u, fb: %u, possible crtcs: %x\n", p->plane_id, p->fb_id, p->possible_crtcs);
                    if (p->possible_crtcs & (1 << crtc_index))
                    {
                        ep->planes[nplanes] = p->plane_id;
                        ++nplanes;
                    }
                    
                    drmModeFreePlane(p);
                }
            }
        }

        if (nplanes == 2)
            break;
        else
        {
            drmModeFreeConnector(connector);
            connector = NULL;
        }
    }

    if (!connector)
    {
        fprintf(stderr, "no suitable connector found\n");
        return false;
    }

    for (i = 0; i < connector->count_modes; i++)
    {
        drmModeModeInfo *m = &connector->modes[i];

        fprintf(stderr, "mode: %ux%u (%s)\n", m->hdisplay, m->vdisplay, m->name);

        if (mode == NULL || m->hdisplay > mode->hdisplay)
            mode = m;
    }

    if (mode && create_drm_buffer(ep->dev, fd, &ep->bg, mode->hdisplay, mode->vdisplay))
    {
        int ret = drmModeSetCrtc(fd, ep->crtc, ep->bg.fb_id, 0, 0, &ep->c_id, 1, mode);

        if (ret)
        {
            fprintf(stderr, "drmModeSetCrtc failed: %s\n", strerror(errno));
            return false;
        }
    }

    for (i = 0; i < 2; ++i)
        if (!create_drm_buffer(ep->dev, fd, &ep->overlay[i], mode->hdisplay, mode->vdisplay))
            return false;
        
    return eplay_show_overlay(ep);
}

void
eplay_cleanup_drm(struct eplay* ep)
{
    int i;
    for (i = 0; i < 2; ++i)
        destroy_drm_buffer(ep->drm_fd, &ep->overlay[i]);

    destroy_drm_buffer(ep->drm_fd, &ep->bg);

    dce_deinit(ep->dev);
}

bool eplay_show_overlay(struct eplay* ep)
{
    int i = ep->current_ov_buffer;
    int w = ep->overlay[i].width;
    int h = ep->overlay[i].height;

    int ret = drmModeSetPlane(ep->drm_fd, ep->planes[1], ep->crtc, ep->overlay[i].fb_id, 0,
                    0, 0, w, h, 0 << 16, 0 << 16, w << 16, h << 16);
    if (ret)
    {
        fprintf(stderr, "drmModeSetPlane failed: %s\n", strerror(errno));
        return false;
    }
    ep->show_overlay = true;
    return true;
}

void eplay_hide_overlay(struct eplay* ep)
{
    drmModeSetPlane(ep->drm_fd, ep->planes[1], ep->crtc, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    ep->show_overlay = false;
}

