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
#include "wserver_messages.h"

/*
 * Macro for unused parameters
 */
#define UNUSED(x) (void)(x)


#define LOG_PREFIX "W_SRV: "
#define LOG_INDENT "    "

/**
 * Global listen socket
 */
int listenSoc;

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
    close(listenSoc);
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
 * Send bytes over a socket, repeat until all bytes are sent
 * @param sock The socket file descriptor
 * @param buf The pointer to the bytes
 * @param len The amount of bytes to send
 * @param flags Flags for the send method
 * @return 0 on success, -1 on error, -2 on client disconnect
 */
int sendfull(int sock, const void *buf, size_t len, int flags) {
    size_t total = 0;
    size_t bytesleft = len;
    ssize_t currsent = 0;
    while (total < len) {
        currsent = send(sock, buf + total, bytesleft, flags);
        if (currsent == -1) {
            if (errno == EPIPE || errno == ECONNRESET) {
                return -2;
            } else {
                return -1;
            }
        }
        total += currsent;
        bytesleft -= currsent;
    }
    return 0;
}

/**
 * Receive bytes from a socket, repeat until all bytes are read
 * @param sock The socket file descriptor
 * @param buf A pointer where to store the received bytes
 * @param len The amount of bytes to receive
 * @param flags Flags for the recv method
 * @return 0 on success, -1 on error, -2 on client disconnect
 */
