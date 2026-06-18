/* server.c
 * Handles the creation of a server socket and data sending.
 * Author  : Jonatan Schroeder
 * Modified: Nov 6, 2021
 *
 * Modified by: Norm Hutchinson
 * Modified: Mar 5, 2022
 *
 * Notes: You will find useful examples in Beej's Guide to Network
 * Programming (http://beej.us/guide/bgnet/).
 */

#include "server.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <signal.h>
#include <pthread.h>

/* TODO: Fill in the server code. You are required to listen on all interfaces for connections. For each connection,
 * invoke the handler on a new thread. */
void run_server(const char *port, void (*handler)(void *)) {
    // TODO: Implement this function
}
