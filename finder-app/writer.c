#include <stdio.h>
#include <string.h>
#include <syslog.h>

int main(int argc, char * argv[]) {
  char filename[512];
  char string[512];
  FILE *ofile;

  openlog("writer", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL0);

  if (argc < 3) {
    syslog (LOG_ERR, "usage: %s file string", argv[0]);
    closelog();
    printf("usage: %s file string\n", argv[0]);
    return(1);
  }
  strcpy(filename, argv[1]);
  strcpy(string, argv[2]);
 
  syslog(LOG_DEBUG, "Writing %s to %s\n", string, filename);
  ofile = fopen(filename,"w");
  if (ofile != NULL) {
    fprintf(ofile, "%s\n", string);
    fclose(ofile);

  } else {
    syslog(LOG_ERR, "Unable to open file %s for writing.", filename);
  }
  

  closelog();
  return 0;
}
