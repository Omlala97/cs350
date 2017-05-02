#include "opt-A2.h"
#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <kern/fcntl.h>
#include <limits.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <mips/trapframe.h>
#include <childlist.h>
#include <vfs.h>
#include <synch.h>

struct trapframe; // sot that compiler won't complian
  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

#if OPT_A2
//	kprintf("exit %d\n", curproc->pid);
	if (curproc->parent != NULL) { // update his parent info, so his parent knows he has exited
                lock_acquire(curproc->parent->lock1);
		struct childinfo* ci = findChildInfo(curproc->parent->cl, curproc);
		if (ci == NULL) {
			panic("parent does not know he has a child! NO!!!!!");
		}
		//ci->ifExited = true;
		//ci->exitCode = _MKWAIT_EXIT(exitcode);
//		addToNotSafe(curproc);  // can not safely reuse his pid, as his parent call wait pid on him later
		// wake up his parent code goes here
		//lock_acquire(curproc->parent->lock1);
	                ci->ifExited = true;
        	        ci->exitCode = _MKWAIT_EXIT(exitcode);
			cv_broadcast(curproc->parent->cv1, curproc->parent->lock1); // I am going to exit, let the parent know
		lock_release(curproc->parent->lock1);
	} else { // no parent
		reuse(curproc);        // there is no parent, so we can safely reuse its pid
	}
	// update all the children, there parent has exited
	if (curproc->cl != NULL) {
		unsigned size = childlist_num(curproc->cl);
		for (unsigned i = 0; i < size; i++) {
         	       	struct childinfo *ci = childlist_get(curproc->cl, i);
			struct proc* child = ci->child;
			child->parent = NULL;
        	}
	}
#endif
  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}

#if OPT_A3
void 
sys_kill(int exitcode) {							// this function is used to kill a thread. It is very similar to sys_exit except we encode the exit code diffrently(_MKWAIT_SIG)
    if (curproc->parent != NULL) { // update his parent info, so his parent knows he has exited
        lock_acquire(curproc->parent->lock1);
        struct childinfo* ci = findChildInfo(curproc->parent->cl, curproc);
        if (ci == NULL) {
            panic("parent does not know he has a child! NO!!!!!");
        }
        ci->ifExited = true;
        ci->exitCode = _MKWAIT_SIG(exitcode);
        cv_broadcast(curproc->parent->cv1, curproc->parent->lock1);
        lock_release(curproc->parent->lock1);
    } else {
        reuse(curproc);
    }
    if (curproc->cl != NULL) {
        unsigned size = childlist_num(curproc->cl);
        for (unsigned i = 0; i < size; i++) {
            struct childinfo *ci = childlist_get(curproc->cl, i);
            struct proc* child = ci->child;
            child->parent = NULL;
        }
    }
    struct addrspace *as;
    struct proc *p = curproc;
    DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);
    KASSERT(curproc->p_addrspace != NULL);
    as_deactivate();
    as = curproc_setas(NULL);
    as_destroy(as);
    proc_remthread(curthread);
    proc_destroy(p);
    thread_exit();
    panic("return from thread_exit in sys_exit\n");

}

#endif // OPT_A3

/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
#if OPT_A2
   *retval = curproc->pid;
#else
   /* for now, this is just a stub that always returns a PID of 1 */
   /* you need to fix this to make it work properly */
   *retval = 1;
