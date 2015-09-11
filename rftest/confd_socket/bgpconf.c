/*
 * bgpconf.c
 *
 * Copyright (C) 2015 - mikael sondi
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "confd_lib.h"
#include "confd_cdb.h"
#include "bgp.h"

struct config {
    struct in_addr ip, ip1, ip2, ip23;
    unsigned int as, rem;
	char *net;
};
static struct config running_db[256];
static int num_servers = 0;
static int num_servers1 = 0;
static int num_servers2 = 0;

static int read_conf(struct sockaddr_in *addr)
{
    int rsock, i, n, st = CONFD_OK;
    struct in_addr ip;
    uint32_t as;
    //char buf[BUFSIZ];

    if ((rsock = socket(PF_INET, SOCK_STREAM, 0)) < 0 )
        return CONFD_ERR;
    if (cdb_connect(rsock, CDB_READ_SOCKET, (struct sockaddr*)addr,
                    sizeof (struct sockaddr_in)) < 0)
        return CONFD_ERR;
    if (cdb_start_session(rsock, CDB_RUNNING) != CONFD_OK)
        return CONFD_ERR;
    cdb_set_namespace(rsock, bgpd__ns);
    num_servers = 0;
    if ((n = cdb_num_instances(rsock, "/router/bgp")) < 0) {
        cdb_end_session(rsock);
        cdb_close(rsock);
        return n;
    }

    for(i=0; i<n; i++) {
        char tmppath[400];
        sprintf(tmppath, "/router/bgp[%d]/as", i);
        if ((st = cdb_get_u_int32(rsock, &as, tmppath)) != CONFD_OK)
            break;
	sprintf(tmppath, "/router/bgp[%d]/router-id", i);
        if ((st = cdb_get_ipv4(rsock, &ip, tmppath))!= CONFD_OK)
            break;
		
        running_db[num_servers].ip.s_addr = ip.s_addr;
        running_db[num_servers].as = as;
        ++num_servers;
    }
    cdb_end_session(rsock),
    cdb_close(rsock);
    return st;
}

static int read_conf1(struct sockaddr_in *addr)
{
    int rsock, i, m, st1 = CONFD_OK;
    struct in_addr  ip1;
    uint32_t rem;
    //char buf[BUFSIZ];

    if ((rsock = socket(PF_INET, SOCK_STREAM, 0)) < 0 )
        return CONFD_ERR;
    if (cdb_connect(rsock, CDB_READ_SOCKET, (struct sockaddr*)addr,
                    sizeof (struct sockaddr_in)) < 0)
        return CONFD_ERR;
    if (cdb_start_session(rsock, CDB_RUNNING) != CONFD_OK)
        return CONFD_ERR;
    cdb_set_namespace(rsock, bgpd__ns);
    num_servers1 = 0;
    if ((m = cdb_num_instances(rsock, "/neighbor/conf")) < 0) {
        cdb_end_session(rsock);
        cdb_close(rsock);
        return m;
    }

    for(i=0; i<m; i++) {
        char tmppath[400];
		
	sprintf(tmppath, "/neighbor/conf[%d]/ip-address", i);
        if ((st1 = cdb_get_ipv4(rsock, &ip1, tmppath))!= CONFD_OK)
            break;
		
	sprintf(tmppath, "/neighbor/conf[%d]/as", i);
        if ((st1 = cdb_get_u_int32(rsock, &rem, tmppath)) != CONFD_OK)
            break;
		
		running_db[num_servers1].ip1.s_addr = ip1.s_addr;
		running_db[num_servers1].rem = rem;
        ++num_servers1;
    }
    cdb_end_session(rsock),
    cdb_close(rsock);
    return st1;
}
static int read_conf2(struct sockaddr_in *addr)
{
    int rsock, i, o, st2 = CONFD_OK;
    struct in_addr  ip2, ip23;

    if ((rsock = socket(PF_INET, SOCK_STREAM, 0)) < 0 )
        return CONFD_ERR;
    if (cdb_connect(rsock, CDB_READ_SOCKET, (struct sockaddr*)addr,
                    sizeof (struct sockaddr_in)) < 0)
        return CONFD_ERR;
    if (cdb_start_session(rsock, CDB_RUNNING) != CONFD_OK)
        return CONFD_ERR;
    cdb_set_namespace(rsock, bgpd__ns);
    num_servers2 = 0;
    if ((o = cdb_num_instances(rsock, "/network/mask")) < 0) {
        cdb_end_session(rsock);
        cdb_close(rsock);
        return o;
    }

    for(i=0; i<o; i++) {
        char tmppath[400];
		
	sprintf(tmppath, "/network/mask[%d]/ip-address", i);
        if ((st2 = cdb_get_ipv4(rsock, &ip2, tmppath))!= CONFD_OK)
            break;
		
	sprintf(tmppath, "/network/mask[%d]/submask", i);
        if ((st2 = cdb_get_ipv4(rsock, &ip23, tmppath)) != CONFD_OK)
            break;
		running_db[num_servers2].ip2.s_addr = ip2.s_addr;
		running_db[num_servers2].ip23.s_addr = ip23.s_addr;
        ++num_servers2;
    }
    cdb_end_session(rsock),
    cdb_close(rsock);
    return st2;
}
char *ro = "router bgp ";
char *roid = "bgp router-id";
char *ne = "neighbor ";
char *remo = "remote-as";
char *net = "network";
char *heading = "password routeflow\nenable password routeflow\n!\n";
char *nett = "network";
char *maskk = "mask";
int i;
FILE *fp;

static void display_config()
{
    /* This function is called initially and whenever there has been  */
    /* a configuration chage. In this simple example, we just dump it */
    /* on stdout */

    
	//opening a file for writing
	fp = fopen("/etc/quagga/bgpd.conf", "w+");

    printf("----- Working with these settings now -----\n");
	printf("%s", heading);
	fprintf(fp, "%s", heading);
    for(i=0; i<num_servers; i++) {
		u_int8_t *ip = (u_int8_t *) &running_db[i].ip.s_addr;

	printf("%s%d\n %s %d.%d.%d.%d\n", ro, running_db[i].as, roid, ip[0],ip[1],ip[2],ip[3]); 
	//printf(" %s %d.%d.%d.%d %s %d\n", ne, ip1[0],ip1[1],ip1[2],ip1[3], remo, running_db[i].rem);

	fprintf(fp, "%s%d\n %s %d.%d.%d.%d\n", ro, running_db[i].as, roid, ip[0],ip[1],ip[2],ip[3]); 
	//fprintf(fp, " %s %d.%d.%d.%d %s %d\n", ne, ip1[0],ip1[1],ip1[2],ip1[3], remo, running_db[i].rem);

	
    }
    printf("-------------------------------------------\n");


