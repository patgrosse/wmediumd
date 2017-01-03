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

#include <sys/socket.h>
#include <errno.h>
#include "wserver_messages.h"

int sendfull(int sock, const void *buf, size_t len, size_t shift, int flags) {
    size_t total = 0;
    size_t bytesleft = len;
    ssize_t currsent = 0;
    while (total < len) {
        currsent = send(sock, buf + shift + total, bytesleft, flags);
        if (currsent == -1) {
            if (errno == EPIPE || errno == ECONNRESET) {
                return WACTION_DISCONNECTED;
            } else {
                return -errno;
            }
        }
        total += currsent;
        bytesleft -= currsent;
    }
    return WACTION_CONTINUE;
}

int recvfull(int sock, void *buf, size_t len, size_t shift, int flags) {
    size_t total = 0;
    size_t bytesleft = len;
    ssize_t currrecv = 0;
    while (total < len) {
        currrecv = recv(sock, buf + shift + total, bytesleft, flags);
        if (currrecv == -1) {
            if (errno == EPIPE || errno == ECONNRESET) {
                return WACTION_DISCONNECTED;
            } else {
                return -errno;
            }
        } else if (currrecv == 0) {
            return WACTION_DISCONNECTED;
        }
        total += currrecv;
        bytesleft -= currrecv;
    }
    return WACTION_CONTINUE;
}

int wserver_recv_msg_base(int sock_fd, wserver_msg *base) {
    return recvfull(sock_fd, base, sizeof(wserver_msg), 0, 0);
}

int wserver_recv_msg_rest(int sock_fd, void *buf, u8 type) {
    ssize_t size = get_msg_size_by_type(type);
    if (size < 0) {
        return -EINVAL;
    }
    return recvfull(sock_fd, buf, (size_t) (size - sizeof(wserver_msg)), sizeof(wserver_msg), 0);
}

int wserver_send_msg(int sock_fd, const void *buf, u8 type) {
    ssize_t size = get_msg_size_by_type(type);
    if (size < 0) {
        return -EINVAL;
    }
    *((u8 *) buf) = type;
    return sendfull(sock_fd, buf, (size_t) size, 0, MSG_NOSIGNAL);
}

ssize_t get_msg_size_by_type(u8 type) {
    switch (type) {
        case WSERVER_SHUTDOWN_REQUEST_TYPE:
            return sizeof(wserver_msg);
        case WSERVER_UPDATE_REQUEST_TYPE:
            return sizeof(snr_update_request);
        case WSERVER_UPDATE_RESPONSE_TYPE:
            return sizeof(snr_update_response);
        case WSERVER_DEL_BY_MAC_REQUEST_TYPE:
            return sizeof(station_del_by_mac_request);
        case WSERVER_DEL_BY_MAC_RESPONSE_TYPE:
            return sizeof(station_del_by_mac_response);
        case WSERVER_DEL_BY_ID_REQUEST_TYPE:
            return sizeof(station_del_by_id_request);
        case WSERVER_DEL_BY_ID_RESPONSE_TYPE:
            return sizeof(station_del_by_id_response);
        case WSERVER_ADD_REQUEST_TYPE:
            return sizeof(station_add_request);
        case WSERVER_ADD_RESPONSE_TYPE:
            return sizeof(station_add_response);
        default:
            return -1;
    }
}
