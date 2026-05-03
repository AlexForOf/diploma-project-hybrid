#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include <sys/types.h>

int main(int argc, char *argv[]) {
  if(argc != 2) {
    fprintf(stderr, "Usage: %s <orchestrator_pid>\n", argv[0]);
    exit(1);
  }

  pid_t bash_pid = atoi(argv[1]);

  FILE *log_file = fopen("telemetry/sensor_stream.log", "w");
  if (log_file == NULL) {
    perror("Failed to open telemetry stream");
    exit(2);
  }

  int counter = 0;
  while(1) {
    fprintf(log_file, "[CLEAR] System stable. Cycle %d\n", counter);

    fflush(log_file);

    sleep(1);
    counter++;

    if(counter == 3) {
      fprintf(log_file, "[SPIKE DETECTED] Metric anomaly threshold exceeded!\n");

      fflush(log_file);

      kill(bash_pid, SIGUSR1);

      fclose(log_file);
      exit(0);
    }
  }

  return 0;
}
