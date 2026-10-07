#include "rule.h"
#include <limits.h>

static int wall(int type) { return type == 9 || type == 10; }

int s14_neighbors(int tile, int width, int height, int output[6]) {
    static const int dx[6] = {-1,0,1,-1,0,1};
    static const int dy[2][6] = {{0,-1,0,1,1,1},{-1,-1,-1,0,1,0}};
    if (width <= 0 || height <= 0 || width > INT_MAX / height || tile < 0 || tile >= width * height) return 0;
    int x = tile % width, y = tile / width, n = 0;
    for (int i=0;i<6;i++) {
        int nx=x+dx[i], ny=y+dy[x&1][i];
        if (nx>=0 && nx<width && ny>=0 && ny<height) output[n++]=ny*width+nx;
    }
    return n;
}

S14Decision s14_evaluate(S14Lookup lookup, void *context, int width, int height,
                        int tile, int type, int owner, int original_allowed, int continuation) {
    S14Decision result = {0,S14_INVALID,-1,0,0};
    if (!lookup || width<=0 || height<=0 || width>INT_MAX/height || tile<0 || tile>=width*height || owner<0 || owner>255) return result;
    if (!original_allowed) { result.reason=S14_ORIGINAL_REJECTED; return result; }
    if (!wall(type)) { result.allowed=1; result.reason=S14_OUTSIDE_SCOPE; return result; }
    S14Cell existing=lookup(context,tile);
    if (continuation) {
        if (existing.type!=type || existing.owner!=owner) return result;
        result.allowed=1; result.reason=S14_CONTINUATION; return result;
    }
    if (wall(existing.type)) { result.reason=S14_OCCUPIED; return result; }
    // The original game may allow replacing a non-wall facility. Preserve it.
    int seen[31]={tile}, seen_count=1, pending[5]={tile}, pending_count=1;
    result.count=1;
    while (pending_count) {
        int adjacent[6], n=s14_neighbors(pending[--pending_count],width,height,adjacent);
        for (int i=0;i<n;i++) {
            int duplicate=0;
            for (int j=0;j<seen_count;j++) if (seen[j]==adjacent[i]) { duplicate=1; break; }
            if (duplicate) continue;
            if (seen_count>=31) return (S14Decision){0,S14_INVALID,-1,0,result.reads};
            seen[seen_count++]=adjacent[i]; result.reads++;
            S14Cell cell=lookup(context,adjacent[i]);
            if (cell.owner!=owner || !wall(cell.type)) continue;
            if (++result.count>5) { result.reason=S14_LIMIT; return result; }
            pending[pending_count++]=adjacent[i];
        }
    }
    result.allowed=1; result.reason=S14_WITHIN; result.exact=1;
    return result;
}

typedef struct { const S14Cell *cells; } Board;
static S14Cell board_lookup(void *context,int tile) { return ((Board*)context)->cells[tile]; }

void S14EvaluateBoard(const S14Cell *cells, int width, int height, int tile,
                     int type, int owner, int original_allowed, int continuation, S14Decision *result) {
    if (!result) return;
    if (!cells) { *result=(S14Decision){0,S14_INVALID,-1,0,0}; return; }
    Board board={cells};
    *result=s14_evaluate(board_lookup,&board,width,height,tile,type,owner,original_allowed,continuation);
}
