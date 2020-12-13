#include "grpwk20.h"
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define LOG_DEBUG 1
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
    ADS_DEBUG(printf("segments[%d]\n", indexA));

    for (int bodyIdx = 0; bodyIdx < SEGMENT_BODY_SIZE; bodyIdx++) {
      unsigned char value = getc(sourceFile);
      bsBuffer[indexA * SEGMENT_BODY_SIZE + bodyIdx] = value;
    }
  }
}

static inline int min4(int a, int b, int c, int d) {
  int items[4] = {a, b, c, d};
  int minItem = a;
  for (int i = 1; i < 4; i++) {
    if (minItem > items[i]) {
      minItem = items[i];
    }
  }
  return minItem;
}

typedef enum {
  EDIT_OP_NONE = 0,
  EDIT_OP_INSERT = 1,
  EDIT_OP_REMOVE = 1 << 1,
  EDIT_OP_SUBST = 1 << 2,
  EDIT_OP_NOP = 1 << 3
} edit_op_kind_t;

typedef struct {
  edit_op_kind_t kind;
  unsigned char payload1;
  unsigned char payload2;
} edit_op_t;

void dumpOpTable(edit_op_kind_t **opTable, unsigned char *bsBuffer,
                 int bsLength, unsigned char *npBuffer, int npLength) {
  printf("|   |   x  |");

  for (int y = 0; y < bsLength; y++) {
    printf("   %c  |", bsBuffer[y]);
  }
  printf("\n");
  printf("│───┼──────┼");
  for (int y = 0; y < bsLength; y++) {
    printf("──────┼");
  }
  printf("\n");

  for (int x = 0; x < npLength + 1; x++) {
    if (x == 0) {
      printf("| x |");
    } else {
      printf("| %c |", npBuffer[x - 1]);
    }
    for (int y = 0; y < bsLength + 1; y++) {
      char symbol[4] = {0};
      int symbolCount = 0;
      if ((opTable[x][y] & EDIT_OP_INSERT) != 0) {
        symbol[symbolCount] = 'I';
        symbolCount++;
      }
      if ((opTable[x][y] & EDIT_OP_REMOVE) != 0) {
        symbol[symbolCount] = 'R';
        symbolCount++;
      }
      if ((opTable[x][y] & EDIT_OP_SUBST) != 0) {
        symbol[symbolCount] = 'S';
        symbolCount++;
      }
      if ((opTable[x][y] & EDIT_OP_NOP) != 0) {
        symbol[symbolCount] = 'N';
        symbolCount++;
      }
      printf(" %4s |", symbol);
    }
    printf("\n");
  }
  printf("\n");
}

void dumpCostTable(int **costTable, unsigned char *bsBuffer, int bsLength,
                   unsigned char *npBuffer, int npLength) {
  printf("|   |   x  |");

  for (int y = 0; y < bsLength; y++) {
    printf("   %c  |", bsBuffer[y]);
  }
  printf("\n");
  printf("│───┼──────┼");
  for (int y = 0; y < bsLength; y++) {
    printf("──────┼");
  }
  printf("\n");

  for (int x = 0; x < npLength + 1; x++) {
    if (x == 0) {
      printf("| x |");
    } else {
      printf("| %c |", npBuffer[x - 1]);
    }
    for (int y = 0; y < bsLength + 1; y++) {
      printf(" %4d |", costTable[x][y]);
    }
    printf("\n");
  }
  printf("\n");
}

