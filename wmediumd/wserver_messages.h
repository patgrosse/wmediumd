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

typedef uint8_t u8;

typedef struct {
    u8 from_addr[ETH_ALEN];
    u8 to_addr[ETH_ALEN];
    u8 snr;
} snr_update_request;

const snr_update_request wserver_close_req = (const snr_update_request) {0};

typedef struct {
    snr_update_request request;
    u8 update_result;
} snr_update_response;

#endif //WMEDIUMD_WSERVER_MESSAGES_H
