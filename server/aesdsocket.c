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

void write_log(char *line_input) {
  FILE *logfile;
  // open log file for (a)ppending.
  logfile = open_log_file_name("a");
  if (logfile != NULL) {
    // write out line_input to file.
    fprintf(logfile, "%s", line_input);
    fclose(logfile);
  } else {
    // can't open the log file.
    syslog(LOG_ERR, "Unable to write log file %s", log_file_name);
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

int recv_packet(int serverSocket, char *hostIp, bool signal_continue, bool *conn_in_progress, int line_buffer_size) {
  socklen_t addr_size;
  struct sockaddr_storage their_addr;
  int line_total, line_len;
  ssize_t status;
  int flags;
  char tempHost[NI_MAXHOST];
  char buffer[MAXBUFFER];
  char *line_input;
  int clientSocket;

  addr_size = sizeof their_addr;
  flags = 0;
  line_input = malloc(LINEBUFFSIZE);
  clientSocket = 0;
  // printf("debug: accept\n");
  if (signal_continue) {
    clientSocket = accept(serverSocket, (struct sockaddr *)&their_addr, &addr_size);
    getnameinfo((struct sockaddr *)&their_addr, addr_size, tempHost, sizeof(tempHost), NULL, 0, NI_NUMERICHOST);
    strcpy(hostIp, tempHost);

    // Things need to happen after an accept, clear out line input.
    line_input[0] = 0;
    line_total = line_buffer_size;
    line_len = 0;
    // start with clear buffer.
    memset(&buffer, 0, MAXBUFFER);
    status = recv(clientSocket, buffer, MAXBUFFER - 1, flags);

    if (status > 0) {
        if (signal_continue) {
          *conn_in_progress = true;
          syslog(LOG_INFO, "Accepted connection from %s", hostIp);
        }
        // first packet of MAXBUFFER - 1 size, just append to line_input
        line_len += status;
        line_input = strcat(line_input, buffer);
        status = check_for_newline(buffer, MAXBUFFER);
        // if there was a newline, we're done.
        // otherwise, grab the next buffer full
        while (status > 0) {
          // clear buffer for another packet.
          memset(&buffer, 0, MAXBUFFER);
          status = recv(clientSocket, buffer, MAXBUFFER - 1, flags);
          line_len += status;
          // If we went over the line buffer size, reallocate more space for the line buffer.
          if (line_len  > line_total) {
            line_total += LINEBUFFSIZE;
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
        write_log(line_input);
        free(line_input);
    }

  }
  return clientSocket;
}

void send_log(int clientSocket) {
      // open logfile for reading
      char buffer[MAXBUFFER];
      FILE *logfile = open_log_file_name("r");
      if (logfile == NULL) {
        exit(-1);
      }
      // read each line and send to client 
      ssize_t readsize, sendsize;
      while ((readsize = fread(buffer, 1, sizeof(buffer) -1 , logfile)) > 0) {
        buffer[readsize] = '\0';
        sendsize = send(clientSocket, buffer, readsize, 0);
        if (sendsize != readsize) {
          syslog(LOG_WARNING, "Did not send full buffer.");
        }
      }
      fclose(logfile);
}

void close_socket(int *clientSocket, bool *conn_in_progress, char *hostip) {
    // Conversation complete, close socket and go around again.
    close(*clientSocket);
    if (conn_in_progress) {
      syslog(LOG_INFO, "Closed connection from %s", hostip);
      conn_in_progress = false;
    }
}

int main(int argc, char * argv[]) {

  int status, opt;
  int socketFD, clientSocket;
  bool conn_in_progress;
  FILE* logfile;
  bool daemon_mode;
  pid_t fork_result;
  char *hostIp;

  fork_result = -1;
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
    /* On  success, the PID of the child process is returned in the parent, and 0 is returned in the child.  On failure, -1 is returned
       in the parent, no child process is created, and errno is set appropriately.*/
  }

  
  // Allocate line buffer.

  int line_buffer_size = LINEBUFFSIZE;
  hostIp = malloc(16);
  // "xxx.xxx.xxx.xxx"
  // Figure out background road 
  if (daemon_mode && fork_result != 0) {
    // Parent process and it's in background mode -d
    // fork_result != 0 is parent thread
    // exit
    exit(0);
  }
  conn_in_progress = false;
  while (signal_continue) {
    clientSocket = recv_packet(socketFD, hostIp, signal_continue, &conn_in_progress, line_buffer_size);
    if (clientSocket > 0) {
      send_log(clientSocket);
      close_socket(&clientSocket, &conn_in_progress, hostIp);
    }
  }

  // Program got a SIGINT or SIGTERM, clean up.
  // delete the file from /var/tmp
  status = unlink(log_file_name);
  if (status == -1) {
    syslog(LOG_ERR, "Unable to delete %s from the file system (%m)", log_file_name);
  }
  // write log and close out.
  syslog(LOG_INFO, "aesdsocket server completed");

  closelog();

}