/// Returns the length of edit operations
int calculateEditOperations(unsigned char *bsBuffer, int bsLength,
                            unsigned char *npBuffer, int npLength,
                            edit_op_t *bestOps) {
  // Phase 0
  /*
   |   | x | A | T | A | C | G | ... Y (Input from BS)
   │───┼───┼───┼───┼───┼───┼───┼────
   | x |   |   |   |   |   |   | ...
   | T |   |   |   |   |   |   | ...
   | A |   |   |   |   |   |   | ...
   | C |   |   |   |   |   |   | ...
   | C |   |   |   |   |   |   | ...
   | . |
   | . |
   | . |
     X
(Input from NP)
  */
  int **costTable = malloc(sizeof(int *) * (npLength + 1));
  for (int i = 0; i < npLength + 1; i++) {
    costTable[i] = malloc(sizeof(int) * (bsLength + 1));
  }

  edit_op_kind_t **opTable = malloc(sizeof(int *) * (npLength + 1));
  for (int i = 0; i < npLength + 1; i++) {
    opTable[i] = malloc(sizeof(int) * (bsLength + 1));
  }

  // Phase 1
  /*
   |   | x | A | T | A | C | G | ... Y (Input from BS)
   │───┼───┼───┼───┼───┼───┼───┼────
   | x | 0 | 1 | 2 | 3 | 4 | 5 | ...
   | T | 1 |   |   |   |   |   | ...
   | A | 2 |   |   |   |   |   | ...
   | C | 3 |   |   |   |   |   | ...
   | C | 4 |   |   |   |   |   | ...
   | . |
   | . |
   | . |
     X
(Input from NP)
  */
  for (int x = 0; x < npLength + 1; x++) {
    costTable[x][0] = x;
    opTable[x][0] = EDIT_OP_REMOVE;
  }
  for (int y = 0; y < bsLength + 1; y++) {
    costTable[0][y] = y;
    opTable[0][y] = EDIT_OP_INSERT;
  }
  // Phase 2
  /*
   |   | x | C | A |    A  | T | G | ... Y (Input from BS)
   │───┼───┼───┼───┼───────┼───┼───┼────
   | x | 0 | 1 | 2 |   3   | 4 | 5 | ...
   | T | 1 | 1 | 2 |   3   |   |   | ...
   |   |   |   |   \3  ↓ 2 |   |   | ...
   | A | 2 | 2 | 1 ┼>1 1   |   |   | ...
   | G | 3 |   |   |       |   |   | ...
   | C | 4 |   |   |       |   |   | ...
   | . |
   | . |
   | . |
     X
(Input from NP)

   *1. TAをCAAにするためのコストを求めたい TODO
      - 1. TAをCAにしてAを挿入する          (追加コスト0)
        2. TAのTをCAAにして末尾のAを削除する (追加コスト1)
        3. TをCAにしてAをAに置換する        (追加コスト0)
      - CAの後ろにAを挿入するコストは、NP向けにエンコードする前に落とした
        連続文字の復元なのでコストは0とする。
      - よって1の操作のコストは1。これは通常の編集距離の定義とは違うので注意。
  */
  ADS_DEBUG(printf("calculateEditOperations npLength=%d\n", npLength));
  ADS_DEBUG(printf("calculateEditOperations bsLength=%d\n", bsLength));
  for (int x = 1; x < npLength + 1; x++) {
    ADS_DEBUG(if (x % 1000 == 0) printf("calculateEditOperations x=%d\n", x));
    for (int y = 1; y < bsLength + 1; y++) {
      ADS_DEBUG(if (y % 1000 == 0) printf("calculateEditOperations y=%d\n", y));
      int insertCost = costTable[x][y - 1] + 1;
      int removeCost = costTable[x - 1][y] + 1;
      int substCost = costTable[x - 1][y - 1] + 1;
      int matchCost = INT_MAX;

      if (bsBuffer[y - 1] == npBuffer[x - 1]) {
        matchCost = costTable[x - 1][y - 1];
      }

      int minOpCost = min4(insertCost, removeCost, substCost, matchCost);
      edit_op_kind_t possibleOp = EDIT_OP_NONE;
      if (insertCost == minOpCost) {
        possibleOp |= EDIT_OP_INSERT;
      }
      if (removeCost == minOpCost) {
        possibleOp |= EDIT_OP_REMOVE;
      }
      if (substCost == minOpCost) {
        possibleOp |= EDIT_OP_SUBST;
      }
      if (matchCost == minOpCost) {
        possibleOp |= EDIT_OP_NOP;
      }
      costTable[x][y] = minOpCost;
      opTable[x][y] = possibleOp;
    }
  }

  dumpOpTable(opTable, bsBuffer, bsLength, npBuffer, npLength);
  dumpCostTable(costTable, bsBuffer, bsLength, npBuffer, npLength);
  int x = npLength;
  int y = bsLength;
  int opCount = 0;
  while (x > 0 || y > 0) {
    if ((opTable[x][y] & EDIT_OP_NOP) != 0) {
      bestOps[opCount] = (edit_op_t){
          .kind = EDIT_OP_NOP,
          .payload1 = npBuffer[x - 1],
      };
      assert(npBuffer[x - 1] == bsBuffer[y - 1]);
      opCount++;
      x--;
      y--;
    } else if ((opTable[x][y] & EDIT_OP_INSERT) != 0) {
      bestOps[opCount] = (edit_op_t){
          .kind = EDIT_OP_INSERT,
          .payload1 = bsBuffer[y - 1],
      };
      opCount++;
      y--;
    } else if ((opTable[x][y] & EDIT_OP_SUBST) != 0) {
      bestOps[opCount] = (edit_op_t){
          .kind = EDIT_OP_SUBST,
          .payload1 = npBuffer[x - 1],
          .payload2 = bsBuffer[y - 1],
      };
      opCount++;
      x--;
      y--;
    } else if ((opTable[x][y] & EDIT_OP_REMOVE) != 0) {
      bestOps[opCount] = (edit_op_t){
          .kind = EDIT_OP_REMOVE,
          .payload1 = npBuffer[x - 1],
      };
      opCount++;
      x--;
    } else {
      assert(false && "Invalid state");
    }
  }
  free(opTable);
  free(costTable);
  return opCount;
}

