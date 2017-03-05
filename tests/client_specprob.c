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

#include "../wmediumd/wserver_messages.h"
#include <stdlib.h>
#include <sys/un.h>
#include <stdio.h>
#include <sys/socket.h>


#define send_request(connection_soc, request, type) \
    { \
        int ret = wserver_send_msg(connection_soc, request, type); \
        if (ret < 0) { \
            perror("error while sending"); \
            close(connection_soc); \
            exit(EXIT_FAILURE); \
        } \
        printf("sent request\n"); \
    }


#define receive_response(connection_soc, response, elemtype, typeint) \
    { \
    wserver_msg base; \
    int recv_type; \
    int ret = wserver_recv_msg_base(connection_soc, &base, &recv_type); \
    if (ret < 0) { \
        perror("error while receiving"); \
        close(connection_soc); \
        exit(EXIT_FAILURE); \
    } \
    if (recv_type != typeint) { \
        fprintf(stderr, "Received invalid request of type %d", recv_type); \
        close(connection_soc); \
        exit(EXIT_FAILURE); \
    } \
    ret = wserver_recv_msg(connection_soc, response, elemtype); \
    if (ret < 0) { \
        perror("error while receiving"); \
        close(connection_soc); \
        exit(EXIT_FAILURE); \
    } \
    printf("received response of type %d\n", typeint); \
    }

void string_to_mac_address(const char *str, u8 *addr) {
    int a[ETH_ALEN];

    sscanf(str, "%x:%x:%x:%x:%x:%x",
           &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]);

    addr[0] = (u8) a[0];
    addr[1] = (u8) a[1];
    addr[2] = (u8) a[2];
    addr[3] = (u8) a[3];
    addr[4] = (u8) a[4];
    addr[5] = (u8) a[5];
}

int main() {
    int create_socket;
    struct sockaddr_un address;
    if ((create_socket = socket(AF_UNIX, SOCK_STREAM, 0)) > 0) {
        printf("Socket has been created\n");
    } else {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }
    address.sun_family = AF_LOCAL;
    strcpy(address.sun_path, WSERVER_SOCKET_PATH);
    if (connect(create_socket,
                (struct sockaddr *) &address,
                sizeof(address)) == 0) {
        printf("Connected to server\n");

        printf("==== station add\n");
        station_add_request request;
        string_to_mac_address("02:00:00:00:02:00", request.addr);
        send_request(create_socket, &request, station_add_request);
        station_add_response response;
        receive_response(create_socket, &response, station_add_response, WSERVER_ADD_RESPONSE_TYPE);
        printf("answer was: %d\n", response.update_result);

        printf("==== station add2\n");
        station_add_request request2;
        string_to_mac_address("02:00:00:00:01:00", request2.addr);
        send_request(create_socket, &request2, station_add_request);
        station_add_response response2;
        receive_response(create_socket, &response2, station_add_response, WSERVER_ADD_RESPONSE_TYPE);
        printf("answer was: %d\n", response2.update_result);

        printf("==== specprob update 1\n");
        specprob_update_request request3;
        string_to_mac_address("02:00:00:00:01:00", request3.from_addr);
        string_to_mac_address("02:00:00:00:02:00", request3.to_addr);
        double errprobs[] = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 0.10, 0.11, 0.12, 0.13, 0.14, 0.15, 0.16,
                             0.17, 0.18, 0.19, 0.20, 0.21, 0.22, 0.23, 0.24, 0.25, 0.26, 0.27, 0.28, 0.29, 0.30, 0.31,
                             0.32, 0.33, 0.34, 0.35, 0.36, 0.37, 0.38, 0.39, 0.40, 0.41, 0.42, 0.43, 0.44, 0.45, 0.46,
                             0.47, 0.48, 0.49, 0.50, 0.51, 0.52, 0.53, 0.54, 0.55, 0.56, 0.57, 0.58, 0.59, 0.60, 0.61,
                             0.62, 0.63, 0.64, 0.65, 0.66, 0.67, 0.68, 0.69, 0.70, 0.71, 0.72, 0.73, 0.74, 0.75, 0.76,
                             0.77, 0.78, 0.79, 0.80, 0.81, 0.82, 0.83, 0.84, 0.85, 0.86, 0.87, 0.88, 0.89, 0.90, 0.91,
                             0.92, 0.93, 0.94, 0.95, 0.96, 0.97, 0.98, 0.99, 0.100, 0.101, 0.102, 0.103, 0.104, 0.105,
                             0.106, 0.107, 0.108, 0.109, 0.110, 0.111, 0.112, 0.113, 0.114, 0.115, 0.116, 0.117, 0.118,
                             0.119, 0.120, 0.121, 0.122, 0.123, 0.124, 0.125, 0.126, 0.127, 0.128, 0.129, 0.130, 0.131,
                             0.132, 0.133, 0.134, 0.135, 0.136, 0.137, 0.138, 0.139, 0.140, 0.141, 0.142, 0.143, 0.144};
        u32 fixedpoint[144];
        for (int i = 0; i < 144; i++) {
            fixedpoint[i] = custom_floating_point_to_fixed_point(errprobs[i]);
        }
        memcpy(request3.errprob, fixedpoint, sizeof(u32) * 144);
        send_request(create_socket, &request3, specprob_update_request);
        specprob_update_response response3;
        receive_response(create_socket, &response3, specprob_update_response, WSERVER_SPECPROB_UPDATE_RESPONSE_TYPE);
        printf("answer was: %d\n", response3.update_result);

        printf("==== station del\n");
        station_del_by_mac_request request4;
        string_to_mac_address("02:00:00:00:02:00", request4.addr);
        send_request(create_socket, &request4, station_del_by_mac_request);
        station_del_by_mac_response response4;
        receive_response(create_socket, &response4, station_del_by_mac_response, WSERVER_DEL_BY_MAC_RESPONSE_TYPE);
        printf("answer was: %d\n", response4.update_result);

        printf("==== station del2\n");
        station_del_by_mac_request request5;
        string_to_mac_address("02:00:00:00:01:00", request5.addr);
        send_request(create_socket, &request5, station_del_by_mac_request);
        station_del_by_mac_response response5;
        receive_response(create_socket, &response5, station_del_by_mac_response, WSERVER_DEL_BY_MAC_RESPONSE_TYPE);
        printf("answer was: %d\n", response5.update_result);

        close(create_socket);
        printf("socket closed\n");
        return EXIT_SUCCESS;
    } else {
        perror("Server connection failed");
        return EXIT_FAILURE;
    }
}