#include "pinyin.h"
#include <string.h>
#include <wctype.h>
#include "vendor/pinyin/readings.h"
static const char *readings(wchar_t code) {
    size_t a=0,z=sizeof(reading_index)/sizeof(reading_index[0]);
    while (a<z) { size_t m=a+(z-a)/2;if (reading_index[m].code<(unsigned int)code) a=m+1;else z=m; }
    if (a<sizeof(reading_index)/sizeof(reading_index[0]) && reading_index[a].code==(unsigned int)code) return reading_pool+reading_index[a].offset;
    return NULL;
}
static wchar_t normal(wchar_t c) { c=(wchar_t)towlower(c);return c==L'ü' || c==L'Ü'?L'v':c; }
static int equal(wchar_t c,char p) { c=normal(c);return c==(wchar_t)p || (p=='v' && c==L'u'); }
int s14_search_contains(const wchar_t *text,const wchar_t *query,size_t length) {
    if (!length) return 1;if (length>127 || !text || !query) return 0;
    /* Each character adds a syllable (or initial). DP avoids enumerating the
       exponential combinations of polyphonic names. Restart at each character
       boundary to support substring search, but do not cross punctuation. */
    for (int initials=0;initials<2;initials++) {
        unsigned char state[128]={0},next[128];
        for (const wchar_t *at=text;*at;at++) {
            state[0]=1;memset(next,0,sizeof(next));const char *variants=readings(*at);
            for (size_t q=0;q<length;q++) if (state[q]) {
                if (normal(*at)==normal(query[q])) { if (q+1==length) return 1;next[q+1]=1; }
                if (!variants) continue;
                const char *syllable=variants;
                while (*syllable) {
                    const char *end=syllable;while (*end && *end!='|') end++;
                    size_t count=initials?1:(size_t)(end-syllable),matched=0;
                    while (matched<count && q+matched<length && equal(query[q+matched],syllable[matched])) matched++;
                    if (q+matched==length) return 1;
                    if (matched==count) next[q+matched]=1;
                    syllable=*end?end+1:end;
                }
            }
            memcpy(state,next,sizeof(state));
        }
    }
    return 0;
}
