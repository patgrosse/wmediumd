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

#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

#include "wserver.h"
#include "wmediumd_dynamic.h"

/*
 * Macro for unused parameters
 */
#define UNUSED(x) (void)(x)


#define LOG_PREFIX "W_SRV: "
#define LOG_INDENT "    "

/**
 * Global listen socket
 */
int listen_soc;

/**
 * Global thread
 */
pthread_t server_thread;
/**
 * When set to false, the server is shut down
 */
bool wserver_run_bit = true;

/**
 * Stores the old sig handler for SIGINT
 */
__sighandler_t old_sig_handler;

/**
 * Shutdown the server
 */
void shutdown_wserver() {
    signal(SIGINT, old_sig_handler);
    printf("\n" LOG_PREFIX "shutting down wserver\n");
    close(listen_soc);
    unlink(WSERVER_SOCKET_PATH);
    wserver_run_bit = true;
}

/**
 * Handle the SIGINT signal
 * @param param The param passed to by signal()
 */
void handle_sigint(int param) {
    UNUSED(param);
    shutdown_wserver();
    exit(EXIT_SUCCESS);
}

/**
 * Create the listening socket
 * @return The FD of the socket
 */
int create_listen_socket() {
    int soc = socket(AF_UNIX, SOCK_STREAM, 0);
    if (soc < 0) {
        perror(LOG_PREFIX "Socket not created");
        return -1;
    }
    printf(LOG_PREFIX "Socket created\n");

    int unlnk_ret = unlink(WSERVER_SOCKET_PATH);
    if (unlnk_ret != 0 && errno != ENOENT) {
        perror(LOG_PREFIX "Cannot remove old UNIX socket at '" WSERVER_SOCKET_PATH "'");
        close(soc);
        return -1;
    }
    struct sockaddr_un saddr = {AF_UNIX, WSERVER_SOCKET_PATH};
    int retval = bind(soc, (struct sockaddr *) &saddr, sizeof(saddr));
    if (retval < 0) {
        perror(LOG_PREFIX "Bind failed");
        close(soc);
        return -1;
    }
    printf(LOG_PREFIX "Bound to UNIX socket '" WSERVER_SOCKET_PATH "'\n");

    retval = listen(soc, 1);
    if (retval < 0) {
        perror(LOG_PREFIX "Listen failed");
        close(soc);
        return -1;
    }
    printf(LOG_PREFIX "Listening for incoming connection\n");

    return soc;
}

/**
 * Accept incoming connections
 * @param listen_soc The FD of the server socket
 * @return The FD of the client socket
 */
int accept_connection(int listen_soc) {
    struct sockaddr_in claddr;
    socklen_t claddr_size = sizeof(claddr);

    int soc = accept(listen_soc, (struct sockaddr *) &claddr, &claddr_size);
    if (soc < 0) {
        return -1;
    }
    return soc;
}

int handle_update_request(struct request_ctx *ctx, const snr_update_request *request) {
    snr_update_response response;
    response.request = *request;

    struct station *sender = NULL;
    struct station *receiver = NULL;
    struct station *station;

    pthread_mutex_lock(&snr_lock);

    list_for_each_entry(station, &ctx->ctx->stations, list) {
        if (memcmp(&request->from_addr, station->addr, ETH_ALEN) == 0) {
            sender = station;
        }
        if (memcmp(&request->to_addr, station->addr, ETH_ALEN) == 0) {
            receiver = station;
        }
    }

    if (!sender || !receiver) {
        printf(LOG_PREFIX LOG_INDENT "Could not perform update: from=" MAC_FMT ", to=" MAC_FMT ", snr=%d\n",
               MAC_ARGS(request->from_addr), MAC_ARGS(request->to_addr), request->snr);
        printf(LOG_PREFIX LOG_INDENT "Interface(s) not found\n");
        response.update_result = WUPDATE_INTF_NOTFOUND;
    } else {
        printf(LOG_PREFIX LOG_INDENT "Performing update: from=" MAC_FMT ", to=" MAC_FMT ", snr=%d\n",
               MAC_ARGS(sender->addr), MAC_ARGS(receiver->addr), request->snr);
        ctx->ctx->snr_matrix[sender->index * ctx->ctx->num_stas + receiver->index] = request->snr;
        response.update_result = WUPDATE_SUCCESS;
    }
    pthread_mutex_unlock(&snr_lock);
    int ret = wserver_send_msg(ctx->sock_fd, &response, WSERVER_UPDATE_RESPONSE_TYPE);
    if (ret < 0) {
        w_logf(ctx->ctx, LOG_ERR, "Error on update response: %s", strerror(abs(ret)));
        return WACTION_ERROR;
    }
    return ret;
}

