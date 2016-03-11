#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "sds.h"
#include "zmalloc.h"

// 根据给定的初始化字符串 init 和 字符串长度initlen，创建sdshdr
// 返回结构体重buf首地址
// 失败返回NULL
sds sdsnewlen(const void *init, size_t initlen) {
    struct sdshdr *sh;

    if (init) {
        // zmalloc 调用 malloc ，不初始化所分配内存
        sh = zmalloc(sizeof(struct sdshdr) + initlen + 1);
    } else {
        // zcalloc 将分配的内存初始化为0
        sh = zcalloc(sizeof(struct sdshdr) + initlen + 1);
    }

    // 内存分配失败
    if (sh == NULL) return NULL;

    sh->len = initlen;

    // 新sds不预留任何空间
    sh->free = 0;

    if (initlen && init) {
        memcpy(sh->buf, init, initlen);
    }

    // 以 \0 结尾，兼容c语言中的字符串，这样可以使用c标准库中字符串处理函数
    sh->buf[initlen] = '\0';

    // 返回buf部分，而不是整个sdshdr
    return (char*)sh->buf;
}

sds sdsempty(void) {
    return sdsnewlen("",0);
}

sds sdsnew(const char *init) {
    size_t initlen = (init == NULL) ? 0 : strlen(init);
    return sdsnewlen(init, initlen);
}

sds sdsdup(const sds s) {
    return sdsnewlen(s, strlen(s));
}

void sdsfree(sds s) {
    if (s == NULL) return;
    zfree(s - sizeof(struct sdshdr));
}

void sdsupdatelen(sds s) {
    struct sdshdr *sh = (void*)(s - (sizeof(struct sdshdr)));
    int reallen = strlen(s);
    sh->free += (sh->len - reallen);
    sh->len = reallen;
}


// 不释放 sds 字符串空间，重置sds保存的字符串为空字符串
void sdsclear(sds s) {
    // 取出 sdshdr
    struct sdshdr *sh = (void*)(s - (sizeof(struct sdshdr)));

    sh->free += sh->len;
    sh->len = 0;

    sh->buf[0] = '\0';
}


// 添加存储空间
// 以前存储的字符串不变
sds sdsMakeRoomFor(sds s, size_t addlen) {
    struct sdshdr *sh, *newsh;

    // 获取剩余空间长度
    size_t free = sdsavail(s);

    size_t len, newlen;
    if (free >= addlen) return s;

    // 目前已占用空间的长度
    len = sdslen(s);
    sh = (void*)(s - (sizeof(struct sdshdr)));

    // s最少需要的长度
    newlen = addlen + len;
    if (newlen < SDS_MAX_PREALLOC) {
        newlen *= 2;
    } else {
        newlen += SDS_MAX_PREALLOC;
    }

    newsh = zrealloc(sh, sizeof(struct sdshdr) + newlen + 1);

    if (newsh == NULL) return NULL;

    newsh->free = newlen - len;

    return newsh->buf;
}


// 回收sds中空闲空间
// 回收时不会对sds中保存的字符串进行修改
sds sdsRemoveFreeSpace(sds s) {
    struct sdshdr *sh;

    sh = (void*)(s - sizeof(struct sdshdr));

    sh = zrealloc(sh, sizeof(struct sdshdr) + sh->len + 1);

    sh->free = 0;

    return sh->buf;
}


/* Return the total size of the allocation of the specifed sds string,
 * including:
 * 1) The sds header before the pointer.
 * 2) The string.
 * 3) The free buffer at the end if any.
 * 4) The implicit null term.
 */
size_t sdsAllocSize(sds s) {
    struct sdshdr *sh = (void*)(s - sizeof(struct sdshdr));
    return sizeof(*sh) + sh->len + sh->free + 1;
}


/* Increment the sds length and decrements the left free space at the
 * end of the string according to 'incr'. Also set the null term
 * in the new end of the string.
 *
 * This function is used in order to fix the string length after the
 * user calls sdsMakeRoomFor(), writes something after the end of
 * the current string, and finally needs to set the new length.
 *
 * Note: it is possible to use a negative increment in order to
 * right-trim the string.
 *
 * Usage example:
 *
 * Using sdsIncrLen() and sdsMakeRoomFor() it is possible to mount the
 * following schema, to cat bytes coming from the kernel to the end of an
 * sds string without copying into an intermediate buffer:
 *
 * oldlen = sdslen(s);
 * s = sdsMakeRoomFor(s, BUFFER_SIZE);
 * nread = read(fd, s+oldlen, BUFFER_SIZE);
 * ... check for nread <= 0 and handle it ...
 * sdsIncrLen(s, nread);
 */
void sdsIncrLen(sds s, int incr) {
    struct sdshdr *sh = (void*)(s - sizeof(struct sdshdr));

    assert(sh->free >= incr);

    sh->len += incr;
    sh->free -= incr;

    assert(sh->free >= 0);

    s[sh->len] = '\0';
}


