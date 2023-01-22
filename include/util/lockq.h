#ifndef PCO_LOCKQ_H
#define PCO_LOCKQ_H

struct slist_node {
    void* data;
    struct slist_node* next;
};

struct slist {
    struct slist_node* head, tail;
};

#define slist_init(l) {(l)->head=(l)->tail=NULL;}

void slist_push(struct slist* l, void* x);
void* slist_pop(struct slist* l);

#endif