int handle_delete_by_id_request(struct request_ctx *ctx, station_del_by_id_request *request) {
    station_del_by_id_response response;
    response.request = *request;
    int ret = del_station_by_id(ctx->ctx, request->id);
    if (ret) {
        if (ret == -ENODEV) {
            response.update_result = WUPDATE_INTF_NOTFOUND;
        } else {
            w_logf(ctx->ctx, LOG_ERR, "Error on delete by id request: %s", strerror(abs(ret)));
            return WACTION_ERROR;
        }
    } else {
        response.update_result = WUPDATE_SUCCESS;
    }
    ret = wserver_send_msg(ctx->sock_fd, &response, WSERVER_DEL_BY_ID_RESPONSE_TYPE);
    if (ret < 0) {
        w_logf(ctx->ctx, LOG_ERR, "Error on delete by id response: %s", strerror(abs(ret)));
        return WACTION_ERROR;
    }
    return ret;
}

int handle_delete_by_mac_request(struct request_ctx *ctx, station_del_by_mac_request *request) {
    station_del_by_mac_response response;
    response.request = *request;
    int ret = del_station_by_mac(ctx->ctx, request->addr);
    if (ret) {
        if (ret == -ENODEV) {
            response.update_result = WUPDATE_INTF_NOTFOUND;
        } else {
            w_logf(ctx->ctx, LOG_ERR, "Error %d on delete by mac request: %s", ret, strerror(abs(ret)));
            return WACTION_ERROR;
        }
    } else {
        response.update_result = WUPDATE_SUCCESS;
    }
    ret = wserver_send_msg(ctx->sock_fd, &response, WSERVER_DEL_BY_MAC_RESPONSE_TYPE);
    if (ret < 0) {
        w_logf(ctx->ctx, LOG_ERR, "Error on delete by mac response: %s", strerror(abs(ret)));
        return WACTION_ERROR;
    }
    return ret;
}

int handle_add_request(struct request_ctx *ctx, station_add_request *request) {
    int ret = add_station(ctx->ctx, request->addr);
    if (ret < 0) {
        w_logf(ctx->ctx, LOG_ERR, "Error on add request: %s", strerror(abs(ret)));
        return WACTION_ERROR;
    }
    w_logf(ctx->ctx, LOG_NOTICE, LOG_PREFIX
            "Added station with MAC " MAC_FMT " and ID %d\n", MAC_ARGS(request->addr), ret);
    station_add_response response = {
            .request = *request,
            .update_result = WUPDATE_SUCCESS
    };
    ret = wserver_send_msg(ctx->sock_fd, &response, WSERVER_ADD_RESPONSE_TYPE);
    if (ret < 0) {
        w_logf(ctx->ctx, LOG_ERR, "Error on add response: %s", strerror(abs(ret)));
        return WACTION_ERROR;
    }
    return ret;
}

int parse_recv_msg_rest_error(struct wmediumd *ctx, int value) {
    if (value > 0) {
        return value;
    } else {
        w_logf(ctx, LOG_ERR, "Error on receive msg rest: %s", strerror(abs(value)));
        return WACTION_ERROR;
    }
}

