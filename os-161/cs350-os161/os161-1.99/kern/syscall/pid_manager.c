#include "opt-A2.h"
#if OPT_A2

#define PROCINLINE

#include <types.h>
#include <array.h>
#include <spinlock.h>
#include <proc.h>
#include <pid_manager.h>

struct spinlock pid_guard = SPINLOCK_INITIALIZER;   // use one spinlock to make sure all operations are atomic, thus provide mutual exclusion to pidlist
struct pidlist* proc_array = NULL;
//struct pidlist* notSafe = NULL;
/* rapper that returns proc_array, which is an array of process pointers.Note the index of the pointer is the pid of that process*/
/* if for a particular index, the pointer points to NULL, that means that pid is reusable */
/* if there are no pids to reuse, then append the proc to the end of the list, and that index is the new pid. */

struct pidlist*
proc_array_create()
{
 spinlock_acquire(&pid_guard);
    if (proc_array != NULL) {
	spinlock_release(&pid_guard);
	return proc_array;     // singleton pattern, only allow one instance of the array
    }
    if ((proc_array = pidlist_create()) == NULL) {
		spinlock_release(&pid_guard);
		return NULL;
    }
    int result = pidlist_setsize(proc_array,2);
    if (result != 0) { // not enough memory
	spinlock_release(&pid_guard);
	return NULL;
    }
    pidlist_set(proc_array, 0, NULL);
    pidlist_set(proc_array, 1, NULL);	/* The pid starts from 2, so we fill the first two be NULL values. All operations will ingnore these two values */
 spinlock_release(&pid_guard);
 return proc_array;
}

/* takes in a process p, and return the pid avaliable */
/* it will reuse the old pids when possible.          */
unsigned 
newPID(struct proc *p) {
   spinlock_acquire(&pid_guard);
	unsigned size = pidlist_num(proc_array);
        struct proc * temp ;
	for (unsigned i = 2; i < size; i++) {
		temp = pidlist_get(proc_array, i);        	// get the process pointer and check if it is NULL
		if (temp == NULL) {				// NULL means that spot is avaliable
			pidlist_set(proc_array, i, p);
			spinlock_release(&pid_guard);
			return i;
		}
	}
	// no pid to reuse, append the process poniter to the back and return that index
	unsigned index_where_added;
	int result;
	if ((result = pidlist_add(proc_array, p, &index_where_added)))	// it will store the index on the index_where_added
	{
 		// there is some memory error
		spinlock_release(&pid_guard);
		return 0; // indicate there is some kind of error
	}
    spinlock_release(&pid_guard);
    return index_where_added;
}


/*when a process is terminated, call this function to reuse that pid. It will set that index of the proc_array to be NULL, thus showing that pid is avaliable */
void
reuse(struct proc *p) {
   spinlock_acquire(&pid_guard);
	unsigned size = pidlist_num(proc_array);
	struct proc *temp;
	for (unsigned i =2; i < size; i++) {
		temp = pidlist_get(proc_array, i);              // get the process pointer and check if it is p
                if (temp == p) {                             	// if it is, set that index to NULL, to show that pid is avaliable to reuse
                        pidlist_set(proc_array, i, NULL);
			spinlock_release(&pid_guard);
                        return;
                }
	}
    spinlock_release(&pid_guard);
}
/*
struct pidlist* addToNotSafe(struct proc* p) {
    spinlock_acquire(&pid_guard);
	notSafe = pidlist_create();
	if (notSafe = NULL) {
		spinlock_release(&pid_guard);
		return NULL;
	}
	unsigned index_where_added;
        int result;
        if ((result = pidlist_add(notsafe, p, &index_where_added)))  { // it will store the index on the index_where_added{
                // there is some memory error
                spinlock_release(&pid_guard);
                return NULL; // indicate there is some kind of error
        }
     spinlock_release(&pid_guard);
	return notSafe;
}

bool ifSafe(struct proc* p) {
	if (notSafe == NULL) return true;
	unsigned size = pidlist_num(notSafe);
	if (size == 0) return true;
	else {
		struct proc* temp;
		for (unsigned i = 0; i < size; i++) {
			temp = pidlist_get(notSafe, i);
			if (temp == p) {
				return false;
			}
		}
	}
	return true;
}

*/

#endif /* OPT_A2 */