int recvfull(int sock, void *buf, size_t len, int flags) {
    size_t total = 0;
    size_t bytesleft = len;
    ssize_t currrecv = 0;
    while (total < len) {
        currrecv = recv(sock, buf + total, bytesleft, flags);
        if (currrecv == -1) {
            if (errno == EPIPE || errno == ECONNRESET) {
                return -2;
            } else {
                return -1;
            }
        } else if (currrecv == 0) {
            return -2;
        }
        total += currrecv;
        bytesleft -= currrecv;
    }
    return 0;
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
 * @param listenSoc The FD of the server socket
 * @return The FD of the client socket
 */
int accept_connection(int listenSoc) {
    struct sockaddr_in claddr;
    socklen_t claddr_size = sizeof(claddr);

    int soc = accept(listenSoc, (struct sockaddr *) &claddr, &claddr_size);
    if (soc < 0) {
        close(listenSoc);
        perror(LOG_PREFIX "Accept failed");
        return -1;
    }

    return soc;
}

/**
 * Check if two requests are equal
 * @param first The first request
 * @param second The second request
 * @return true if equal, false otherwise
 */
bool compare_requests(const snr_update_request *first, const snr_update_request *second) {
    if (memcmp(first->from_addr, second->from_addr, ETH_ALEN) != 0) {
        return false;
    }
    if (memcmp(first->to_addr, second->to_addr, ETH_ALEN) != 0) {
        return false;
    }
    if (first->snr != second->snr) {
        return false;
    }
    return true;
}

/**
 * Receive a snr_update_request from a client
 * @param connectionSoc The FD of the connection socket
 * @param request A pointer for the read results
 * @return An integer WACTION constant to determine what to do next
 */
int receive_update_request(const int connectionSoc, snr_update_request *request) {
    ssize_t bytes_rcvd = recvfull(connectionSoc, (void *) request, sizeof(snr_update_request), 0);
    if (bytes_rcvd < 0) {
        int retval;
        if (bytes_rcvd == -2) {
            retval = WACTION_DISCONNECTED;
        } else {
            perror(LOG_PREFIX LOG_INDENT LOG_INDENT "Error while receiving");
            retval = WACTION_ERROR;
        }
        close(connectionSoc);
        return retval;
    }
    if (compare_requests(request, &wserver_close_req) == true) {
        return WACTION_CLOSE;
    }
    return WACTION_CONTINUE;
}

/**
 * Handle a snr_update_request and pass it to wmediumd
 * @param ctx The wmediumd context
 * @param request The received request
 * @param response A pointer to store the generated response
 */
void handle_update_request(struct wmediumd *ctx, const snr_update_request *request, snr_update_response *response) {
    response->request = *request;

    struct station *sender = NULL;
    struct station *receiver = NULL;
    struct station *station;

    list_for_each_entry(station, &ctx->stations, list) {
        if (memcmp(&request->from_addr, station->addr, ETH_ALEN) == 0) {
            sender = station;
        }
        if (memcmp(&request->to_addr, station->addr, ETH_ALEN) == 0) {
            receiver = station;
        }
    }

    if (!sender || !receiver) {
        response->update_result = WUPDATE_INTF_NOTFOUND;
        return;
    }
    printf(LOG_PREFIX LOG_INDENT "Performing update: from=" MAC_FMT ", to=" MAC_FMT ", snr=%d\n",
           MAC_ARGS(sender->addr), MAC_ARGS(receiver->addr), request->snr);
    ctx->snr_matrix[sender->index * ctx->num_stas + receiver->index] = request->snr;
    response->update_result = WUPDATE_SUCCESS;
}

/**
 * Send a response to a client
 * @param connection_soc The FD of the connection socket
 * @param response The response to send
 * @return An integer WACTION constant to determine what to do next
 */
int send_update_response(int connection_soc, const snr_update_response *response) {
    ssize_t bytes_sent = sendfull(connection_soc, (const void *) response, sizeof(snr_update_response), MSG_NOSIGNAL);
    if (bytes_sent < 0) {
        int retval;
        if (bytes_sent == -2) {
            retval = WACTION_DISCONNECTED;
        } else {
            perror(LOG_PREFIX LOG_INDENT LOG_INDENT "Error while sending");
            retval = WACTION_ERROR;
        }
        close(connection_soc);
        return retval;
    }
    return WACTION_CONTINUE;
}

/**
 * Run the server using the given wmediumd context
 * @param ctx The wmediumd context
 * @return NULL, required for pthread
 */
void *run_wserver(void *ctx) {
    old_sig_handler = signal(SIGINT, handle_sigint);

    listenSoc = create_listen_socket();
    if (listenSoc < 0) {
        return NULL;
    }
    while (wserver_run_bit) {
        printf(LOG_PREFIX "Waiting for client to connect...\n");
        int connectSoc = accept_connection(listenSoc);
        printf(LOG_PREFIX LOG_INDENT "Client connected\n");
        while (1) {
            snr_update_request request;
            snr_update_response response;
            int action_resp;

            printf(LOG_PREFIX "Waiting for request...\n");
            action_resp = receive_update_request(connectSoc, &request);
            if (action_resp == WACTION_DISCONNECTED) {
                printf(LOG_PREFIX LOG_INDENT "Client has disconnected\n");
                break;
            }
            if (action_resp == WACTION_ERROR) {
                printf(LOG_PREFIX LOG_INDENT "Disconnecting client because of error\n");
                break;
            }
            printf(LOG_PREFIX LOG_INDENT "Received: from=" MAC_FMT ", to=" MAC_FMT ", snr=%d\n",
                   MAC_ARGS(request.from_addr), MAC_ARGS(request.to_addr), request.snr);
            if (action_resp == WACTION_CLOSE) {
                printf(LOG_PREFIX LOG_INDENT "Closing server\n");
                wserver_run_bit = false;
                break;
            }

            handle_update_request(ctx, &request, &response);

            printf(LOG_PREFIX "Answering with state %d...\n", response.update_result);
            action_resp = send_update_response(connectSoc, &response);
            if (action_resp == WACTION_DISCONNECTED) {
                printf(LOG_PREFIX LOG_INDENT "Client has disconnected\n");
                break;
            }
            printf(LOG_PREFIX LOG_INDENT "Answer sent.\n");
            if (action_resp == WACTION_ERROR) {
                printf(LOG_PREFIX LOG_INDENT "Disconnecting client because of error\n");
                break;
            } else if (action_resp == WACTION_CLOSE) {
                printf(LOG_PREFIX LOG_INDENT "Closing server\n");
                wserver_run_bit = false;
                break;
            }
        }
        close(connectSoc);
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
