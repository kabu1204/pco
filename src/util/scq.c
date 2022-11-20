#include "util/scq.h"
#include "atomic.h"
#include "common.h"
#include <stdlib.h>

size_t align16_backward(size_t x) {
    return (x + (size_t)(15)) & ~(size_t)(15);
}

void print_bits(u64 x){
    char buf[65];
    int p=0;
    u64 i=(1ull<<63);
    while(i){
        buf[p++]=(x&i)?'1':'0';
        i>>=1;
    }
    buf[p++]='\n';
    buf[p]='\0';
    write(1, buf, p);
}

lscq_t* lscq_create(){
    scq_t* cq = scq_create();
    lscq_t* q = malloc(sizeof(lscq_t));
    q->head = q->tail = cq;
    return q;
}

void lscq_push(lscq_t* q, u64 val){
    scq_t *cq, *new_scq, *next;
    while(1){
        cq = atomic_load_u64(&q->tail);
        printf("cq: %p\ncq->next: %p\n", cq, cq->next);
        next = (scq_t*)atomic_load_u64((u64*)(&cq->next));
        if(next!=NULL){
            atomic_cas_u64(&q->tail, cq, next);
            continue;
        }
        if(likely(scq_push(cq, val))){
            return;
        }
        atomic_bts_u64(&cq->tail, 63);
        new_scq = scq_create();
        scq_push(new_scq, val);
        if(atomic_cas_u64(&cq->next, NULL, new_scq)){
            atomic_cas_u64(&q->tail, cq, new_scq);
        }
        scq_free(new_scq);
    }
}

int lscq_pop(lscq_t* q, u64* val){
    scq_t *cq, *new_scq, *next;
    while(1){
        cq = atomic_load_u64(&q->head);
        if(likely(scq_pop(cq, val))){
            return 1;
        }
        next = atomic_load_u64(&cq->next);
        if(next==NULL){
            return 0;
        }
        atomic_store_u64(&cq->threshold, SCQ_RESET_THRESHOLD);
        if(scq_pop(cq, val)){
            return 1;
        }
        if(atomic_cas_u64(&q->head, cq, next)){
            cq = NULL;
        }
    }
}

scq_t* scq_create(){
    int i;
    u64 t=0;
    scq_t* q = malloc(sizeof(scq_t));
    q->head = q->tail = SCQ_BUF_SIZE;
    q->threshold = -1;
    q->entries = (void*)align16_backward((size_t)q->buf);
    q->next = NULL;
    for(i=0; i<SCQ_BUF_SIZE; ++i){
        q->entries[i].flag = SCQ_FLAG_MASK;
        q->entries[i].data = 0;
    }
    printf("SCQ_BUF_SIZE: %ld\n", SCQ_BUF_SIZE);
    printf("SCQ_SIZE: %ld\n", SCQ_SIZE);
    printf("q->tail: %ld\n", q->tail);
    printf("q->tail: %ld\n", q->head);
    write(1,"SCQ_BUF_SIZE_MASK:   ",21); print_bits(SCQ_BUF_SIZE_MASK);
    write(1,"SCQ_TAIL_CYCLE_MASK: ",21); print_bits(SCQ_TAIL_CYCLE_MASK);
    write(1,"SCQ_CLOSED:          ",21); print_bits(SCQ_CLOSED);
    write(1,"SCQ_TAIL_MASK:       ",21); print_bits(SCQ_TAIL_MASK);
    write(1,"SCQ_FLAG_UNUSED:     ",21); print_bits(SCQ_FLAG_UNUSED);
    write(1,"SCQ_FLAG_SAFE:       ",21); print_bits(SCQ_FLAG_SAFE);
    write(1,"SCQ_FLAG_MASK:       ",21); print_bits(SCQ_FLAG_MASK);
    write(1,"SCQ_CYCLE_MASK:      ",21); print_bits(SCQ_CYCLE_MASK);
    write(1,"SCQ_RESET_THRESHOLD: ",21); print_bits(SCQ_RESET_THRESHOLD);
    atomic_bts_u64(&t, 63); atomic_bts_u64(&t, 62);
    write(1,"t:                   ",21); print_bits(t);
    return q;
}

void scq_free(scq_t* q){
    free(q);
}

int scq_push(scq_t* q, u64 val){
    u64 T, tcycle, ecycle, j, is_safe, is_unused;
    u128 ent;
    u128 new_ent = {SCQ_FLAG_SAFE, val};
    while(1){
        T = atomic_add_u64(&q->tail, 1)-1;
        if(T & SCQ_CLOSED) return 0;    // closed
        T &= SCQ_TAIL_MASK;
        tcycle = (T & SCQ_TAIL_CYCLE_MASK)>>(SCQ_SHIFT+1);
        j = cache_remap_8B(T);
    retry:
        ent = atomic_load_u128(&q->entries[j]);
        ecycle = ent.flag & SCQ_CYCLE_MASK;
        is_safe = ent.flag & SCQ_FLAG_SAFE;
        is_unused = ent.flag & SCQ_FLAG_UNUSED;
        if((ecycle < tcycle) && (is_unused==SCQ_FLAG_UNUSED) && 
            ((is_safe==SCQ_FLAG_SAFE) || (atomic_load_u64(&q->head) <= T))){
            new_ent.flag = tcycle | SCQ_FLAG_SAFE;
            if(!atomic_cas_u128(&q->entries[j], ent, new_ent)) {
                goto retry;
            }
            if(atomic_load_u64(&q->threshold) != SCQ_RESET_THRESHOLD){
                atomic_store_u64(&q->threshold, SCQ_RESET_THRESHOLD);
            }
            return 1;
        }
        if(T+1>=atomic_load_u64(&q->head)+SCQ_BUF_SIZE){
            return 0;
        }
    }
    return 0;
}

int scq_pop(scq_t* q, u64* val){
    if(atomic_load_u64(&q->threshold) < 0){
		return 0;
	}
	u64 H, j, hcycle, ecycle, is_safe, is_unused, T;
	u128 ent, new_ent;
	while(1){
		H = atomic_add_u64(&q->head, 1) - 1;
		hcycle = (H & SCQ_TAIL_CYCLE_MASK)>>(SCQ_SHIFT+1);
		j = cache_remap_8B(H);
	retry:
		ent = atomic_load_u128(&q->entries[j]);
        ecycle = ent.flag & SCQ_CYCLE_MASK;
        is_safe = ent.flag & SCQ_FLAG_SAFE;
        is_unused = ent.flag & SCQ_FLAG_UNUSED;
		if(ecycle == hcycle){
            atomic_bts_u64(&(q->entries[j].flag), 63);
            *val = ent.data;
			return 1;
		}
		if(ecycle < hcycle){
			new_ent.flag = ecycle;
			new_ent.data = ent.data;
			if(is_unused == SCQ_FLAG_UNUSED){
				new_ent.flag = hcycle | is_safe | SCQ_FLAG_UNUSED;
                new_ent.data = 0;
			}
			if(!atomic_cas_u128(&q->entries[j], ent, new_ent)) {
				goto retry;
			}
		}
		T = atomic_load_u64(&q->tail);
		if(T<=H+1){
            scq_catchup(q, T, H+1);
            atomic_add_u64(&q->threshold, -1);
			return 0;
		}
		if(atomic_add_u64(&q->threshold, -1)+1 <= 0){
			return 0;
		}
	}
}

void scq_catchup(scq_t* q, u64 T, u64 H){
    while(!atomic_cas_u64(&q->tail, T, H)){
        H = atomic_load_u64(&q->head);
        T = atomic_load_u64(&q->tail);
        if(T>=H) return;
    }
}