#include "opt-A2.h"
#if OPT_A2

#ifndef _CHILDINFO_H__
#define _CHILDINFO_H__

#include <types.h>
struct proc;

struct childinfo {
    struct proc* child;
    int exitCode;
    pid_t pid;
    bool ifExited;
};

struct childinfo *childinfo_create(struct proc *child, pid_t pid);

#endif /* _CHILDINFO_H__ */
#endif /* OPT_A2 */