#endif /* OPT_A2 */
   return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{

#if OPT_A2
//	kprintf("waitpid, %d\n", pid);
	if (options != 0) {
    		return(EINVAL);
  	}
	if (curproc->cl == NULL) { 		// parent has no children
		return ESRCH;
	}
	int exitCode;
	unsigned index; // used to track the childinfo's index
	/*We need to find the child first*/
	unsigned size = childlist_num(curproc->cl);
	struct childinfo *target = NULL;
	for (unsigned i = 0; i < size; i++) {
		struct childinfo *ci = childlist_get(curproc->cl, i);
		if (ci->pid == pid) { // that is our child!
			target = ci;
			index = i;
			break;
		}
	}
	if (target == NULL) {			// parent does not have this particular child
		return ESRCH;
	}
	if (target->ifExited) {
		exitCode = target->exitCode;
		/*should delete this struct*/
		spinlock_acquire(&curproc->p_lock2);
		childlist_remove(curproc->cl, index);
		spinlock_release(&curproc->p_lock2);
	} else {
		// should block
		// then wait on the child is done to get up
		// once get up, remmeber to update the notSafeToUse list
		// and free that childinfo
		lock_acquire(curproc->lock1);
		while (target->ifExited == false) {// child is still running
			cv_wait(curproc->cv1, curproc->lock1);
		}
                exitCode = target->exitCode; // got the exit code
		lock_release(curproc->lock1);
		reuse(target->child);	// resue that pid
		spinlock_acquire(&curproc->p_lock2);
			childlist_remove(curproc->cl, index);  // no longer to keep this guy's infomation
			if (childlist_num(curproc->cl) == 0) { // no more child, just set that to null, so when this process exit, check the now will save a lot of trouble
				childlist_cleanup(curproc->cl);
				curproc->cl = NULL;
			}
		spinlock_release(&curproc->p_lock2);
		// now we have got its exit code, and delete its structure, we safely use its pid;
	}
	int result = copyout((void *)&exitCode,status,sizeof(int));
 	if (result) {
    		return(result);
  	}
  	*retval = pid;
  	return(0);
}
	

#else
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}
#endif/*OPT_A2*/

#if OPT_A2
/*implement fork*/
int 
sys_fork(struct trapframe *tf, pid_t *retval) {
   struct proc* child;
   /* STEP 1, Create process structure for child process*/
	char *name = kmalloc((strlen(curproc->p_name) + 3) * sizeof(char));
   	if (name == NULL) { // not enough memory, should return this memory error
        	return ENOMEM;
   	}
   // name the children in the parent name + "-c" to indicate the child parent relationship, for debugging purposes
   	strcpy(name,curproc->p_name);
   	strcat(name,"-c");
   	child = proc_create_runprogram(name);
   
   	if (child == NULL) { // that means we cannot allocate enough memory for child proc, should return  memory error
		kfree(name);
		return ENOMEM;
   	}
   
   /* end  of STEP 1 */

 
   /* STEP 2, create copy address space (and data) from parent to child */
   	struct addrspace* child_as = as_create();
	if (child_as == NULL) {
		kfree(name);
		as_destroy(child_as);
		proc_destroy(child);
		return ENOMEM;
	}
   	int i = as_copy(curproc->p_addrspace, &child_as);
   	if (i != 0) {// some type of error
		kfree(name);
		as_destroy(child_as);  // since it fails, I have to call as_destroy to free the space
                proc_destroy(child);
		return ENOMEM;         // out of memory error
   	}
   /* end of Step 2*/

   /* STEP 3, attach the newly created address space to the child process structure */

   /* now the as is created, we need to give it to the new process created in STEP 1 */
   /* the implementation below is modified from curproc_setaas() defined in proc.c */

	spinlock_acquire(&child->p_lock);
	child->p_addrspace = child_as;
	spinlock_release(&child->p_lock);
   /* note that at this time the address space has not been activated yet, we need to call as_activate() when the curproc is the child process.*/
   /* end of Step 3*/


  /* STEP 4, Assign PID to child process and create the parent/child relationship */
	child->parent = curproc;    // create parent and child relationship, notice that the pid assignment has been done when the child process structure is created.
  /* end of of Step 4 */

  /* STEP 5, create thread for child process */
	char *thread_name = kmalloc(sizeof(char) * strlen(name));
	if (thread_name == NULL) {
		kfree(name);
                as_destroy(child_as);
                proc_destroy(child);
		return ENOMEM;
	}
        strcpy(thread_name, name);
	// we need a safe way to pass the trapframe to the child. Now the parent's trapframe is on the kernel stack of the parent process,
	// if we just directly pass the pointer to the child, the pointer may not be valid as the when the prent process is back to the user space,
	// the trapframe will be poped.
	// my solution is to copy it to the heap, and then the child copys the trapframe from the heap, and then free the trapframe on the heap.
	// so we are passing the pointer to the trapframe copy in the heap
	struct trapframe *heap_copy = kmalloc(sizeof(struct trapframe));
	if (heap_copy == NULL) {
		kfree(name);
		kfree(thread_name);
		as_destroy(child_as);
		proc_destroy(child);
		return ENOMEM;
	}
	*heap_copy = *tf;
	void* stuff = heap_copy;       		/*make it a void pointer so that the thread_fork function can take in*/
	spinlock_acquire(&curproc->p_lock2);
        int err = add(child, curproc); // see if we can add this childinfo to to the struct
	spinlock_release(&curproc->p_lock2);
        if (err != 0) {
		kfree(name);
                kfree(thread_name);
                kfree(heap_copy);
		as_destroy(child_as);
                proc_destroy(child);
                return ENOMEM;
        }

 	int result = thread_fork(thread_name /* thread name */,
      			         child	 /* new process */,
          	   		 enter_forked_process/* thread function, really it is enter_forked_process */,
           			 stuff/* thread arg */, 2/* thread arg */);
    	if (result) {  //not 0, some type of error
 		kfree(name);
		kfree(thread_name);
		kfree(heap_copy);
		spinlock_acquire(&curproc->p_lock2); // add lock to provide mutual excluation
		remove(child, curproc);
		spinlock_release(&curproc->p_lock2);
                as_destroy(child_as);
                proc_destroy(child);
        	return result;
    	}
//	curproc->cl = add(child, curproc);
//	if (curproc->cl == NULL) {
//		return ENOMEM;
//	}
	*retval = child->pid;
	return 0;
}
  /* end of Step 5, note that step 6 and step 7 is done by the enter_forked_process() function, which will take care of things */
