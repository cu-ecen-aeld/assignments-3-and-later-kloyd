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
#define MAXBUFFER 10
#define LINEBUFFSIZE 20

bool signal_continue = true;

void signal_handler(int signal_number) {

  if (signal_number == SIGINT) {
    printf("Received Interrupt, closing.\n");
    signal_continue = false;
  }
  if (signal_number == SIGTERM) {
    printf("Received SIGTERM, closing.\n");
    signal_continue = false;
  }

}

int check_for_newline(char * buffer, int len) {
  // Return 1 if no newline (not end of input yet)
  // Return 0 if newline found.
  int status = 1;
  for(int i= 0; i < len; i++) {
    if (buffer[i] == '\n') {
      status = 0;
      break;
    }
  }
  return status;
}

int configure_signals() {
  
  struct sigaction monitor_thread;
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

  return 0;
}

int bind_to_port() {
  struct addrinfo hints;
  struct addrinfo *servinfo;
  int status, socketFD;

  // Bind to the PORT on 0.0.0.0 
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((status = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
    syslog (LOG_ERR, "Unable to bind to port %s, error: %d (%m)", PORT, errno);
    return (-1);
  }

  socketFD = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  bind(socketFD, servinfo->ai_addr, servinfo->ai_addrlen);

  freeaddrinfo(servinfo);

  return socketFD;

}


int main(int argc, char * argv[]) {

  int status, flags;
  int socketFD, receiveFD;
  socklen_t addr_size;
  struct sockaddr_storage their_addr;
  char buffer[MAXBUFFER];
  //struct sigaction monitor_thread;
  char *line_input;   // complete line received.
  int line_total, line_len;     // where are we at?

  // Setup syslog first.
  openlog("aesdsocket", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL0);
  syslog(LOG_INFO, "Socket server startup.");

  // Setup Signal Handler
  if (configure_signals() == -1) {
    return(-1);
  }
  
    flags = 0;
  /* e. Receives data over the connection and appends to file /var/tmp/aesdsocketdata, creating this file if it doesn’t exist. */
  // get FD to /var/tmp/aesdsocketdata
  socketFD = bind_to_port();
  if (socketFD == -1) {
    exit(-1);
  }
  
  if (listen(socketFD, BACKLOG) == -1) {
    syslog(LOG_ERR, "Unable to listen to port 9000 (%m)");
    exit(-1);
  }

  printf("Listening on 9000\n");

  while (signal_continue) {
    addr_size = sizeof their_addr;
    // printf("----------\ndebug: Accepting\n");
    receiveFD = accept(socketFD, (struct sockaddr *)&their_addr, &addr_size);

    // After accept, initialize the line_input buffer.
    line_input = malloc(LINEBUFFSIZE);
    if (line_input < 0) {
      syslog(LOG_ERR, "Unable to allocate memory for line buffer.");
      exit(-1);
    }
    line_input[0] = 0;
    line_total = LINEBUFFSIZE;
    line_len = 0;

    status = recv(receiveFD, buffer, MAXBUFFER - 1, flags);
    if (status > 0) {
      // first packet of MAXBUFFER - 1 size, just append to line_input
      line_len += status;
      // printf("received: %s", buffer);
      line_input = strcat(line_input, buffer);
      status = check_for_newline(buffer, MAXBUFFER);
      // if there was a newline, we're done.
      // otherwise, grab the next buffer full
      while (status > 0) {
        // clear buffer for another packet.
        memset(&buffer, 0, MAXBUFFER);
        status = recv(receiveFD, buffer, MAXBUFFER - 1, flags);
        line_len += status;
        // printf("%s|", buffer);
        // printf("debug: received bytes %d, total buffer: %d\n", line_len, line_total);
        // If we went over the line buffer size, reallocate more space for the line buffer.
        if (line_len  > line_total) {
          // realloc line_input
          line_total += LINEBUFFSIZE;
         // printf("debug: realloc to %d \n", line_total);
          line_input = realloc(line_input, line_total);
        }
        // Append to line_input.
        line_input = strcat(line_input, buffer);
        status = check_for_newline(buffer, MAXBUFFER);
        // keep going until we get a newline.
      } 
      
    }
    
    close(receiveFD);
    // write out line_input to file.
    printf("line_input: %s", line_input);
    // printf("debug: outside if\n");
    // free line_input.
    free(line_input);

  }

  // Program got a SIGINT or SIGTERM, clean up.
  // delete the file from /var/tmp

  // write log and close out.
  syslog(LOG_INFO, "Program completed.\n");

  closelog();

  return 0;
}
