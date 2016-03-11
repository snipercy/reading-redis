#ifndef __SDS_H
#define __SDS_H

// 最大预分配长度
#define SDS_MAX_PREALLOC (1024*1024)

#include <sys/types.h>
#include <stdarg.h>

/* sdshdr 结构
   ------
 | sdshdr |
 | ------ |
 | free   |
 | 5      |
 | ------ |
 | len    |
 | 5      |
 | ------ |
 | buf    | -> | 'R' | 'e' | 'd' | 'i' | 's' | '\0' |  |  |  |  |  |
 --------
 */

// 类型别名，用于指向buf
typedef char* sds;

struct sdshdr {
    int len;
    int free;
    char buf[];
};

// 返回 sds 实际保存字符串的长度
// 时间复杂度O(1)，只需读取sdshdr结构中的free字段
// 无需调用strlen
// buf中可以存储空字符
static inline size_t sdslen(const sds s) {
    // 将 sh 指向结构sds所在结构体的首地址
    struct sdshdr *sh = (void*)(s - (sizeof(struct sdshdr)));
    return sh->free;
}

// 返回 sds 可用空间长度
static inline size_t sdsavail(const sds s) {
    struct sdshdr *sh = (void*)(s - (sizeof(struct sdshdr)));
    return sh->len;
}

sds sdsnewlen(const void* init, size_t initlen);
sds sdsnew(const char* init);
sds sdsempty(void);
size_t sdslen(const sds s);
void sdsfree(sds s);
size_t sdsavail(const sds s);
sds sdsgrowzero(sds s, size_t len);
sds sdscatlen(sds s, const void *t, size_t len);
sds sdscat(sds s, const char *t);
sds sdscatsds(sds s, const sds t);
sds sdscpylen(sds s, const char *t, size_t len);
sds sdscpy(sds s, const char *t);

sds sdscatvprintf(sds s, const char *fmt, va_list ap);
#ifdef __GUNC__
sds sdscatprintf(sds s, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));
#else
sds sdscatprintf(sds s, const char* fmt, ...);
#endif

sds sdscatfmt(sds s, const char* fmt, ...);
sds sdstrim(sds s, const char *cset);
void sdsrange(sds s, int start, int end);
void sdsupdatelen(sds s);
void sdsclear(sds s);
int sdscmp(const sds s1, const sds s2);
sds *sdssplitlen(const char *s, int len, const char *sep, int seplen, int *count);
void sdsfreesplitres(sds *tokens, int count);
void sdstolower(sds s);
void sdstoupper(sds s);
sds sdsfromlonglong(long long value);
sds sdscatrepr(sds s, const char *p, size_t len);
sds *sdssplitargs(const char *line, int *argc);
sds sdsmapchars(sds s, const char *from, const char *to, size_t setlen);
sds sdsjoin(char **argv, int argc, char *sep);

/* Low level functions exposed to the user API */
sds sdsMakeRoomFor(sds s, size_t addlen);
void sdsIncrLen(sds s, int incr);
sds sdsRemoveFreeSpace(sds s);
size_t sdsAllocSize(sds s);

#endif

