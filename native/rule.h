#ifndef S14_RULE_H
#define S14_RULE_H
#include <stdint.h>

typedef struct { int type, owner; } S14Cell;
typedef struct { int allowed, reason, count, exact, reads; } S14Decision;
typedef S14Cell (*S14Lookup)(void *context, int tile);
enum { S14_ORIGINAL_REJECTED, S14_OUTSIDE_SCOPE, S14_CONTINUATION,
       S14_OCCUPIED, S14_LIMIT, S14_WITHIN, S14_INVALID };

int s14_neighbors(int tile, int width, int height, int output[6]);
S14Decision s14_evaluate(S14Lookup lookup, void *context, int width, int height,
                        int tile, int type, int owner, int original_allowed, int continuation);
__declspec(dllexport) void S14EvaluateBoard(const S14Cell *cells, int width, int height,
                        int tile, int type, int owner, int original_allowed,
                        int continuation, S14Decision *result);
#endif
