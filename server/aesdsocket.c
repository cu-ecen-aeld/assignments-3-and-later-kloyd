#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>

#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>

#define PORT "9000"
#define BACKLOG 10

bool signal_continue = true;

void signal_handler(int signal_number) {
  int saved_errno = errno;

  if (signal_number == SIGINT) {
    printf("Received Interrupt, closing.\n");
    signal_continue = false;
  }
  if (signal_number == SIGTERM) {
    printf("Received SIGTERM, closing.\n");
    signal_continue = false;
  }

  errno = saved_errno;

}
int main(int argc, char * argv[]) {

  int status, len, flags;
  int socketFD, new_fd;
  struct addrinfo hints;
  struct addrinfo *servinfo;
  struct sockaddr_storage their_addr;
  socklen_t addr_size;
  char *buffer;
  struct sigaction monitor_thread;

  // Setup syslog first.
  openlog("aesdsocket", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL0);
  syslog(LOG_INFO, "Socket server startup.");

  // Setup Signal Handler
  memset(&monitor_thread, 0, sizeof(struct sigaction));
  monitor_thread.sa_handler = signal_handler;
  if (sigaction(SIGTERM, &monitor_thread, NULL) != 0) {
    syslog(LOG_ERR, "Error %d (%s) registering for SIGTERM", errno, strerror(errno));
    return(-1);
  }

  if (sigaction(SIGINT, &monitor_thread, NULL) != 0) {
    syslog(LOG_ERR, "Error %d (%s) registering for SIGINT", errno, strerror(errno));
    return(-1);
  }

  // Prepare to listen on port 9000
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((status = getaddrinfo(NULL, "9000", &hints, &servinfo)) != 0) {
    syslog (LOG_ERR, "Unable to bind to port 9000, error: %d (%m)", errno);
    return (-1);
  }

  // silly assumption... 1k byte buffer.
  len = 1024;
  flags = 0;
  buffer = malloc(len);
  if (buffer == NULL) {
    syslog(LOG_ERR, "Unable to allocate buffer memory. Program will exit.");
    return (-1);
  }

  socketFD = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  bind(socketFD, servinfo->ai_addr, servinfo->ai_addrlen);

  if (listen(socketFD, BACKLOG) == -1) {
    syslog(LOG_ERR, "Unable to listen to port 9000 (%m)");
    exit(-1);
  }
  
  printf("Listening on 9000\n");

  while (signal_continue) {
    addr_size = sizeof their_addr;
    new_fd = accept(socketFD, (struct sockaddr *)&their_addr, &addr_size);

    status = recv(new_fd, buffer, len, flags);
    if (status > 0) {
      printf("received: %s\n", buffer);

    }
    close(new_fd);
  }

  syslog(LOG_INFO, "Program completed.\n");

  freeaddrinfo(servinfo);
  closelog();

  return 0;
}