//fputs("This is testing for fputs...\n", fp);
fclose(fp);

}

static void display_config1()
{
    /* This function is called initially and whenever there has been  */
    /* a configuration chage. In this simple example, we just dump it */
    /* on stdout */

	//opening a file for writing
	fp = fopen("/etc/quagga/bgpd.conf", "a+");
	
    printf("----- Working with these settings now -----\n");
	printf("%s", heading);
	//fprintf(fp, "%s", heading);
    for(i=0; i<num_servers1; i++) {
        //u_int8_t *ip = (u_int8_t *) &running_db[i].ip.s_addr; /* this is in NBO */
	u_int8_t *ip1 = (u_int8_t *) &running_db[i].ip1.s_addr;

	//printf("%s%d\n %s %d.%d.%d.%d\n", ro, running_db[i].as, roid, ip[0],ip[1],ip[2],ip[3]); 
	printf(" %s %d.%d.%d.%d %s %d\n", ne, ip1[0],ip1[1],ip1[2],ip1[3], remo, running_db[i].rem);

	//fprintf(fp, "%s%d\n %s %d.%d.%d.%d\n", ro, running_db[i].as, roid, ip[0],ip[1],ip[2],ip[3]); 
	fprintf(fp, " %s %d.%d.%d.%d %s %d\n", ne, ip1[0],ip1[1],ip1[2],ip1[3], remo, running_db[i].rem);

	
    }
    printf("-------------------------------------------\n");


//fputs("This is testing for fputs...\n", fp);
fclose(fp);

}
static void display_config2()
{
    /* This function is called initially and whenever there has been  */
    /* a configuration chage. In this simple example, we just dump it */
    /* on stdout */

	//opening a file for writing
	fp = fopen("/etc/quagga/bgpd.conf", "a+");
	
    printf("----- Working with these settings now -----\n");
	printf("%s", heading);
	//fprintf(fp, "%s", heading);
    for(i=0; i<num_servers2; i++) {
        //u_int8_t *ip = (u_int8_t *) &running_db[i].ip.s_addr; /* this is in NBO */
	u_int8_t *ip2 = (u_int8_t *) &running_db[i].ip2.s_addr;
	u_int8_t *ip23 = (u_int8_t *) &running_db[i].ip23.s_addr;

	//printf("%s%d\n %s %d.%d.%d.%d\n", ro, running_db[i].as, roid, ip[0],ip[1],ip[2],ip[3]); 
	printf(" %s %d.%d.%d.%d %s %d.%d.%d.%d\n", nett, ip2[0],ip2[1],ip2[2],ip2[3], maskk, ip23[0],ip23[1],ip23[2],ip23[3]);

	//fprintf(fp, "%s%d\n %s %d.%d.%d.%d\n", ro, running_db[i].as, roid, ip[0],ip[1],ip[2],ip[3]); 
	fprintf(fp, " %s %d.%d.%d.%d %s %d.%d.%d.%d\n", nett, ip2[0],ip2[1],ip2[2],ip2[3], maskk, ip23[0],ip23[1],ip23[2],ip23[3]);

	
    }
    printf("-------------------------------------------\n");


//fputs("This is testing for fputs...\n", fp);
fclose(fp);

}

