#ifndef PCO_LINKQ_H
#define PCO_LINKQ_H

#if __cplusplus
extern "C" {
#endif

struct list_node {
    struct list_node *next, *prev;
};

#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

static void __attribute__((always_inline)) init_list_node(struct list_node* l){
    l->next = l->prev = l;
};

static int __attribute__((always_inline)) list_empty(struct list_node* l){
    return l == l->next;
}

static void __attribute__((always_inline)) list_push_front(struct list_node* l, struct list_node* new_){
    l->next->prev = new_;
    new_->next = l->next;
    new_->prev = l;
    l->next = new_;
}

static struct list_node* __attribute__((always_inline)) list_pop_back(struct list_node* l){
    if(list_empty(l)) return NULL;
    struct list_node* t = l->prev;
    t->prev->next = t->next;
    l->prev = t->prev;
    t->next = NULL;
    t->prev = NULL;
    return t;
}

#if __cplusplus
}
#endif

#endif