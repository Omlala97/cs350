#include "opt-A2.h"
#if OPT_A2

#ifndef _CHILDLIST_H__
#define _CHILDLIST_H__

#include <types.h>
#include <array.h>
#include <childinfo.h>

#ifndef CHILDINFOINLINE
#define CHILDINFOINLINE INLINE
#endif /* CHILDLISTINLINE */




DECLARRAY_BYTYPE(childlist, struct childinfo);
DEFARRAY_BYTYPE(childlist, struct childinfo, CHILDINFOINLINE);

int add(struct proc *child, struct proc* parent); // add the child to the curproc, which is his parent. Note this function will only be called in the sys_fork
void remove(struct proc *child,struct proc *parent); //  remove this child info after the child exsits and the parent has got his exsit code
struct childinfo* findChildInfo(struct childlist* cl, struct proc *child);// find the particular child in the child list

#endif /* _CHILDLIST__H__ */
#endif /* OPT_A2 */

