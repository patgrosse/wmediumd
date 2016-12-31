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

#ifndef WMEDIUMD_SERVER_H
#define WMEDIUMD_SERVER_H

#include "wmediumd.h"

/**
 * Start the server using the given wmediumd context in a background task
 * @param ctx The wmediumd context
 * @return 0 on success
 */
int start_wserver(struct wmediumd *ctx);

/**
 * Stop the server if active
 * @return 0 on success
 */
int stop_wserver();

#endif //WMEDIUMD_SERVER_H