#define NPBLOCK_MAX_SIZE (ORGDATA_LEN / 2)
/// Returns the length of NP block
int readNPBlock(FILE *sourceFile, unsigned char *npBuffer) {
  int npLength = 0;

  {
    unsigned char loaded = getc(sourceFile);
    npBuffer[npLength] = loaded;

    while (loaded != '\n') {
      npLength++;
      int newInput = getc(sourceFile);
      if (loaded == newInput) {
        continue;
      }
      npBuffer[npLength] = newInput;
      loaded = newInput;
    };
  }
  return npLength;
}

void dumpEditOps(edit_op_t *ops, int opLength) {
  for (int i = opLength - 1; i >= 0; i--) {
    printf("edit[%d]: ", opLength - i - 1);
    switch (ops[i].kind) {
    case EDIT_OP_SUBST:
      printf("subst '%c' with '%c'\n", ops[i].payload1, ops[i].payload2);
      break;
    case EDIT_OP_NOP:
      printf("nop '%c'\n", ops[i].payload1);
      break;
    case EDIT_OP_INSERT:
      printf("insert '%c'\n", ops[i].payload1);
      break;
    case EDIT_OP_REMOVE:
      printf("remove '%c'\n", ops[i].payload1);
      break;
    default:
      break;
    }
  }
}

void dec(void) {
  FILE *sourceFile;
  if ((sourceFile = fopen(ENCDATA, "r")) == NULL) {
    fprintf(stderr, "cannot open %s\n", SEQDATA);
    exit(1);
  }

  FILE *outputFile;
  if ((outputFile = fopen(DECDATA, "w")) == NULL) {
    fprintf(stderr, "cannot open %s\n", DECDATA);
    exit(1);
  }

  unsigned char bsBuffer[MAX_SEGMENT_INDEX * SEGMENT_BODY_SIZE];
  unsigned char npBuffer[NPBLOCK_MAX_SIZE];

  printf("SEGMENT_COUNT = %d\n", SEGMENT_COUNT);
  printf("MAX_SEGMENT_INDEX = %d\n", MAX_SEGMENT_INDEX);
  printf("MAX_SEGMENT_INDEX * SEGMENT_BODY_SIZE = %d\n",
         MAX_SEGMENT_INDEX * SEGMENT_BODY_SIZE);
  printf("BSBLOCK_SIZE = %d\n", BSBLOCK_SIZE);

  int bsLength = BSBLOCK_SIZE;
  readBSBlock(sourceFile, bsBuffer);
  int npLength = readNPBlock(sourceFile, npBuffer);

  edit_op_t bestOps[bsLength + npLength];
  printf("bsBuffer[0..<15] = %.15s\n", bsBuffer);
  printf("npBuffer[0..<15] = %.15s\n", npBuffer);
  int opLength = calculateEditOperations(bsBuffer, 15, npBuffer, 15, bestOps);
  ADS_DEBUG(dumpEditOps(bestOps, opLength));

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
