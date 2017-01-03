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

#ifndef WMEDIUMD_WSERVER_MESSAGES_H
#define WMEDIUMD_WSERVER_MESSAGES_H

#include <stdint.h>
#include <unistd.h>
#include "ieee80211.h"

#define WACTION_CONTINUE 0 /* Operation successful, continue */
#define WACTION_ERROR 1 /* Error occured, disconnect client */
#define WACTION_DISCONNECTED 2 /* Client has disconnected */
#define WACTION_CLOSE 3 /* Close the server */

#define WUPDATE_SUCCESS 0 /* update of SNR successful */
#define WUPDATE_INTF_NOTFOUND 1 /* unknown interface */

/* Socket location following FHS guidelines:
 * http://www.pathname.com/fhs/pub/fhs-2.3.html#PURPOSE46 */
#define WSERVER_SOCKET_PATH "/var/run/wmediumd.sock"

#define WSERVER_SHUTDOWN_REQUEST_TYPE 0
#define WSERVER_UPDATE_REQUEST_TYPE 1
#define WSERVER_UPDATE_RESPONSE_TYPE 2
#define WSERVER_DEL_BY_MAC_REQUEST_TYPE 3
#define WSERVER_DEL_BY_MAC_RESPONSE_TYPE 4
#define WSERVER_DEL_BY_ID_REQUEST_TYPE 5
#define WSERVER_DEL_BY_ID_RESPONSE_TYPE 6
#define WSERVER_ADD_REQUEST_TYPE 7
#define WSERVER_ADD_RESPONSE_TYPE 8

typedef uint8_t u8;

typedef struct {
    u8 type;
} wserver_msg;

typedef struct {
    wserver_msg base;
    u8 from_addr[ETH_ALEN];
    u8 to_addr[ETH_ALEN];
    u8 snr;
} snr_update_request;

typedef struct {
    wserver_msg base;
    snr_update_request request;
    u8 update_result;
} snr_update_response;

typedef struct {
    wserver_msg base;
    u8 addr[ETH_ALEN];
} station_del_by_mac_request;

typedef struct {
    wserver_msg base;
    station_del_by_mac_request request;
    u8 update_result;
} station_del_by_mac_response;

typedef struct {
    wserver_msg base;
    int id;
} station_del_by_id_request;

typedef struct {
    wserver_msg base;
    station_del_by_id_request request;
    u8 update_result;
} station_del_by_id_response;

typedef struct {
    wserver_msg base;
    u8 addr[ETH_ALEN];
} station_add_request;

typedef struct {
    wserver_msg base;
    station_add_request request;
    u8 update_result;
} station_add_response;

/**
 * Send bytes over a socket, repeat until all bytes are sent
 * @param sock The socket file descriptor
 * @param buf The pointer to the bytes
 * @param len The amount of bytes to send
 * @param shift The amount of bytes that should be skipped in the buffer
 * @param flags Flags for the send method
 * @return 0 on success, -1 on error, -2 on client disconnect
 */
int sendfull(int sock, const void *buf, size_t len, size_t shift, int flags);

/**
 * Receive bytes from a socket, repeat until all bytes are read
 * @param sock The socket file descriptor
 * @param buf A pointer where to store the received bytes
 * @param len The amount of bytes to receive
 * @param shift The amount of bytes that should be skipped in the buffer
 * @param flags Flags for the recv method
 * @return 0 on success, -1 on error, -2 on client disconnect
 */
int recvfull(int sock, void *buf, size_t len, size_t shift, int flags);

/**
 * Receive the wserver_msg from a socket
 * @param sock_fd The socket file descriptor
 * @param base Where to store the wserver_msg
 * @return A positive WACTION_* constant, or a negative errno value
 */
int wserver_recv_msg_base(int sock_fd, wserver_msg *base);

/**
 * Receive the rest from a wserver msg from a socket
 * @param sock_fd The socket file descriptor
 * @param buf Where to store the msg
 * @param type The WSERVER_*_TYPE
 * @return A positive WACTION_* constant, or a negative errno value
 */
int wserver_recv_msg_rest(int sock_fd, void *buf, u8 type);

/**
 * Send a response to a client
 * @param sock_fd The request_ctx context
 * @param buf The response to send
 * @param type The response type
 * @return 0 on success
 */
int wserver_send_msg(int sock_fd, const void *buf, u8 type);

/**
 * Get the size of a request/response based on its type
 * @param type The WSERVER_*_TYPE
 * @return The size or -1 if not found
 */
ssize_t get_msg_size_by_type(u8 type);

#endif //WMEDIUMD_WSERVER_MESSAGES_H