/* Grow the sds to have the specified length. Bytes that were not part of
 * the original length of the sds will be set to zero.
 *
 * if the specified length is smaller than the current length, no operation
 * is performed.
 */
sds sdsgroezero(sds s, size_t len) {
    struct sdshdr *sh = (void*)(s - (sizeof(struct sdshdr)));
    size_t totlen, curlen = sh->len;

    if (len <= curlen) return s;

    s = sdsMakeRoomFor(s, len-curlen);

    // 内存不足
    if (s == NULL) return NULL;

    // 将重新分配的空间用0填充，防止出现垃圾内容
    sh = (void*)(s - sizeof(struct sdshdr));
    memset(s + curlen, 0, (len - curlen + 1)); // also set trailing \0 byte

    totlen = sh->len + sh->free;
    sh->len = len;
    sh->free = totlen - sh->len;

    return s;
}


/* Append the specified binary-safe string pointed by 't' of 'len' bytes to the
 * end of the specified sds string 's'.
 *
 * After the call, the passed sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
sds sdscatlen(sds s, const void *t, size_t len) {
    struct sdshdr *sh;

    size_t curlen = strlen(s);

    s = sdsMakeRoomFor(s, len);

    if (s == NULL) return NULL;

    sh = (void*)(sh - sizeof(struct sdshdr));
    memcpy(s + curlen, t, len);

    sh->len = curlen + len;
    sh->free = sh->free - len;

    s[curlen + len] = '\0';

    return s;
}


/* Append the specified null termianted C string to the sds string 's'.
 *
 * After the call, the passed sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 */
sds sdscat(sds s, const char *t) {
    return sdscatlen(s, t, strlen(t));
}

sds sdscatsds(sds s, const sds t) {
    return sdscatlen(s, t, sdslen(t));
}


// 将字符串t的前len个字符拷贝到s中，并以\0终结
// s中原内容被覆盖
sds sdscpylen(sds s, const char* t, size_t len) {
    struct sdshdr *sh = (void*)(s - sizeof(struct sdshdr));

    size_t totlen = sh->len + sh->free;

    if (totlen < len) {
        s = sdsMakeRoomFor(s, len - sh->len);
        if (s == NULL) return NULL;
        sh = (void*)(s - sizeof(struct sdshdr));
        totlen = sh->free + sh->len;
    }

    memcpy(s, t, len);

    s[len] = '\0';

    sh->len = len;
    sh->free = totlen - len;

    return s;
}


// 将字符串复制到sds中
// 覆盖原有字符
sds sdscpy(sds s, const char* t) {
    return sdscpylen(s, t, strlen(t));
}


/* Helper for sdscatlonglong() doing the actual number -> string
 * conversion. 's' must point to a string with room for at least
 * SDS_LLSTR_SIZE bytes.
 *
 * The function returns the lenght of the null-terminated string
 * representation stored at 's'.
 */
#define SDS_LLSTR_SIZE 21
int sdsll2str(char *s, long long value) {

    char *p = NULL;
    char aux;
    unsigned long long v;
    size_t l;

    v = (value < 0) ? -value : value;
    p = s;

    do {
        *p++ = (v % 10) + '0';
        v /= 10;
    } while(v);

    if (value < 0) *p++ = '-';

    // compute length and add null trem
    l = p - s;
    *p = '\0';

    // reverser the string
    p--;
    while (s < p) {
        aux = *s;
        *s = *p;
        *p = aux;
        s++;
        p--;
    }

    return l;
}


// Identical sdsll2str(), but for unsigned long long type
int sdsull2str(char *s, unsigned long long v) {
    char *p = NULL;
    char aux;
    size_t l;

    p = s;
    do {
        *p++ = (v % 10) + '0';
        v /= 10;
    } while(v);

    l = p - s;
    *p = '\0';

    // reverse the string
    p--;
    while (s < p) {
        aux = *s;
        *s = *p;
        *p = aux;
        s++;
        p--;
    }

    return l;
}


// create an sds string from a long long value. It is much faster than
// sdscatprintf(sdsempty(), "%lld\n", value);
// buf 中存储的long long 转换的字符串
sds sdsfromlonglong(long long value) {
    char buf[SDS_LLSTR_SIZE];
    int len = sdsll2str(buf, value);

    return sdsnewlen(buf, len);
}


