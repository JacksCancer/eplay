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


#include <sys/stat.h>
#include <sys/mount.h>

#include <libudev.h>


static void mount_disk(struct eplay* ep, const char* devnode, const char* name)
{
    char buf[512];
    char path[256];
    snprintf(path, sizeof(path), "/media/%s", name);
    snprintf(buf, sizeof(buf), "mount -o ro %s %s", devnode, path);
    mkdir(path, 0755);
    int ret = system(buf);

    if (ret)
        fprintf(stderr, "mount failed (%s): %i\n", buf, WEXITSTATUS(ret));
    else
    {
        ep->mount_list = eina_list_append(ep->mount_list, strdup(name));
        fprintf(stderr, "mounted %s\n", devnode);
        eplay_refresh_browser(ep);
    }
}

static void umount_disk(const char* name)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "umount /media/%s", name);
    int ret = system(buf);
    if (ret)
        fprintf(stderr, "umount failed (%s): %i\n", buf, WEXITSTATUS(ret));
}

static Eina_Bool handle_partition_event(void *data, Ecore_Fd_Handler *handler)
{
    struct eplay* ep = data;

    if (ecore_main_fd_handler_active_get(handler, ECORE_FD_ERROR))
    {
        printf("An error has occurred. Stop watching this fd.\n");
        return ECORE_CALLBACK_CANCEL;
    }

    struct udev_device *dev = udev_monitor_receive_device(ep->disk_monitor);

    if (dev)
    {
        const char* action = udev_device_get_action(dev);
        const char* devnode = udev_device_get_devnode(dev);
        const char* name = udev_device_get_sysname(dev);
        printf("%s %s\n", action, name);

        if (devnode && action)
        {
            if (strcmp("add", action) == 0)
            {
                mount_disk(ep, devnode, name);
            }
            else if (strcmp("remove", action) == 0)
            {
                //umount_disk(devnode);
            }
            else if (strcmp("change", action) == 0)
            {
                //umount_disk(devnode);
                //mount_disk(devnode);
            }
        }
        udev_device_unref(dev);    
    }

    return ECORE_CALLBACK_RENEW;
}


bool eplay_setup_udev(struct eplay* ep)
{
    ep->udev = udev_new();
    ep->disk_monitor = udev_monitor_new_from_netlink(ep->udev, "kernel");

    udev_monitor_filter_add_match_subsystem_devtype(ep->disk_monitor, "block", "partition");

    udev_monitor_enable_receiving(ep->disk_monitor);

    struct udev_enumerate* udev_enum = udev_enumerate_new(ep->udev);
    if (udev_enum)
    {
        struct udev_list_entry *udev_entry;
        printf("scanning disks\n");

        udev_enumerate_add_match_subsystem(udev_enum, "block");
        udev_enumerate_scan_devices(udev_enum);

        udev_list_entry_foreach(udev_entry, udev_enumerate_get_list_entry(udev_enum))
        {
            const char* path = udev_list_entry_get_name(udev_entry);
            struct udev_device *dev = udev_device_new_from_syspath(ep->udev, path);

            if (dev)
            {
                const char* devnode = udev_device_get_devnode(dev);
                const char* name = udev_device_get_sysname(dev);
                const char* type = udev_device_get_devtype(dev);
                if (devnode && name && type && strcmp(type, "partition") == 0)
                {
                    mount_disk(ep, devnode, name);
                }
                udev_device_unref(dev);
            }
            else perror(path);
        }

        udev_enumerate_unref(udev_enum);
    }

    ep->udev_fd = udev_monitor_get_fd(ep->disk_monitor);

    if (ep->udev_fd >= 0)
        ep->udev_handler = ecore_main_fd_handler_add(ep->udev_fd, ECORE_FD_READ | ECORE_FD_ERROR, handle_partition_event, ep, NULL, NULL);


    return ep->disk_monitor && ep->udev_handler;
}

void eplay_cleanup_udev(struct eplay* ep)
{
    if (ep->udev_fd >= 0)
        close(ep->udev_fd);
    
    if (ep->udev_handler)
        ecore_main_fd_handler_del(ep->udev_handler);

    udev_monitor_unref(ep->disk_monitor);
    udev_unref(ep->udev);

    char* mount_name;
    Eina_List *l;

    EINA_LIST_FOREACH(ep->mount_list, l, mount_name)
    {
        umount_disk(mount_name);
        free(mount_name);
    }
}
