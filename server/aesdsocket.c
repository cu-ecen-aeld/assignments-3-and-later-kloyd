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
#define MAXBUFFER 256
#define LINEBUFFSIZE 1024
#define log_file_name "/var/tmp/aesdsocketdata"

bool signal_continue = true;

void signal_handler(int signal_number) {
  // Handle INT/TERM with graceful shutdown.
  if (signal_number == SIGINT) {
    syslog(LOG_INFO, "Caught signal, exiting");
    signal_continue = false;
  }
  if (signal_number == SIGTERM) {
    syslog(LOG_INFO, "Caught signal, exiting");
    signal_continue = false;
  }
}

int check_for_newline(char * buffer, int len) {
  // 0 for found, 1 for not found.
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
  // hook up signal handler function.
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
  // All good.
  return 0;
}

FILE* open_log_file_name(const char* mode) {
  FILE* ofile;
  // open for (a)ppending, creates if does not exist.
  // or (r)eading
  ofile = fopen(log_file_name, mode);
  if (ofile != NULL) {
    return ofile;
  } else {
    syslog(LOG_ERR, "Unable to open file %s for writing (%m)", log_file_name);
    exit(-1);
  }
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

int recv_packet(char *line_input, int serverSocket, bool signal_continue, bool *conn_in_progress) {
  socklen_t addr_size;
  struct sockaddr_storage their_addr;
  int line_total, line_len;
  ssize_t status;
  int flags;

  char buffer[MAXBUFFER];
  char hostip[NI_MAXHOST];
  addr_size = sizeof their_addr;
  flags = 0;
  int clientSocket = accept(serverSocket, (struct sockaddr *)&their_addr, &addr_size);
  getnameinfo((struct sockaddr *)&their_addr, addr_size, hostip, sizeof(hostip), NULL, 0, NI_NUMERICHOST);
  if (signal_continue) {
    conn_in_progress = true;
    syslog(LOG_INFO, "Accepted connection from %s", hostip);
  }
  // Allocate line buffer.
  line_input = malloc(LINEBUFFSIZE);
  if (line_input < 0) {
    syslog(LOG_ERR, "Unable to allocate memory for line buffer.");
    exit(-1);
  }
    // Things need to happen after an accept, clear out line input.
  line_input[0] = 0;
  // n.b. line_input won't be NULL on return, but may be null terminated from no receive.
  line_total = LINEBUFFSIZE;
  line_len = 0;
  // start with clear buffer.
  memset(&buffer, 0, MAXBUFFER);
  status = recv(clientSocket, buffer, MAXBUFFER - 1, flags);
  if (status > 0) {
      // first packet of MAXBUFFER - 1 size, just append to line_input
      line_len += status;
      // printf("debug received: %s", buffer);
      line_input = strcat(line_input, buffer);
      status = check_for_newline(buffer, MAXBUFFER);
      // if there was a newline, we're done.
      // otherwise, grab the next buffer full
      while (status > 0) {
        // clear buffer for another packet.
        memset(&buffer, 0, MAXBUFFER);
        status = recv(clientSocket, buffer, MAXBUFFER - 1, flags);
        line_len += status;
        // printf("debug: %s|", buffer);
        // printf("debug: received bytes %d, total buffer: %d\n", line_len, line_total);
        // If we went over the line buffer size, reallocate more space for the line buffer.
        if (line_len  > line_total) {
          // realloc line_input
          line_total += LINEBUFFSIZE;
          // printf("debug: realloc to %d \n", line_total);
          char *tmp = realloc(line_input, line_total);
          if (tmp == NULL) {
            syslog(LOG_ERR, "Failed to realloc (%m)");
            exit(-1);
          }
          line_input = tmp;
        }
        // Append to line_input.
        line_input = strcat(line_input, buffer);
        status = check_for_newline(buffer, MAXBUFFER);
        // keep going until we get a newline.
      } 

      // we have the entire packet for returning.
  }

  return clientSocket;
}

FILE* write_log(char *line_input) {
  FILE *logfile;
  // open log file for (a)ppending.
  logfile = open_log_file_name("a");
  // write out line_input to file.
  // printf("line_input: %s", line_input);
  fprintf(logfile, "%s", line_input);
  fclose(logfile);
  free(line_input);
  return logfile;
}

send_log(int clientSocket, char *line_input) {
      // open logfile for reading
      FILE *logfile = open_log_file_name("r");
      if (logfile == NULL) {
        exit(-1);
      }
      // read each line
      // send to client 
      ssize_t readsize, sendsize;
      while ((readsize = fread(buffer, 1, sizeof(buffer) -1 , logfile)) > 0) {
        buffer[readsize] = '\0';
        sendsize = send(clientSocket, buffer, readsize, 0);
        if (sendsize != readsize) {
          syslog(LOG_WARNING, "Did not send full buffer.");
        }
      }
      // close logfile
      fclose(logfile);

}

int main(int argc, char * argv[]) {

  int status, opt;
  int socketFD, clientSocket;
  bool conn_in_progress;
  char *line_input;   // complete line received.
  int line_total, line_len;     // where are we at?
  FILE* logfile;
  bool daemon_mode;
  pid_t fork_result;

  // Setup syslog first.
  openlog("aesdsocket", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL0);
  syslog(LOG_INFO, "Socket server startup");
  daemon_mode = false;
  while ((opt = getopt(argc, argv, "d")) != -1) {
    if (opt == 'd') {
      daemon_mode = true;
    }
  }

  // Setup Signal Handler
  if (configure_signals() == -1) {
    exit(-1);
  }
  
  // bind to port 9000.
  socketFD = bind_to_port();
  if (socketFD == -1) {
    exit(-1);
  }
  
  if (listen(socketFD, BACKLOG) == -1) {
    syslog(LOG_ERR, "Unable to listen to port %s (%m)", PORT);
    exit(-1);
  }

  // ensure empty capture file when starting up.
  logfile = open_log_file_name("w");
  fclose(logfile);

  // debug code.
  if (daemon_mode) {
    syslog(LOG_INFO, "Starting in daemon mode.");
  } else {
    syslog(LOG_INFO, "Starting in foreground mode.");
  }
  // end debug
  //  program should fork after ensuring it can bind to port 9000
  if (daemon_mode) {
    fork_result = fork();
  }
  conn_in_progress = false;
  // properly start accepting.
  printf("Accepting connection on %s\n", PORT);
  if (1) {
    while (signal_continue) {
      clientSocket = recv_packet(line_input, signal_continue, &conn_in_progress);
      logfile = write_log(line_input);
      send_log(logfile);
      close_socket();
    }
    
    }  // end of recv() block.

    // Conversation complete, close socket and go around again.
    // end of recv() block.
    close(clientSocket);
    if (conn_in_progress) {
      syslog(LOG_INFO, "Closed connection from %s", hostip);
      conn_in_progress = false;
    }
  }
  // end daemon / fork test.

  // Program got a SIGINT or SIGTERM, clean up.
  // delete the file from /var/tmp
  status = unlink(log_file_name);
  if (status == -1) {
    syslog(LOG_ERR, "Unable to delete %s from the file system (%m)", log_file_name);
  }
  // write log and close out.
  syslog(LOG_INFO, "aesdsocket server completed");

  closelog();

  return 0;
}
