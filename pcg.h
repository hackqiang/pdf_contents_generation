#define SERACH_MAX_PAGE 20

#define MAX_KEYWORD_LEN 128
#define KEYWORKS_NUM 128

typedef struct outline_tree {
    int text[32];   //todo..
    int pagen;
    int total;
    struct outline_tree *pre;
    struct outline_tree *next;
    struct outline_tree *parent;
    struct outline_tree *child;
}outline_tree;