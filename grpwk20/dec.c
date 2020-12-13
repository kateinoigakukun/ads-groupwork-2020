#include "grpwk20.h"
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define LOG_DEBUG 1
//#define LOG_TRACE 1

#define SEGMENT_SIZE SR_SIZE
// x = 200000/(25 - log_4(x))
// log_4 (x) ≒ 7
#define SEGMENT_INDEX_SIZE 7
#define SEGMENT_CONSTRAINT_LENGTH 3
#define SEGMENT_INDEX_STATE_COUNT (SEGMENT_CONSTRAINT_LENGTH - 1) * 2
#define HEADER_SIZE (SEGMENT_INDEX_SIZE * 2 + SEGMENT_CONSTRAINT_LENGTH - 1)
#define SEGMENT_BODY_SIZE (SEGMENT_SIZE - HEADER_SIZE)

#define ADS_NOP                                                                \
  do {                                                                         \
  } while (false)
#define ADS_PRINT(X)                                                           \
  do {                                                                         \
    X;                                                                         \
  } while (false)

#ifdef LOG_DEBUG
#define ADS_DEBUG(X) ADS_PRINT(X)
#else
#define ADS_DEBUG(X) ADS_NOP
#endif

#ifdef LOG_TRACE
#define ADS_TRACE(X) ADS_PRINT(X)
#else
#define ADS_TRACE(X) ADS_NOP
#endif

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
    ADS_TRACE(fprintf(stderr, "Invalid input for decodeUInt: %d\n", value));
    return 0;
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

  return (transition_output_t){.output = (c1 << 1) + c0,
                               .nextState = nextState};
}

typedef struct {
  int fromState;
  int minPathMetric;
  int inputBit;
  bool isActivated;
} state_node_t;

/// See also http://caspar.hazymoon.jp/convolution/index.html
int decodeViterbi(FILE *fp) {
  state_node_t paths[HEADER_SIZE + 1][SEGMENT_INDEX_STATE_COUNT];
  for (int i = 0; i < HEADER_SIZE + 1; i++) {
    for (int j = 0; j < SEGMENT_INDEX_STATE_COUNT; j++) {
      paths[i][j].isActivated = false;
    }
  }
  paths[0][0].fromState = 0;
  paths[0][0].minPathMetric = 0;
  paths[0][0].isActivated = true;

  for (int time = 0; time < HEADER_SIZE; time++) {

    char inputBitChar = getc(fp);
    if (inputBitChar == '\n')
      return -1;
    int expectedOutput = decodeUInt(inputBitChar);

    for (int state = 0; state < SEGMENT_INDEX_STATE_COUNT; state++) {
      if (!paths[time][state].isActivated)
        continue;

      bool onlyAcceptZero = time >= SEGMENT_INDEX_SIZE * 2;
      for (int possibleInput = 0; possibleInput <= (onlyAcceptZero ? 0 : 1);
           possibleInput++) {

        transition_output_t output =
            convolutionTransition(possibleInput, state);

        int branchMetric = hammingDistance(expectedOutput, output.output);
        ADS_TRACE(printf("state = %d, nextState = %d possibleInput = %d, "
                         "branchMetric = %d\n",
                         state, output.nextState, possibleInput, branchMetric));
        int pathMetric = paths[time][state].minPathMetric + branchMetric;

        state_node_t *nextStates = paths[time + 1];
        bool shouldUpdate =
            (nextStates[output.nextState].isActivated) &&
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
  for (int time = HEADER_SIZE; HEADER_SIZE - 2 < time; time--) {
    lastState = paths[time][lastState].fromState;
  }
  for (int time = HEADER_SIZE - 2; 0 < time; time--) {
    ADS_TRACE(
        printf("result[%d] = %d\n", time, paths[time][lastState].inputBit));
    result |= paths[time][lastState].inputBit
              << ((SEGMENT_INDEX_SIZE * 2) - time);
    lastState = paths[time][lastState].fromState;
  }

  return result;
}

#define MAX_SEGMENT_INDEX ((1 << (SEGMENT_INDEX_SIZE * 2)) - 1)
#define SEGMENT_COUNT                                                          \
  (((ORGDATA_LEN / 2) + SEGMENT_BODY_SIZE - 1) / SEGMENT_BODY_SIZE)
#define BSBLOCK_SIZE (SEGMENT_COUNT * SEGMENT_BODY_SIZE) // 277800

void readBSBlock(FILE *sourceFile, unsigned char *bsBuffer) {
  for (int readSegmentIdx = 0; readSegmentIdx < SEGMENT_COUNT;
       readSegmentIdx++) {
    unsigned indexA = decodeViterbi(sourceFile);
    ADS_TRACE(printf("segments[%d]\n", indexA));

    for (int bodyIdx = 0; bodyIdx < SEGMENT_BODY_SIZE; bodyIdx++) {
      unsigned char value = getc(sourceFile);
      bsBuffer[indexA * SEGMENT_BODY_SIZE + bodyIdx] = value;
    }
  }
  if (getc(sourceFile) != '\n') {
    assert(false && "expect newline");
  }
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

  unsigned char bsBuffer[MAX_SEGMENT_INDEX * SEGMENT_BODY_SIZE];

  printf("SEGMENT_COUNT = %d\n", SEGMENT_COUNT);
  printf("MAX_SEGMENT_INDEX = %d\n", MAX_SEGMENT_INDEX);
  printf("MAX_SEGMENT_INDEX * SEGMENT_BODY_SIZE = %d\n",
         MAX_SEGMENT_INDEX * SEGMENT_BODY_SIZE);
  printf("BSBLOCK_SIZE = %d\n", BSBLOCK_SIZE);

  readBSBlock(sourceFile, bsBuffer);

  for (int index = 0; index < SEGMENT_COUNT * SEGMENT_BODY_SIZE; index++) {
    unsigned value = decodeUInt(bsBuffer[index]);
    fputc((value >> 1) + '0', outputFile);
    fputc((value & 0x1) + '0', outputFile);
  }

  fputc('\n', outputFile);

  fclose(sourceFile);
  fclose(outputFile);
  return;
}

int main(void) {
  dec();
  return 0;
}
