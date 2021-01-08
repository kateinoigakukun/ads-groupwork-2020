#include "grpwk20.h"
#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#pragma GCC optimize("Ofast")

#define NP_LINES_LENGTH 5
#define NP_DIFF_THRESHOLD 3
#define PEEK_LENGTH 15

//#define LOG_DEBUG 1
//#define LOG_TRACE 1

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

#define ADS_FASTPATH(X) X

// MARK: - Generic utils
void reportError(char *context) {
  perror(context);
  exit(EXIT_FAILURE);
}

static inline int min3(int a, int b, int c) {
  int items[3] = {a, b, c};
  int minItem = a;
  for (int i = 1; i < 3; i++) {
    if (minItem > items[i]) {
      minItem = items[i];
    }
  }
  return minItem;
}

int max2(int a, int b) { return a > b ? a : b; }

bool isAllEqualArray(char *items, int length) {
  char head = items[0];
  for (int idx = 1; idx < length; idx++) {
    if (head != items[idx]) {
      return false;
    }
  }
  return true;
}

int maxIndexOfArray(int *items, int length) {
  int maxItem = items[0];
  int index = 0;
  for (int i = 1; i < length; i++) {
    if (maxItem < items[i]) {
      maxItem = items[i];
      index = i;
    }
  }
  return index;
}


int costTable[PEEK_LENGTH + 1][PEEK_LENGTH + 1];
int opTable[PEEK_LENGTH + 1][PEEK_LENGTH + 1];

// MARK: - Debug Support for edit distance
void dumpCostTable(char *base, int baseLength, char *target,
                   int targetLength) {
  printf("|   |");
  for (int y = 0; y < baseLength + 1; y++) {
    printf("  %2d  |", y);
  }
  printf("\n");

  printf("|   |   x  |");

  for (int y = 0; y < baseLength; y++) {
    printf("   %c  |", base[y]);
  }
  printf("\n");
  printf("│───┼──────┼");
  for (int y = 0; y < baseLength; y++) {
    printf("──────┼");
  }
  printf("\n");

  for (int x = 0; x < targetLength + 1; x++) {
    if (x == 0) {
      printf("| x |");
    } else {
      printf("| %c |", target[x - 1]);
    }
    for (int y = 0; y < baseLength + 1; y++) {
      if (costTable[x][y] == INT_MAX ||
          costTable[x][y] < 0) { // < 0 means uninitialized
        printf("    X |");
      } else {
        printf(" %4d |", costTable[x][y]);
      }
    }
    printf("\n");
  }
  printf("\n");
}


int peekingEditDistance(char *base, char *target) {
  int length = PEEK_LENGTH;
  costTable[0][0] = 0;
  for (int x = 1; x < length + 1; x++) {
    costTable[x][0] = x;
  }
  for (int y = 1; y < length + 1; y++) {
    costTable[0][y] = y;
  }

  for (int x = 1; x < length + 1; x++) {
    for (int y = 1; y < length + 1; y++) {
      int insertCost = costTable[x][y - 1] + 1;
      int removeCost = costTable[x - 1][y] + 1;
      int matchCost =
          base[y - 1] == target[x - 1] ? costTable[x - 1][y - 1] : INT_MAX;

      int minCost = min3(insertCost, removeCost, matchCost);
      costTable[x][y] = minCost;
    }
  }
  return costTable[length][length];
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
  int bsIndex;
} edit_op_t;

