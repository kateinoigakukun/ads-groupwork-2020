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

// MARK: - Debug Support for edit distance
void dumpCostTable(int **costTable, char *base, int baseLength, char *target,
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
  int costTable[PEEK_LENGTH][PEEK_LENGTH];
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

typedef enum { ENCODED_BIT_ZERO, ENCODED_BIT_ONE } encoded_bit_t;

encoded_bit_t decodeBit(char value) {
  return (value == BASE_A || value == BASE_T) ? ENCODED_BIT_ZERO
                                              : ENCODED_BIT_ONE;
}

// MARK: - Debug Support
encoded_bit_t *originalBuffer;
void readEncodedBuffer() {
  int encdataFD;
  if ((encdataFD = open(ORGDATA, O_RDONLY, S_IRUSR)) < 0) {
    reportError(ORGDATA);
  }

  char *rawBuffer =
      mmap(NULL, ORGDATA_LEN, PROT_READ, MAP_PRIVATE, encdataFD, 0);
  originalBuffer = malloc(sizeof(encoded_bit_t) * ORGDATA_LEN);
  for (int idx = 0; idx < ORGDATA_LEN; idx++) {
    switch (rawBuffer[idx]) {
    case '0':
      originalBuffer[idx] = ENCODED_BIT_ZERO;
      break;
    case '1':
      originalBuffer[idx] = ENCODED_BIT_ONE;
      break;
    }
  }
}

// MARK: - Reader

typedef struct {
  char *lines[NP_LINES_LENGTH];
  int lineCursors[NP_LINES_LENGTH];
  int lineLengths[NP_LINES_LENGTH];
  int outputCursor;
  encoded_bit_t *outputBuffer;
} reader_state_t;

reader_state_t createReader() {
  int sourceFD;
  if ((sourceFD = open(SEQDATA, O_RDONLY, S_IRUSR)) < 0) {
    reportError(SEQDATA);
  }

  struct stat sb;
  if (fstat(sourceFD, &sb) == -1) {
    reportError("fstat");
  }

  reader_state_t state = {.outputCursor = 0};
  state.outputBuffer = malloc(sizeof(encoded_bit_t) * ORGDATA_LEN);
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
      printf("%1d", state->outputBuffer[cursor]);
    }
    printf("[x]xxxxxxxxx\n");
    for (int cursor = max2(baseCursor - 10, 0); cursor < baseCursor; cursor++) {
      char ch;
      if (cursor % 2 == 0) {
        ch =
            (state->outputBuffer[cursor] == ENCODED_BIT_ZERO) ? BASE_T : BASE_C;
      } else {
        ch =
            (state->outputBuffer[cursor] == ENCODED_BIT_ZERO) ? BASE_A : BASE_G;
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
        printf("[%d]", originalBuffer[cursor]);
      } else {
        printf("%d", originalBuffer[cursor]);
      }
    }
    printf("\n");
    for (int cursor = max2(baseCursor - 10, 0); cursor < baseCursor + 10;
         cursor++) {
      char ch;
      if (cursor % 2 == 0) {
        ch = (originalBuffer[cursor] == ENCODED_BIT_ZERO) ? BASE_T : BASE_C;
      } else {
        ch = (originalBuffer[cursor] == ENCODED_BIT_ZERO) ? BASE_A : BASE_G;
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

void writeOutput(reader_state_t *state, encoded_bit_t value) {
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

encoded_bit_t estimateHeadBit(reader_state_t *state, char *heads) {
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
  return zeros < ones ? ENCODED_BIT_ONE : ENCODED_BIT_ZERO;
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

void estimateHeadOffsets(reader_state_t *state, encoded_bit_t bit, char *heads,
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
  // headDeletedBases[0] = 本来あるはずの先頭0個が消えた状態 (そのまま)
  // headDeletedBases[1] = 本来あるはずの先頭1個が消えた状態
  // ...
  char headDeletedBases[NP_DIFF_THRESHOLD + 1][PEEK_LENGTH + 1];
  for (int diff = 0; diff < NP_DIFF_THRESHOLD + 1; diff++) {
    for (int offset = 0; offset < PEEK_LENGTH; offset++) {
      headDeletedBases[diff][offset] = estimatedHeads[offset + diff];
    }
    headDeletedBases[diff][PEEK_LENGTH] = '\0';
  }

  for (int line = 0; line < NP_LINES_LENGTH; line++) {
    if (isValidLine[line]) {
      offsetsByLine[line] = 0;
      continue;
    }
    // headInsertedTargets[0] = 本来無いはずの先頭0個が消えた状態 (そのまま)
    // headInsertedTargets[1] = 本来無いはずの先頭1個が消えた状態
    // ...
    char headInsertedTargets[NP_DIFF_THRESHOLD + 1][PEEK_LENGTH + 1];
    for (int diff = 0; diff < NP_DIFF_THRESHOLD + 1; diff++) {
      int baseCursor = state->lineCursors[line] + diff;
      for (int offset = 0; offset < PEEK_LENGTH; offset++) {
        headInsertedTargets[diff][offset] =
            state->lines[line][baseCursor + offset];
      }
      headInsertedTargets[diff][PEEK_LENGTH] = '\0';
    }

    // Check insertion
    int bestOffset = INT_MAX;
    int minCost = INT_MAX;
    for (int diff = 1; diff < NP_DIFF_THRESHOLD + 1; diff++) {
      int cost = peekingEditDistance(headDeletedBases[0], headInsertedTargets[diff]);
      if (minCost > cost) {
        minCost = cost;
        bestOffset = diff;
      }
    }

    // Check deletion
    for (int diff = 1; diff < NP_DIFF_THRESHOLD + 1; diff++) {
      int cost = peekingEditDistance(headDeletedBases[diff], headInsertedTargets[0]);
      if (minCost > cost) {
        minCost = cost;
        bestOffset = -diff;
      }
    }
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

    encoded_bit_t headBit = estimateHeadBit(state, heads);

    int offsetsByLine[NP_LINES_LENGTH] = {};
    estimateHeadOffsets(state, headBit, heads, offsetsByLine);
    for (int line = 0; line < NP_LINES_LENGTH; line++) {
      state->lineCursors[line] += offsetsByLine[line] + 1;
    }
    writeOutput(state, headBit);
  }
}

void dec(void) {

  FILE *outputFile;
  if ((outputFile = fopen(DECDATA, "w")) == NULL) {
    fprintf(stderr, "cannot open %s\n", DECDATA);
    exit(1);
  }

  ADS_DEBUG(readEncodedBuffer());
  reader_state_t state = createReader();
  adjustErrors(&state);

  for (int idx = 0; idx < ORGDATA_LEN; idx++) {
    switch (state.outputBuffer[idx]) {
    case ENCODED_BIT_ZERO:
      fputc('0', outputFile);
      break;
    case ENCODED_BIT_ONE:
      fputc('1', outputFile);
      break;
    }
  }
  fputc('\n', outputFile);

  fclose(outputFile);
  return;
}

int main(void) {
  dec();
  return 0;
}