int receive_handle_request(struct request_ctx *ctx) {
    wserver_msg base;
    int ret = wserver_recv_msg_base(ctx->sock_fd, &base);
    if (ret > 0) {
        return ret;
    } else if (ret < 0) {
        w_logf(ctx->ctx, LOG_ERR, "Error on receive base request: %s", strerror(abs(ret)));
        return WACTION_ERROR;
    }
    if (base.type == WSERVER_SHUTDOWN_REQUEST_TYPE) {
        return WACTION_CLOSE;
    } else if (base.type == WSERVER_UPDATE_REQUEST_TYPE) {
        snr_update_request request;
        request.base = base;
        if ((ret = wserver_recv_msg_rest(ctx->sock_fd, &request, WSERVER_UPDATE_REQUEST_TYPE))) {
            return parse_recv_msg_rest_error(ctx->ctx, ret);
        } else {
            return handle_update_request(ctx, &request);
        }
    } else if (base.type == WSERVER_DEL_BY_MAC_REQUEST_TYPE) {
        station_del_by_mac_request request;
        request.base = base;
        if ((ret = wserver_recv_msg_rest(ctx->sock_fd, &request, WSERVER_DEL_BY_MAC_REQUEST_TYPE))) {
            return parse_recv_msg_rest_error(ctx->ctx, ret);
        } else {
            return handle_delete_by_mac_request(ctx, &request);
        }
    } else if (base.type == WSERVER_DEL_BY_ID_REQUEST_TYPE) {
        station_del_by_id_request request;
        request.base = base;
        if ((ret = wserver_recv_msg_rest(ctx->sock_fd, &request, WSERVER_DEL_BY_ID_REQUEST_TYPE))) {
            return parse_recv_msg_rest_error(ctx->ctx, ret);
        } else {
            return handle_delete_by_id_request(ctx, &request);
        }
    } else if (base.type == WSERVER_ADD_REQUEST_TYPE) {
        station_add_request request;
        request.base = base;
        if ((ret = wserver_recv_msg_rest(ctx->sock_fd, &request, WSERVER_ADD_REQUEST_TYPE))) {
            return parse_recv_msg_rest_error(ctx->ctx, ret);
        } else {
            return handle_add_request(ctx, &request);
        }
    } else {
        return -1;
    }
}

/**
 * Run the server using the given wmediumd context
 * @param ctx The wmediumd context
 * @return NULL, required for pthread
 */
void *run_wserver(void *ctx) {
    old_sig_handler = signal(SIGINT, handle_sigint);

    listen_soc = create_listen_socket();
    if (listen_soc < 0) {
        return NULL;
    }
    while (wserver_run_bit) {
        printf(LOG_PREFIX "Waiting for client to connect...\n");
        struct request_ctx rctx;
        rctx.ctx = ctx;
        rctx.sock_fd = accept_connection(listen_soc);
        if (rctx.sock_fd < 0) {
            perror(LOG_PREFIX "Accept failed");
        } else {
            printf(LOG_PREFIX "Client connected\n");
            while (1) {
                int action_resp;
                printf(LOG_PREFIX LOG_INDENT "Waiting for request...\n");
                action_resp = receive_handle_request(&rctx);
                if (action_resp == WACTION_DISCONNECTED) {
                    printf(LOG_PREFIX LOG_INDENT "Client has disconnected\n");
                    break;
                } else if (action_resp == WACTION_ERROR) {
                    printf(LOG_PREFIX LOG_INDENT "Disconnecting client because of error\n");
                    break;
                } else if (action_resp == WACTION_CLOSE) {
                    printf(LOG_PREFIX LOG_INDENT "Closing server\n");
                    wserver_run_bit = false;
                    break;
                }
            }
        }
        close(rctx.sock_fd);
    }
    shutdown_wserver();
    return NULL;
}

int start_wserver(struct wmediumd *ctx) {
    return pthread_create(&server_thread, NULL, run_wserver, ctx);
}

int stop_wserver() {
    int pthread_val = pthread_cancel(server_thread);
    shutdown_wserver();
    return pthread_val;
}
