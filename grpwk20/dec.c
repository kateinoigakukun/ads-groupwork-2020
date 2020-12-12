#include "grpwk20.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define SEGMENT_SIZE SR_SIZE
// x = 200000/(25 - log_4(x))
// log_4 (x) ≒ 7
#define SEGMENT_INDEX_SIZE 7
#define SEGMENT_CONSTRAINT_LENGTH 3
#define SEGMENT_INDEX_STATE_COUNT (SEGMENT_CONSTRAINT_LENGTH - 1) * 2
#define HEADER_SIZE SEGMENT_INDEX_SIZE + SEGMENT_CONSTRAINT_LENGTH - 1

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
  int digits = SEGMENT_INDEX_SIZE;
  while (digits > 0) {
    digits--;
    char loaded = getc(fp);
    if (loaded == '\n')
      return -1;
    value += decodeUInt(loaded) << (2 * digits);
  }
  return value;
}

int hammingDistance(int lhs, int rhs) {
  int xored = lhs ^ rhs;
  int distance = 0;
  while (xored) {
    xored &= xored - 1;
    distance++;
  }
  return distance;
}

typedef struct {
  int output;
  int nextState;
} transition_output_t;

transition_output_t convolutionTransition(int input, int state) {
  int inputs[SEGMENT_CONSTRAINT_LENGTH];
  inputs[0] = input;
  int registerSize = SEGMENT_CONSTRAINT_LENGTH - 1;
  for (int i = 0; i < registerSize; i++) {
    inputs[1 + i] = (state >> (registerSize - i - 1)) & 0x1;
  }

  int nextState = (state >> 1) | (input << (registerSize - 1));
  int c0 = (inputs[0] + inputs[2]) & 0x1;
  int c1 = (inputs[0] + inputs[1] + inputs[2]) & 0x1;

  return (transition_output_t){
    .output = (c1 << 1) + c0, .nextState = nextState
  };
}

typedef struct {
  int fromState;
  int minPathMetric;
  int inputBit;
  bool isActivated;
} state_node_t;

int decodeViterbi(FILE *fp) {
  state_node_t paths[HEADER_SIZE + 1][SEGMENT_INDEX_STATE_COUNT];
  paths[0][0].fromState = 0;
  paths[0][0].minPathMetric = 0;
  paths[0][0].isActivated = true;

  for (int time = 0; time < HEADER_SIZE; time++) {
    
    char inputBitChar = getc(fp);
    if (inputBitChar == '\n')
      return -1;
    int expectedOutput = decodeUInt(inputBitChar);

    for (int state = 0; state < SEGMENT_INDEX_STATE_COUNT; state++) {
      if (!paths[time][state].isActivated) continue;

      bool onlyAcceptZero = time > SEGMENT_INDEX_SIZE;
      for (int possibleInput = 0; possibleInput <= (onlyAcceptZero ? 0 : 1); possibleInput++) {
        transition_output_t output = convolutionTransition(possibleInput, state);
        
        int branchMetric = hammingDistance(expectedOutput, output.output);
        int pathMetric = paths[time][state].minPathMetric + branchMetric;
        
        state_node_t *nextStates = paths[time + 1];
        bool shouldUpdate = (nextStates[output.nextState].isActivated) &&
        nextStates[output.nextState].minPathMetric > pathMetric;
        
        if (!nextStates[output.nextState].isActivated || shouldUpdate) {
          nextStates[output.nextState].minPathMetric = pathMetric;
          nextStates[output.nextState].fromState = state;
          nextStates[output.nextState].inputBit = possibleInput;
        }
        nextStates[output.nextState].isActivated = true;
      }
    }
  }

  int lastState = 0;
  int result = 0;
  for (int time = HEADER_SIZE - 1; 0 <= time; time--) {
    result |= paths[time][lastState].inputBit << (HEADER_SIZE - time - 2);
    lastState = paths[time][lastState].fromState;
  }
  return result;
}

void dec(void) {
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
  unsigned segmentCount = 0;
  unsigned maxSegmentCount = (1 << (SEGMENT_INDEX_SIZE * 2)) - 1;

  unsigned char *buffer =
      malloc(sizeof(unsigned char) * maxSegmentCount * bodySize);

  for (int readSegmentIdx = 0; readSegmentIdx < maxSegmentCount;
       readSegmentIdx++) {
    unsigned indexA = readSegmentIndex(sourceFile);
    if (indexA == -1) {
      segmentCount = readSegmentIdx;
      break;
    }
    for (int bodyIdx = 0; bodyIdx < bodySize; bodyIdx++) {
      unsigned char value = getc(sourceFile);
      buffer[indexA * bodySize + bodyIdx] = value;
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

int main(void) {
  FILE *sourceFile;
  if ((sourceFile = fopen("testdata", "r")) == NULL) {
    fprintf(stderr, "cannot open %s\n", SEQDATA);
    exit(1);
  }
  int result = decodeViterbi(sourceFile);
  printf("%d\n", result);
//  dec();
  return 0;
}
