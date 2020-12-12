#include "grpwk20.h"
#include <stdio.h>
#include <stdlib.h>

#define SEGMENT_SIZE SR_SIZE
// x = 200000/(25 - log_4(x))
// log_4 (x) ≒ 7
#define SEGMENT_INDEX_SIZE 7
#define SEGMENT_CONSTRAINT_LENGTH 3
#define HEADER_SIZE SEGMENT_INDEX_SIZE + SEGMENT_CONSTRAINT_LENGTH - 1

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

int coder(int *reg, int bit) {
  int registerSize = SEGMENT_CONSTRAINT_LENGTH - 1;
  int inputs[SEGMENT_CONSTRAINT_LENGTH];
  inputs[0] = bit;

  for (int i = 0; i < registerSize; i++) {
    inputs[1 + i] = (*reg >> (registerSize - i - 1)) & 0x1;
  }

  int c0 = (inputs[0] + inputs[2]) & 0x1;
  int c1 = (inputs[0] + inputs[1] + inputs[2]) & 0x1;

  *reg = (*reg >> 1) | (bit << (registerSize - 1));
  return (c1 << 1) + c0;
}

int encodeViterbi(int input, FILE *fp) {
  int reg = 0;
  for (int i = 0; i < SEGMENT_INDEX_SIZE; i++) {
    int bit = (input >> (SEGMENT_INDEX_SIZE - i - 1)) & 0x1;
    int output = coder(&reg, bit);
    fputc(encodeUInt(output), fp);
  }
  for (int i = 0; i < SEGMENT_CONSTRAINT_LENGTH - 1; i++) {
    int output = coder(&reg, 0);
    fputc(encodeUInt(output), fp);
  }
  return 0;
}

//void writeIndex(unsigned int segmentIndex, FILE *fp) {
//  int shift = SEGMENT_INDEX_SIZE * 2;
//  while (shift > 0) {
//    shift--;
//    unsigned upperBit = (segmentIndex >> shift) & 0x1;
//    shift--;
//    unsigned lowerBit = (segmentIndex >> shift) & 0x1;
//    fputc(encodeUInt(upperBit * 2 + lowerBit), fp);
//  }
//}

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
    encodeViterbi(segmentIdx, outputFile);
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
//  enc();
  encodeViterbi(28, stdout);
  return 0;
}
