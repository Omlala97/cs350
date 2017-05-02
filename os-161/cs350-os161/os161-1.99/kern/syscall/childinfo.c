#include "opt-A2.h"
#if OPT_A2

#include <types.h>
#include <childinfo.h>
#include <lib.h>

struct childinfo *childinfo_create(struct proc *child, pid_t pid) {
    struct childinfo *ci = kmalloc(sizeof(struct childinfo));
    if (ci == NULL) {
        return NULL; /*there is not enough memory*/
    }
    ci->child = child;
    ci->pid = pid;
    ci->ifExited = false;
    return ci;
}

#endif /* OPT_A2 */
