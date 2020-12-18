#include "grpwk20.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>

void emitNPBlock(FILE *outputFile, char *buffer) {
  for (int i = 0; i < ORGDATA_LEN; i++) {
    char value = buffer[i];
    fputc(value, outputFile);
  }
}

int enc(void) {
  int sourceFD;
  if ((sourceFD = open(ORGDATA, O_RDONLY, S_IRUSR)) < 0) {
    fprintf(stderr, "cannot open %s\n", ORGDATA);
    exit(1);
  }

  FILE *outputFile;
  if ((outputFile = fopen(ENCDATA, "w")) == NULL) {
    fprintf(stderr, "cannot open %s\n", ENCDATA);
    exit(1);
  }

  char buffer[ORGDATA_LEN];
  char *rawBuffer;
  rawBuffer = mmap(NULL, ORGDATA_LEN, PROT_READ, MAP_PRIVATE, sourceFD, 0);

  // 左から1bit目をT or C、2bit目をA or Gで表現する
  // T and A = 0, C and G = 1
  for (int cursor = 0; cursor < ORGDATA_LEN; cursor += 2) {
    char upperBit = rawBuffer[cursor];
    char lowerBit = rawBuffer[cursor + 1];
    switch (upperBit) {
    case '0': buffer[cursor] = BASE_T; break;
    case '1': buffer[cursor] = BASE_C; break;
    }
    switch (lowerBit) {
    case '0': buffer[cursor + 1] = BASE_A; break;
    case '1': buffer[cursor + 1] = BASE_G; break;
    }
  }

  emitNPBlock(outputFile, buffer);

  fputc('\n', outputFile);
  close(sourceFD);
  fclose(outputFile);
  return 0;
}

int main(void) {
  enc();
  return 0;
}
