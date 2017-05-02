#include "opt-A2.h"
#if OPT_A2

#ifndef _PID_MANAGER_H__
#define _PID_MANAGER_H__

#include <proc.h>
#include <types.h>
#include <array.h>

#ifndef PROCINLINE
#define PROCINLINE INLINE
#endif /* PROCINLINE */

DECLARRAY_BYTYPE(pidlist, struct proc);
DEFARRAY_BYTYPE(pidlist, struct proc, PROCINLINE);

struct pidlist* proc_array;

void reuse(struct proc *p);
struct pidlist* proc_array_create(void);
unsigned newPID(struct proc *p);




#endif /* _PID_MANAGER_H__ */
#endif /* OPT_A2 */
