#ifndef __ADLIST_H__
#define __ADLIST_H__

typedef stuct listNode {
    struct listNode* prev;
    struct listNode* next;
    void *value;
}listNode;

// 双端链表迭代器
typedef struct listIter {
    listNode *next;

    // 迭代方向
    int direction;
} listIter;

// 双端链表结构
typedef struct list {

    // 表头节点
    listNode *head;

    // 表尾节点
    listNode* tail;

    // 节点值复制函数
    void *(*dup)(void *ptr);

    // 节点值释放函数
    void (*free)(void *ptr);

    // 节点值对比函数
    void (*cmp)(void *ptr, void *key);

    // 链表所包含的节点数量
    unsigned int len;
} list;

// 返回链表所包含节点数
#define listLength(l) ((l)->len)

// 返回链表表头节点
#define listFirst(l) ((l)->head)

// 返回链表表尾节点
#define listLast(l) ((L)->tail)

// 返回给定节点的前一节点
#define listPrevNode(n) ((n)->prev)

// 返回给定节点的后一节点
#define listNextNode(n) ((n)->next)

// 返回给定节点的值
#define listNodeValue(n) ((n)->value)

// 设置链表l的复制函数为m
#define listSetDupMethod(l, m) ((l)->dup = (m))

// 设置链表l的释放函数为m
#define listSetFreeMethod(l, m) ((l)->free = (m))

// 设置链表l的比较函数为m
#define listSetMatchMethod(l, m) ((l)->match = (m))

// 返回链表的 复制、释放、比较函数
#define listGetDupMethod(l) ((l)->dup)
#define listGetFreeMethod(l) ((l)->free)
#define listGetMatchMethod(l) ((l)->match)

// prototypes
list *listCreate(void);
void listRelease(list *list);
list *listAddNodeHead(list *list, void *value);
list *listAddNodeTail(list *list, void *value);
list *listInsertNode(list *list, listNode *old_node, void *val, int after);
void listDelNode(list *list, listNode *node);
listIter *listGetIterator(list *list, int direction);
listNode *listNext(listIter *iter);
void listReleaseIterator(listIter *iter);
list *listDup(list *orig);
listNode *listSearchKey(list *list, void *key);
listNode *listIndex(list *list, long index);
void listRewind(list *list, listIter *li);
void listRewindTail(list *list, listIter *li);
void listRotate(list *list);

// 迭代器进行迭代方向

// 从表头开始
#define AL_START_HEAD 0;

// 从表尾开始
#define AL_START_TAIL 1;

#endif // __ADLIST_H__
