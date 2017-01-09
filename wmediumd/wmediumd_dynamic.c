/*
 *	wmediumd_server - server for on-the-fly modifications for wmediumd
 *	Copyright (c) 2016, Patrick Grosse <patrick.grosse@uni-muenster.de>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version 2
 *	of the License, or (at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *	02110-1301, USA.
 */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "wmediumd_dynamic.h"

pthread_mutex_t snr_lock = PTHREAD_MUTEX_INITIALIZER;

int switch_matrix(int **matrix_loc, const int oldsize, const int newsize, int **backup) {
    if (backup) {
        *backup = malloc(sizeof(int) * oldsize * oldsize);
        if (!*backup) {
            return -ENOMEM;
        }
        memcpy(*backup, *matrix_loc, sizeof(int) * oldsize * oldsize);
    }
    free(*matrix_loc);
    *matrix_loc = malloc(sizeof(int) * newsize * newsize);
    if (!*matrix_loc) {
        return -ENOMEM;
    }
    return 0;
}

int add_station(struct wmediumd *ctx, const u8 addr[]) {
    struct station *sta_loop;
    list_for_each_entry(sta_loop, &ctx->stations, list) {
        if (memcmp(sta_loop->addr, addr, ETH_ALEN) == 0)
            return -EEXIST;
    }

    pthread_mutex_lock(&snr_lock);
    int oldnum = ctx->num_stas;
    int newnum = oldnum + 1;

    // Save old matrix and init new matrix
    int *oldmatrix;
    int ret;
    if ((ret = switch_matrix(&ctx->snr_matrix, oldnum, newnum, &oldmatrix))) {
        goto out;
    }

    // Copy old matrix
    for (int x = 0; x < oldnum; x++) {
        for (int y = 0; y < oldnum; y++) {
            ctx->snr_matrix[x * newnum + y] = oldmatrix[x * oldnum + y];
        }
    }

    // Fill last lines with null
    for (int x = 0; x < newnum; x++) {
        ctx->snr_matrix[x * newnum + oldnum] = 0;
    }
    for (int y = 0; y < newnum; y++) {
        ctx->snr_matrix[oldnum * newnum + y] = 0;
    }

    // Init new station object
    struct station *station;
    station = malloc(sizeof(*station));
    if (!station) {
        ret = -ENOMEM;
        goto out;
    }
    station->index = oldnum;
    memcpy(station->addr, addr, ETH_ALEN);
    memcpy(station->hwaddr, addr, ETH_ALEN);
    station_init_queues(station);
    list_add_tail(&station->list, &ctx->stations);
    ctx->num_stas = newnum;
    ret = station->index;

    out:
    pthread_mutex_unlock(&snr_lock);
    return ret;
}

int del_station(struct wmediumd *ctx, struct station *station) {
    if (ctx->num_stas == 0) {
        return -ENXIO;
    }
    int oldnum = ctx->num_stas;
    int newnum = oldnum - 1;

    // Save old matrix and init new matrix
    int *oldmatrix;
    int ret;
    if ((ret = switch_matrix(&ctx->snr_matrix, oldnum, newnum, &oldmatrix))) {
        return ret;
    }

    int index = station->index;

    // Decreasing index of stations following deleted station
    struct station *sta_loop = station;
    list_for_each_entry_from(sta_loop, &ctx->stations, list) {
        sta_loop->index = sta_loop->index - 1;
    }

    // Copy all values not related to deleted station
    int xnew = 0;
    for (int x = 0; x < oldnum; x++) {
        if (x == index) {
            continue;
        }
        int ynew = 0;
        for (int y = 0; y < oldnum; y++) {
            if (y == index) {
                continue;
            }
            ctx->snr_matrix[xnew * newnum + ynew] = oldmatrix[x * oldnum + y];
            ynew++;
        }
        xnew++;
    }
    free(oldmatrix);

    list_del(&station->list);
    ctx->num_stas = newnum;

    free(station);
    return 0;
}

int del_station_by_id(struct wmediumd *ctx, const int id) {
    pthread_mutex_lock(&snr_lock);
    int ret;
    struct station *station;
    list_for_each_entry(station, &ctx->stations, list) {
        if (station->index == id) {
            ret = del_station(ctx, station);
            goto out;
        }
    }

    out:
    ret = -ENODEV;
    pthread_mutex_unlock(&snr_lock);
    return ret;
}

int del_station_by_mac(struct wmediumd *ctx, const u8 *addr) {
    pthread_mutex_lock(&snr_lock);
    int ret;
    struct station *station;
    list_for_each_entry(station, &ctx->stations, list) {
        if (memcmp(addr, station->addr, ETH_ALEN) == 0) {
            ret = del_station(ctx, station);
            goto out;
        }
    }
    ret = -ENODEV;
    
    out:
    pthread_mutex_unlock(&snr_lock);
    return ret;
}