#endif/* OPT_A2*/

#if OPT_A2
int sys_execv(userptr_t progname, userptr_t args) {

	//Step 1, Count the number of arguments and copy them into the kernel
	int numArgs = 0;
	char ** c = (char**) args; // casted args
	if (c == NULL) {
		return EFAULT;
	}
	while (c[numArgs] != NULL) {
		numArgs ++;
		if (numArgs > ARG_MAX) {
			return E2BIG;
		}
	}
	int err;   			// used to check if there is an error
	char **tempArgs = kmalloc(sizeof(char*) * (numArgs+1));    // the last one for NULL
	if (tempArgs == NULL) {
		return ENOMEM;
	}
	tempArgs[numArgs] = NULL; // The last one should be null
	char *input;  // store each arg
	size_t length;
	size_t l;     // we need it for the function call, never gonna use it. It is a trivial value
	for(int i = 0; i < numArgs; i++) {
		length = strlen(c[i]) + 1;
		input = kmalloc(sizeof(char) * length);
		if (input == NULL) {
			kfree(tempArgs);
			return ENOMEM;
		}
		err = copyinstr((const_userptr_t)c[i], input, length, &l);
		if (err) {
			kfree(input);
			for (int j =0; j < i; j++) kfree(tempArgs[j]);     // free all the previous allocated stuff
			kfree(tempArgs);
			return err;
		}
		tempArgs[i] = input;
	}


	//Step 2, copy the program path
	if ((char*)progname == NULL) {
		for (int j =0; j < numArgs; j++) kfree(tempArgs[j]);     // free all the previous allocated stuff
                kfree(tempArgs);
		return EFAULT;
	}
	char *tempName;    // buffer used to store the program name
	tempName = kmalloc(PATH_MAX);
	if (tempName == NULL) {
                for (int j =0; j < numArgs; j++) kfree(tempArgs[j]);     // free all the previous allocated stuff
                kfree(tempArgs);
		return ENOMEM;
	}

	if ((err = copyinstr(progname, tempName, PATH_MAX, &l)) != 0)
    	{
		for (int j =0; j < numArgs; j++) kfree(tempArgs[j]);     // free all the previous allocated stuff
		kfree(tempArgs);
		kfree(tempName);
        	return err; 
   	}


/* The next three steps coms from runprogram
* 1.Open the program file using vfs_open(prog_name, …)
* 2.Create new address space, set process to the new address space, and activate it
* 3.Using the opened program file, load the program image using load_elf
*/



	struct addrspace *as;
        struct vnode *v;
        vaddr_t entrypoint, stackptr;
	int result;
 	result = vfs_open(tempName, O_RDONLY, 0, &v);

        if (result) {
		for (int j =0; j < numArgs; j++) kfree(tempArgs[j]);     // free all the previous allocated stuff
                kfree(tempArgs);
                kfree(tempName);
                return result;
        }

        /* Create a new address space. */
        as = as_create();
        if (as ==NULL) {
		for (int j =0; j < numArgs; j++) kfree(tempArgs[j]);     // free all the previous allocated stuff
                kfree(tempArgs);
                kfree(tempName);
                vfs_close(v);
                return ENOMEM;
        }

        /* Switch to it and activate it. */
	as_destroy(curproc->p_addrspace);  // destroy the old one
        curproc_setas(as);

        as_activate();

        /* Load the executable. */
        result = load_elf(v, &entrypoint);
        if (result) {
                /* p_addrspace will go away when curproc is destroyed */
		for (int j =0; j < numArgs; j++) kfree(tempArgs[j]);     // free all the previous allocated stuff
                kfree(tempArgs);
                kfree(tempName);
                vfs_close(v);
                return result;
        }

        /* Done with the file now. */
        vfs_close(v);

        /* Define the user stack in the address space */
        result = as_define_stack(as, &stackptr);
        if (result) {
                /* p_addrspace will go away when curproc is destroyed */
		for (int j =0; j < numArgs; j++) kfree(tempArgs[j]);     // free all the previous allocated stuff
                kfree(tempArgs);
                kfree(tempName);
                return result;
        }
	/*************************************end of step  5*********************/

	//Step 6,  copy to the user_stack
	char **tempCopy = kmalloc(sizeof(char*) * (numArgs+1)); // add one for the null. This array will store the address of the copied new strings. And then we will copy these to user stack as well
	if (tempCopy == NULL) {
		for (int j =0; j < numArgs; j++) kfree(tempArgs[j]);     // free all the previous allocated stuff
                kfree(tempArgs);
                kfree(tempName);
                return ENOMEM;
	}
        tempCopy[numArgs] = NULL;
        for (int i = 0; i < numArgs; i++) {
                stackptr -= ROUNDUP(strlen(tempArgs[i])+1, 8);
                err = copyoutstr(tempArgs[i], (userptr_t)stackptr, strlen(tempArgs[i]) +1, &l);
		if(err) {
		     return err;
		}
                tempCopy[i] = (char *)stackptr;
        }
        stackptr -= ROUNDUP((numArgs+1) * sizeof(char*), 8); // allocate enough space for all the pointers
        size_t stackpCopy = stackptr; // remember where the top is
        for (int i = 0; i <=numArgs; i++) { // copy the array it self
                err = copyout(&tempCopy[i], (userptr_t)stackptr, sizeof(char*));
                if (err) {
			return err;
		}
		stackptr += sizeof(char*);
        }
        stackptr = stackpCopy;

	for (int j =0; j < numArgs; j++) kfree(tempArgs[j]);     // free all the previous allocated stuff
        kfree(tempArgs);
        kfree(tempName);
	kfree(tempCopy);

	/* Warp to user mode. */
	enter_new_process(numArgs /*argc*/, (userptr_t)stackptr /*userspace addr of argv*/,
			  stackptr, entrypoint);
	
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;

}
#endif/*OPT_A2*/
