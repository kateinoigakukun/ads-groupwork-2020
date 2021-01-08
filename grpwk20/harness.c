#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: ./harness [iteration]\n");
    exit(1);
  }

  int success = 0;
  int iteration = atoi(argv[1]);
  for (int i = 0; i < iteration; i++) {
    int status = system("bash -c \"./test.sh $(cat conf.txt | tail -n1) 2> /dev/null | grep -q hd=0\"");
    if (status == 0) {
      success++;
    }
  }
  printf("Success rate: %f\n", success/(double)iteration);
  return 0;
}
