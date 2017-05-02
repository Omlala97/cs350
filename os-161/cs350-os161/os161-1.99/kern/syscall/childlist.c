#include "opt-A2.h"
#if OPT_A2

#define CHILDINFOINLINE

#include <types.h>
#include <array.h>
#include <spinlock.h>
#include <childinfo.h>
#include <childlist.h>
#include <kern/errno.h>
#include <proc.h>

int
add(struct proc *child, struct proc* parent) { // add the child  his parent. Note this function will only be called in the sys_fork
    if ( parent->cl == NULL) {
        parent->cl = childlist_create();
    }
    if ( parent->cl == NULL) { // still NULL, that means there is not enough memory to allocate this sturcture in heap
        return ENOMEM;
    }
    int result;
    unsigned index;
    struct childinfo* ci = childinfo_create(child, child->pid);
    if (ci == NULL) {
        return ENOMEM;
    }
    result = childlist_add(parent->cl, ci, &index);
    if (result != 0) {
        return ENOMEM;                    /*there is some type of memeory error*/
    }
    return 0;
}

void remove(struct proc *child, struct proc *parent ) { //  remove this child info after the child exsits and the parent has got his exsit code
    unsigned size = childlist_num(parent->cl);
    for (unsigned i=0; i < size; i++) {
        struct childinfo *temp = childlist_get(parent->cl, i);
        if (temp->child == child) { // that is the one, we need to remove it
            childlist_remove(parent->cl, i);
            return;
        }
    }
    //panic("did not find the child!");
    return;
}

struct childinfo*
findChildInfo(struct childlist* cl, struct proc *child) {
        /*We need to find the child first*/
	if (cl == NULL) return NULL; 
        unsigned size = childlist_num(cl);
        struct childinfo *target = NULL;
        for (unsigned i = 0; i < size; i++) {
                struct childinfo *ci = childlist_get(cl, i);
                if (ci->child == child ) { // that is our child!
                        target = ci;
                        break;
                }
        }
	return target;
}

#endif /*OPT-A2*/