// hhhhh
// 打印函数，被sdscatprintf所调用
// T = O(N^2)
// Like sdscatpritf() but gets va_list instead of being variadic.
sds sdscatvprintf(sds s, const char *fmt, va_list ap) {
    va_list cpy;
    char staticbuf[1024];
    char *buf = staticbuf;
    char *t = NULL;
    size_t buflen = strlen(fmt) * 2;

    if (buflen > sizeof(staticbuf)) {
        buf = zmalloc(buflen);
        if (buf == NULL) return NULL;
    } else {
        buflen = sizeof(staticbuf);
        // for debug
        printf("%s:%d: staticbuf length:%zu\n", __FILE__, __LINE__, buflen);
    }

    while (1) {
        buf[buflen-2] = '\0';
        va_copy(cpy, ap);

        vsnprintf(buf, buflen, fmt, cpy);

        // 空间不够用，重新分配吗 ？
        // hhhhh
        if (buf[buflen - 2] != '\0') {
            if (buf != staticbuf) zfree(buf);
            buflen *= 2;
            buf = zmalloc(buflen);
            if (buf == NULL) return NULL;
            continue;
        }
        break;
    }

    t = sdscat(s, buf);
    if (buf != staticbuf) zfree(buf);

    return t;
}


//
/* Append to the sds string 's' a string obtained using printf-alike format
 * specifier.
 *
 * After the call, the modified sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 *
 * Example:
 *
 * s = sdsempty("Sum is: ");
 * s = sdscatprintf(s,"%d+%d = %d",a,b,a+b).
 *
 * Often you need to create a string from scratch with the printf-alike
 * format. When this is the need, just use sdsempty() as the target string:
 *
 * s = sdscatprintf(sdsempty(), "... your format ...", args);
 */
sds sdscatprintf(sds s, const char *fmt, ...) {
    va_list ap;
    char *t = NULL;
    va_start(ap, fmt);

    // O(N^2)
    t = sdscatvprintf(s, fmt, ap);

    va_end(ap);

    return t;
}


// 与函数sdscatprintf类似，但性能更好
/* This function is similar to sdscatprintf, but much faster as it does
 * not rely on sprintf() family functions implemented by the libc that
 * are often very slow. Moreover directly handling the sds string as
 * new data is concatenated provides a performance improvement.
 *
 * However this function only handles an incompatible subset of printf-alike
 * format specifiers:
 *
 * %s - C String
 * %S - SDS string
 * %i - signed int
 * %I - 64 bit signed integer (long long, int64_t)
 * %u - unsigned int
 * %U - 64 bit unsigned integer (unsigned long long, uint64_t)
 * %% - Verbatim "%" character.
 */
sds sdscatfmt(sds s, char const *fmt, ...) {
    struct sdshdr *sh = (void*)(s - (sizeof(struct sdshdr)));
    size_t initlen = sdslen(s);
    const char *f = fmt;
    int i;
    va_list ap;

    va_start(ap, fmt);
    f = fmt;    /* Next format specifier byte to process. */
    i = initlen; /* Position of the next byte to write to dest str. */

    while (*f) {
        char next, *str;
        size_t l;
        long long num;
        unsigned long long unum;

        // make sure there is always space for at least 1 char
        if (sh->free == 0) {
            s = sdsMakeRoomFor(s, 1);
            sh = (void*)(s - sizeof(struct sdshdr));
        }

        switch(*f) {
            case '%':
                next = *(f + 1);
                f++;
                switch(next) {
                    case 's':
                    case 'S':
                        str = va_arg(ap, char*);
                        l = (next == 's') ? strlen(str) : sdslen(str);
                        if (sh->free < l) {
                            s = sdsMakeRoomFor(s, l);
                            sh = (void*)(s - (sizeof(struct sdshdr)));
                        }
                        memcpy(s + i, str, l);
                        sh->len += l;
                        sh->free -= l;
                        i += l;
                        break;
                    case 'i':
                    case 'I':
                        if (next == 'i')
                            num = va_arg(ap, int);
                        else
                            num = va_arg(ap, long long);
                        {
                            char buf[SDS_LLSTR_SIZE];
                            l = sdsll2str(buf, num);
                            if (sh->free < l) {
                                s = sdsMakeRoomFor(s, l);
                                sh = (void*)(s - (sizeof(struct sdshdr)));
                            }
                            memcpy(s+i, buf, l);
                            sh->len += l;
                            sh->free -= l;
                            i += l;
                        }
                        break;
                    case 'u':
                    case 'U':
                        if (next == 'u')
                            unum = va_arg(ap, unsigned int);
                        else
                            unum = va_arg(ap, unsigned long long);
                        {
                            char buf[SDS_LLSTR_SIZE];
                            l = sdsull2str(buf, unum);
                            if (sh->free < l) {
                                s = sdsMakeRoomFor(buf, num);
                                sh = (void*)(s - (sizeof(struct sdshdr)));
                            }
                            memcpy(s+i, buf, l);
                            sh->len += l;
                            sh->free -= l;
                            i += l;
                        }
                        break;
                    default: /* Handle %% and generally %<unknown>. */
                        s[i++] = next;
                        sh->len += 1;
                        sh->free -= 1;
                        break;
                } // switch(next)
                break;
            default:
                s[i++] = *f;
                sh->len += 1;
                sh->free -= 1;
                break;
        } // switch(*f)
        f++;
    } // while
    va_end(ap);

    s[i] = '\0';
    return s;
}

