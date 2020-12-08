#include "grpwk20.h"
#include <stdio.h>
#include <stdlib.h>

#define SEGMENT_SIZE SR_SIZE
// x = 200000/(25 - log_4(x))
// log_4 (x) ≒ 7
#define HEADER_SIZE 7

unsigned decodeUInt(unsigned char value) {
  switch (value) {
  case BASE_A:
    return 0;
  case BASE_C:
    return 1;
  case BASE_G:
    return 2;
  case BASE_T:
    return 3;
  default:
    return 0;
    //      fprintf(stderr, "Invalid input for decodeUInt: %d", value);
    //      abort();
  }
}

int readSegmentIndex(FILE *fp) {
  int value = 0;
  int digits = HEADER_SIZE;
  while (digits > 0) {
    digits--;
    char loaded = getc(fp);
    if (loaded == '\n')
      return -1;
    value += decodeUInt(loaded) << (2 * digits);
  }
  return value;
}

void dec() {
  FILE *sourceFile;
  if ((sourceFile = fopen(SEQDATA, "r")) == NULL) {
    fprintf(stderr, "cannot open %s\n", SEQDATA);
    exit(1);
  }

  FILE *outputFile;
  if ((outputFile = fopen(DECDATA, "w")) == NULL) {
    fprintf(stderr, "cannot open %s\n", DECDATA);
    exit(1);
  }

  unsigned bodySize = SEGMENT_SIZE - HEADER_SIZE;
  unsigned segmentCount = 0; //(ORGDATA_LEN + bodySize - 1)/bodySize;
  unsigned maxSegmentCount = (1 << (HEADER_SIZE * 4)) - 1;

  unsigned char *buffer =
      malloc(sizeof(unsigned char) * maxSegmentCount * bodySize);

  for (int readSegmentIdx = 0; readSegmentIdx < maxSegmentCount;
       readSegmentIdx++) {
    unsigned index = readSegmentIndex(sourceFile);
    if (index == -1) {
      segmentCount = readSegmentIdx;
      break;
    }
    printf("segment[%d]\n", index);
    for (int bodyIdx = 0; bodyIdx < bodySize; bodyIdx++) {
      unsigned char value = getc(sourceFile);
      buffer[index * bodySize + bodyIdx] = value;
    }
  }

  for (int index = 0; index < segmentCount * bodySize; index++) {
    unsigned value = decodeUInt(buffer[index]);
    fputc((value >> 1) + '0', outputFile);
    fputc((value & 0x1) + '0', outputFile);
  }

  fputc('\n', outputFile);

  fclose(sourceFile);
  fclose(outputFile);
  return;
}

int main() {
  dec();
  return 0;
}