void dumpOpTable(char *bsBuffer,
                 int bsLength, char *npBuffer, int npLength) {
  printf("|   |");
  for (int y = 0; y < bsLength + 1; y++) {
    printf("  %2d  |", y);
  }
  printf("\n");

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
      if (opTable[x][y] == EDIT_OP_NONE) {
        printf("   X  |");
        continue;
      }
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

/// Returns the length of edit operations
int calculateEditOperations(char *bsBuffer, int bsLength,
                            char *npBuffer, int npLength,
                            edit_op_t *bestOps) {

  for (int x = 0; x < npLength + 1; x++) {
    costTable[x][0] = x;
    opTable[x][0] = EDIT_OP_REMOVE;
  }
  costTable[0][0] = 0;
  opTable[0][1] = EDIT_OP_INSERT | EDIT_OP_REMOVE;
  costTable[0][1] = 1;
  opTable[0][1] = EDIT_OP_INSERT;
  for (int y = 2; y < bsLength + 1; y++) {
    costTable[0][y] = y;
    opTable[0][y] = EDIT_OP_INSERT;
  }
  opTable[0][0] = EDIT_OP_NONE;

  for (int x = 1; x < npLength + 1; x++) {
    for (int y = 1; y < bsLength + 1; y++) {

      int insertCost = costTable[x][y - 1] + 1;
      int removeCost = costTable[x - 1][y] + 1;
      int matchCost = bsBuffer[y - 1] == npBuffer[x - 1] ? costTable[x - 1][y - 1] : INT_MAX;

      int minOpCost = min3(insertCost, removeCost, matchCost);
      edit_op_kind_t possibleOp = EDIT_OP_NONE;
      if (insertCost == minOpCost) {
        possibleOp |= EDIT_OP_INSERT;
      }

      if (removeCost == minOpCost) {
        possibleOp |= EDIT_OP_REMOVE;
      }

      if (matchCost == minOpCost) {
        possibleOp |= EDIT_OP_NOP;
      }
      assert(possibleOp != 0);
      costTable[x][y] = minOpCost;
      opTable[x][y] = possibleOp;
    }
  }

  int x = npLength;
  int y = bsLength;
  int opCount = 0;
  while (x > 0 || y > 0) {
    if ((opTable[x][y] & EDIT_OP_NOP) != 0) {
      bestOps[opCount] = (edit_op_t){
          .kind = EDIT_OP_NOP,
          .payload1 = npBuffer[x - 1],
          .bsIndex = y - 1,
      };
      ADS_DEBUG(assert(npBuffer[x - 1] == bsBuffer[y - 1]));
      opCount++;
      x--;
      y--;
    } else if ((opTable[x][y] & EDIT_OP_INSERT) != 0) {
      bestOps[opCount] = (edit_op_t){
          .kind = EDIT_OP_INSERT,
          .payload1 = bsBuffer[y - 1],
          .bsIndex = y - 1,
      };
      opCount++;
      y--;
    } else if ((opTable[x][y] & EDIT_OP_REMOVE) != 0) {
      bestOps[opCount] = (edit_op_t){
          .kind = EDIT_OP_REMOVE,
          .payload1 = npBuffer[x - 1],
          .bsIndex = y - 1,
      };
      opCount++;
      x--;
    } else {
      ADS_DEBUG(assert(false && "Invalid state"));
    }
  }
  return opCount;
}

void dumpEditOps(edit_op_t *ops, int opLength) {
  for (int i = opLength - 1; i >= 0; i--) {
    printf("edit[%2d][bs_index=%2d]: ", opLength - i - 1, ops[i].bsIndex);
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

// MARK: - En/Decoding Format
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

char encodeUInt(unsigned value) {
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

char decodeBit(char value) {
  return (value == BASE_A || value == BASE_T) ? '0' : '1';
}

// MARK: - Debug Support
char *originalBuffer;
void readEncodedBuffer() {
  int encdataFD;
  if ((encdataFD = open(ORGDATA, O_RDONLY, S_IRUSR)) < 0) {
    reportError(ORGDATA);
  }

  char *rawBuffer =
      mmap(NULL, ORGDATA_LEN, PROT_READ, MAP_PRIVATE, encdataFD, 0);
  originalBuffer = malloc(sizeof(char) * ORGDATA_LEN);
  for (int idx = 0; idx < ORGDATA_LEN; idx++) {
    originalBuffer[idx] = rawBuffer[idx];
  }
}

// MARK: - Reader

typedef struct {
  char *lines[NP_LINES_LENGTH];
  int lineCursors[NP_LINES_LENGTH];
  int lineLengths[NP_LINES_LENGTH];
  int outputCursor;
  char *outputBuffer;
  int outputFD;
} reader_state_t;

reader_state_t createReader() {
  int error;
  int sourceFD;
  if ((sourceFD = open(SEQDATA, O_RDONLY, S_IRUSR)) < 0) {
    reportError(SEQDATA);
  }

  struct stat sb;
  if ((error = fstat(sourceFD, &sb)) == -1) {
    reportError("fstat");
  }

  int outputFD;
  if ((outputFD = open(DECDATA, O_CREAT | O_APPEND | O_RDWR,
                       S_IRUSR | S_IWUSR)) == -1) {
    reportError(DECDATA);
  }

  // extend file size
  ftruncate(outputFD, ORGDATA_LEN + 1);

  reader_state_t state = {.outputCursor = 0};
  state.outputFD = outputFD;
  state.outputBuffer = mmap(NULL, ORGDATA_LEN + 1, PROT_READ | PROT_WRITE,
                            MAP_SHARED, outputFD, 0);
  if (state.outputBuffer == MAP_FAILED) {
    reportError("DECDATA MAP_FAILED");
  }

  char *buffer = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, sourceFD, 0);
  int cursor = 0;
  for (int line = 0; line < NP_LINES_LENGTH; line++) {
    state.lines[line] = buffer + cursor;
    int lineStart = cursor;
    while (buffer[cursor] != '\n') {
      cursor++;
    }
    state.lineLengths[line] = cursor - lineStart;
    cursor++; // skip '\n'
  }
  assert(cursor == sb.st_size);
  close(sourceFD);
  return state;
}

void finalizeReader(reader_state_t *state) {
  msync(state->outputBuffer, ORGDATA_LEN + 1, 0);
  munmap(state->outputBuffer, ORGDATA_LEN + 1);
  close(state->outputFD);
}

void dumpState(reader_state_t *state) {
  printf("============ Reader State ============\n");
  printf("Input lines:\n");
  for (int line = 0; line < NP_LINES_LENGTH; line++) {
    int baseCursor = state->lineCursors[line];
    for (int cursor = max2(baseCursor - 10, 0); cursor < baseCursor + 10;
         cursor++) {
      if (cursor == baseCursor) {
        printf("[%c]", state->lines[line][cursor]);
      } else {
        printf("%c", state->lines[line][cursor]);
      }
    }
    printf(" [length = %6d, cursor = %6d]\n", state->lineLengths[line],
           baseCursor);
  }

  {
    int baseCursor = state->outputCursor;
    printf("Output buffer: [cursor = %6d]\n", baseCursor);
    for (int cursor = max2(baseCursor - 10, 0); cursor < baseCursor; cursor++) {
      printf("%c", state->outputBuffer[cursor]);
    }
    printf("[x]xxxxxxxxx\n");
    for (int cursor = max2(baseCursor - 10, 0); cursor < baseCursor; cursor++) {
      char ch;
      if (cursor % 2 == 0) {
        ch = (state->outputBuffer[cursor] == '0') ? BASE_T : BASE_C;
      } else {
        ch = (state->outputBuffer[cursor] == '0') ? BASE_A : BASE_G;
      }
      printf("%c", ch);
    }
    printf("[x]xxxxxxxxx\n");
  }
  {
    printf("Expected buffer:\n");
    int baseCursor = state->outputCursor;
    for (int cursor = max2(baseCursor - 10, 0); cursor < baseCursor + 10;
         cursor++) {
      if (cursor == baseCursor) {
        printf("[%c]", originalBuffer[cursor]);
      } else {
        printf("%c", originalBuffer[cursor]);
      }
    }
    printf("\n");
    for (int cursor = max2(baseCursor - 10, 0); cursor < baseCursor + 10;
         cursor++) {
      char ch;
      if (cursor % 2 == 0) {
        ch = (originalBuffer[cursor] == '0') ? BASE_T : BASE_C;
      } else {
        ch = (originalBuffer[cursor] == '0') ? BASE_A : BASE_G;
      }
      if (cursor == baseCursor) {
        printf("[%c]", ch);
      } else {
        printf("%c", ch);
      }
    }
    printf("\n");
  }
}

void writeOutput(reader_state_t *state, char value) {
#ifdef LOG_DEBUG
  if (value != originalBuffer[state->outputCursor]) {
    dumpState(state);
    assert(false);
  }
#endif
  state->outputBuffer[state->outputCursor] = value;
  state->outputCursor++;
}

void readEachLineHead(reader_state_t *state, char *headState) {
  for (int line = 0; line < NP_LINES_LENGTH; line++) {
    int lineCursor = state->lineCursors[line];
    headState[line] = state->lines[line][lineCursor];
  }
}

void advanceLineCursors(reader_state_t *state) {
  for (int line = 0; line < NP_LINES_LENGTH; line++) {
    state->lineCursors[line]++;
  }
}

char estimateHeadBit(reader_state_t *state, char *heads) {
  int zeros = 0, ones = 0;
  for (int line = 0; line < NP_LINES_LENGTH; line++) {
    switch (heads[line]) {
    case BASE_A:
    case BASE_T: {
      if (heads[line] == (state->outputCursor % 2 ? BASE_A : BASE_T)) {
        zeros++;
      }
      break;
    }
    case BASE_G:
    case BASE_C: {
      if (heads[line] == (state->outputCursor % 2 ? BASE_G : BASE_C)) {
        ones++;
      }
      break;
    }
    default:
      continue;
    }
  }
#ifdef LOG_DEBUG
  assert(zeros != 0 || ones != 0);
#endif
  return zeros < ones ? '1' : '0';
}

// いまのカーソルの状態からPEEK_LENGTHだけ多数決で推定する
void estimatePeekingHeads(reader_state_t *state, bool *isValidLine,
                          char *buffer) {
  for (int offset = 0; offset < PEEK_LENGTH + 3; offset++) {
    bool isAllHeadsMismatched = true;
    int charCounts[4] = {};
    for (int line = 0; line < NP_LINES_LENGTH; line++) {
      if (!isValidLine[line]) {
        continue;
      }
      if (state->lineLengths[line] < state->lineCursors[line] + offset) {
        continue;
      }
      char currentChar = state->lines[line][state->lineCursors[line] + offset];
      isAllHeadsMismatched = false;
      charCounts[decodeUInt(currentChar)]++;
    }
    char picked = encodeUInt(maxIndexOfArray(charCounts, 4));
    if (isAllHeadsMismatched) {
      buffer[offset] = 'x';
    } else {
      buffer[offset] = picked;
    }
  }
}

int offsetFromEditOps(edit_op_t *ops, int opsLength) {
  int index = opsLength - 1;
  int offset = 0;
  while (index < opsLength && ops[index].kind != EDIT_OP_NOP) {
    switch (ops[index].kind) {
      case EDIT_OP_INSERT:
        offset -= 1;
        break;
      case EDIT_OP_REMOVE:
        offset += 1;
        break;
      default:
        ADS_DEBUG(assert(false));
        break;
    }
    index -= 1;
  }
  return offset;
}

void estimateHeadOffsets(reader_state_t *state, char bit, char *heads,
                         int *offsetsByLine) {
  bool isValidLine[NP_LINES_LENGTH];
  for (int line = 0; line < NP_LINES_LENGTH; line++) {
    bool isSameBit = bit == decodeBit(heads[line]);
    bool isUpperBit = heads[line] == BASE_T || heads[line] == BASE_C;
    bool isSameLoc = (state->outputCursor % 2 == 0) == isUpperBit;
    isValidLine[line] = isSameBit && isSameLoc;
  }
  char estimatedHeads[PEEK_LENGTH + NP_DIFF_THRESHOLD];
  estimatePeekingHeads(state, isValidLine, estimatedHeads);

  for (int line = 0; line < NP_LINES_LENGTH; line++) {
    edit_op_t bestOps[PEEK_LENGTH * 2];
    int opsLen = calculateEditOperations(estimatedHeads, PEEK_LENGTH,
                                         state->lines[line] + state->lineCursors[line], PEEK_LENGTH, bestOps);
    int bestOffset = offsetFromEditOps(bestOps, opsLen);
    offsetsByLine[line] = bestOffset;
  }
}

/// Reader main entrypoint
void adjustErrors(reader_state_t *state) {
  while (state->outputCursor < ORGDATA_LEN) {
    char heads[NP_LINES_LENGTH];
    readEachLineHead(state, heads);
    if (ADS_FASTPATH(isAllEqualArray(heads, NP_LINES_LENGTH))) {
      advanceLineCursors(state);
      writeOutput(state, decodeBit(heads[0]));
      continue;
    }

    char headBit = estimateHeadBit(state, heads);

    int offsetsByLine[NP_LINES_LENGTH] = {};
    estimateHeadOffsets(state, headBit, heads, offsetsByLine);
    for (int line = 0; line < NP_LINES_LENGTH; line++) {
      state->lineCursors[line] += offsetsByLine[line] + 1;
    }
    writeOutput(state, headBit);
  }
}

void dec(void) {

  ADS_DEBUG(readEncodedBuffer());
  reader_state_t state = createReader();
  adjustErrors(&state);
  state.outputBuffer[state.outputCursor] = '\n';
  finalizeReader(&state);

  return;
}

int main(void) {
  dec();
  return 0;
}
