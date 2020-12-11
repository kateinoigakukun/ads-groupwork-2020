#include "grpwk20.h"
#include <stdio.h>
#include <stdlib.h>

#define SEGMENT_SIZE SR_SIZE
// x = 200000/(25 - log_4(x))
// log_4 (x) ≒ 7
#define SEGMENT_INDEX_SIZE 7
#define HEADER_SIZE SEGMENT_INDEX_SIZE

unsigned parseUInt(unsigned char upper, unsigned char lower) {
  return ((upper & 0x1) << 1) + (lower & 0x1);
}

unsigned char encodeUInt(unsigned value) {
  switch (value) {
  case 0:
    return BASE_A;
  case 1:
    return BASE_C;
  case 2:
    return BASE_G;
  case 3:
    return BASE_T;
  default:
    fprintf(stderr, "Invalid input for encodeUInt: %d", value);
    abort();
  }
}

void writeIndex(unsigned int segmentIndex, FILE *fp) {
  int shift = SEGMENT_INDEX_SIZE * 2;
  while (shift > 0) {
    shift--;
    unsigned upperBit = (segmentIndex >> shift) & 0x1;
    shift--;
    unsigned lowerBit = (segmentIndex >> shift) & 0x1;
    fputc(encodeUInt(upperBit * 2 + lowerBit), fp);
  }
}

int enc(void) {
  FILE *sourceFile;
  if ((sourceFile = fopen(ORGDATA, "r")) == NULL) {
    fprintf(stderr, "cannot open %s\n", ORGDATA);
    exit(1);
  }

  FILE *outputFile;
  if ((outputFile = fopen(ENCDATA, "w")) == NULL) {
    fprintf(stderr, "cannot open %s\n", ENCDATA);
    exit(1);
  }

  unsigned bodySize = SEGMENT_SIZE - HEADER_SIZE;
  unsigned segmentCount = (ORGDATA_LEN / 2 + bodySize - 1) / bodySize;

  for (int segmentIdx = 0; segmentIdx < segmentCount; segmentIdx++) {
    writeIndex(segmentIdx, outputFile);
    for (int bodyIdx = 0; bodyIdx < bodySize; bodyIdx++) {
      unsigned char upperBit = getc(sourceFile);
      unsigned char lowerBit = getc(sourceFile);
      unsigned value = parseUInt(upperBit, lowerBit);
      fputc(encodeUInt(value), outputFile);
    }
  }
  fputc('\n', outputFile);
  fclose(sourceFile);
  fclose(outputFile);
  return 0;
}

int main(void) {
  enc();
  return 0;
}
