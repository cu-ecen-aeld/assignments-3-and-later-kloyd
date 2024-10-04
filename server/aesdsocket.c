#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>

#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>

void sig_handler(int s) {
  int saved_errno = errno;

  while(waitpid(-1, NULL, WNOHANG) > 0);

  errno = saved_errno;

}
int main(int argc, char * argv[]) {

  int status, len, flags;
  int socketFD, new_fd;
  struct addrinfo hints;
  struct addrinfo *servinfo;
  struct sockaddr_storage their_addr;
  socklen_t addr_size;
  void *buffer;


  openlog("aesdsocket", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL0);
  syslog(LOG_INFO, "Socket server startup.");
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((status = getaddrinfo(NULL, "9000", &hints, &servinfo)) != 0) {
    syslog (LOG_ERR, "Unable to bind to port 9000");
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

  addr_size = sizeof their_addr;
  new_fd = accept(socketFD, (struct sockaddr *)&their_addr, &addr_size);

  status = recv(new_fd, buffer, len, flags);

  syslog(LOG_INFO, "Normal socket server shutdown.");

  freeaddrinfo(servinfo);

  closelog();
  return 0;
}
