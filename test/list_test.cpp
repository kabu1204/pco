#include <gtest/gtest.h>
#include <stdlib.h>
#include "util/list.h"

typedef struct {
    int a;
    struct list_node node;
} node_t;

node_t* alloc_node(int a){
    node_t *t = (node_t*)malloc(sizeof(node_t));
    t->a = a;
    return t;
}

#define NIL (struct list_node*)(0)

// Demonstrate some basic assertions.
TEST(ListTest, BasicTest) {
    struct list_node l;
    struct list_node* tmp;
    node_t *t;
    init_list_node(&l);

    EXPECT_TRUE(list_empty(&l));

    t = alloc_node(123);
    list_push_front(&l, &t->node);

    EXPECT_FALSE(list_empty(&l));

    t = alloc_node(456);
    list_push_front(&l, &t->node);

    t = alloc_node(789);
    list_push_front(&l, &t->node);

    tmp = list_pop_back(&l);
    EXPECT_NE(tmp, NIL);

    t = container_of(tmp, node_t, node);
    EXPECT_EQ(t->a, 123);
    free(t);

    tmp = list_pop_back(&l);
    EXPECT_NE(tmp, NIL);

    t = container_of(tmp, node_t, node);
    EXPECT_EQ(t->a, 456);
    free(t);

    tmp = list_pop_back(&l);
    EXPECT_NE(tmp, NIL);

    t = container_of(tmp, node_t, node);
    EXPECT_EQ(t->a, 789);
    free(t);

    tmp = list_pop_back(&l);
    EXPECT_EQ(tmp, NIL);

    // EXPECT_EQ(NULL, container_of(NIL, node_t, node));
}