int main(int argc, char **argv)
{
    struct sockaddr_in addr;
    int subsock;
    int status, status1, status2;
    int spoint, spoint1, spoint2;

    setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_family = AF_INET;
    addr.sin_port = htons(CONFD_PORT);

    confd_init(argv[0], stderr, CONFD_TRACE);

    if ((subsock = socket(PF_INET, SOCK_STREAM, 0)) < 0 )
        confd_fatal("Failed to open socket\n");
    if (cdb_connect(subsock, CDB_SUBSCRIPTION_SOCKET, (struct sockaddr*)&addr,
                    sizeof (struct sockaddr_in)) < 0)
        confd_fatal("Failed to confd_connect() to confd \n");
    if ((status = cdb_subscribe(subsock, 3, bgpd__ns, &spoint,"/router"))
        != CONFD_OK) {
        fprintf(stderr, "Terminate: subscribe %d\n", status);
        exit(0);
    }
	if ((status1 = cdb_subscribe(subsock, 3, bgpd__ns, &spoint1,"/neighbor"))
        != CONFD_OK) {
        fprintf(stderr, "Terminate: subscribe %d\n", status1);
        exit(0);
    }
	if ((status2 = cdb_subscribe(subsock, 3, bgpd__ns, &spoint2,"/network"))
        != CONFD_OK) {
        fprintf(stderr, "Terminate: subscribe %d\n", status2);
        exit(0);
    }
    if (cdb_subscribe_done(subsock) != CONFD_OK)
        confd_fatal("cdb_subscribe_done() failed");

    printf("Subscription point = %d\n", spoint);

    if ((status = read_conf(&addr)) != CONFD_OK) {
        fprintf(stderr, "Terminate: read_conf %d\n", status);
        exit(0);
    }
	if ((status = read_conf1(&addr)) != CONFD_OK) {
        fprintf(stderr, "Terminate: read_conf %d\n", status1);
        exit(0);
    }
	if ((status = read_conf2(&addr)) != CONFD_OK) {
        fprintf(stderr, "Terminate: read_conf %d\n", status2);
        exit(0);
    }
    display_config();
	display_config1();
	display_config2();

    /* Poll loop */

    while (1) {
        struct pollfd set[1];

        set[0].fd = subsock;
        set[0].events = POLLIN;
        set[0].revents = 0;


        if (poll(set, sizeof(set)/sizeof(*set), -1) < 0) {
            perror("Poll failed:");
            continue;
        }

        /* Check for I/O */
        if (set[0].revents & POLLIN) {
            int sub_points[1];
            int reslen;
            if ((status = cdb_read_subscription_socket(subsock,
                                                       &sub_points[0],
                                                       &reslen)) != CONFD_OK)
                exit(status);
            if (reslen > 0) {
                if ((status = read_conf(&addr)) != CONFD_OK)
                    exit(0);
            }
            /* This is the place where we may act on the newly read configuration */
            display_config();

            if ((status = cdb_sync_subscription_socket(subsock, CDB_DONE_PRIORITY))
                != CONFD_OK) {
                exit(status);
            }
		/* Check for I/O */
        if (set[0].revents & POLLIN) {
            int sub_points[1];
            int reslen;
            if ((status1 = cdb_read_subscription_socket(subsock,
                                                       &sub_points[0],
                                                       &reslen)) != CONFD_OK)
                exit(status1);
            if (reslen > 0) {
                if ((status1 = read_conf1(&addr)) != CONFD_OK)
                    exit(0);
            }
            /* This is the place where we may act on the newly read configuration */
            display_config1();

            if ((status1 = cdb_sync_subscription_socket(subsock, CDB_DONE_PRIORITY))
                != CONFD_OK) {
                exit(status1);
            }
        }

		/* Check for I/O */
        if (set[0].revents & POLLIN) {
            int sub_points[1];
            int reslen;
            if ((status2 = cdb_read_subscription_socket(subsock,
                                                       &sub_points[0],
                                                       &reslen)) != CONFD_OK)
                exit(status2);
            if (reslen > 0) {
                if ((status2 = read_conf2(&addr)) != CONFD_OK)
                    exit(0);
            }
            /* This is the place where we may act on the newly read configuration */
            display_config2();

            if ((status2 = cdb_sync_subscription_socket(subsock, CDB_DONE_PRIORITY))
                != CONFD_OK) {
                exit(status2);
            }
        }
    }
}
}
