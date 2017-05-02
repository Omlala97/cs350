/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, int initial_count)
{
        struct semaphore *sem;

        KASSERT(initial_count >= 0);

        sem = kmalloc(sizeof(struct semaphore));
        if (sem == NULL) {
                return NULL;
        }

        sem->sem_name = kstrdup(name);
        if (sem->sem_name == NULL) {
                kfree(sem);
                return NULL;
        }

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
        sem->sem_count = initial_count;

        return sem;
}

void
sem_destroy(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
        kfree(sem->sem_name);
        kfree(sem);
}

void 
P(struct semaphore *sem)
{
        KASSERT(sem != NULL);

        /*
         * May not block in an interrupt handler.
         *
         * For robustness, always check, even if we can actually
         * complete the P without blocking.
         */
        KASSERT(curthread->t_in_interrupt == false);

	spinlock_acquire(&sem->sem_lock);
        while (sem->sem_count == 0) {
		/*
		 * Bridge to the wchan lock, so if someone else comes
		 * along in V right this instant the wakeup can't go
		 * through on the wchan until we've finished going to
		 * sleep. Note that wchan_sleep unlocks the wchan.
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_lock(sem->sem_wchan);
		spinlock_release(&sem->sem_lock);
                wchan_sleep(sem->sem_wchan);

		spinlock_acquire(&sem->sem_lock);
        }
        KASSERT(sem->sem_count > 0);
        sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

        sem->sem_count++;
        KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
        struct lock *lock;

        lock = kmalloc(sizeof(struct lock));
        if (lock == NULL) {
                return NULL;
        }

        lock->lk_name = kstrdup(name);
        if (lock->lk_name == NULL) {
                kfree(lock);
                return NULL;
        }

        lock->l_wchan = wchan_create(lock->lk_name);	// create a wait channel
        if (lock->l_wchan == NULL) {
		kfree(lock->lk_name);
                kfree(lock);
                return NULL;
        }
        spinlock_init(&lock->l_lock);
        spinlock_init(&lock->l_lock2);
        lock->owner_thread = NULL; 	// nobody owns the lock when the lock is initially created
        lock->flag = 0;			// set to 0 initially to make the first thread aquire to be successful
	// add stuff here as needed
        
        return lock;
}

void
lock_destroy(struct lock *lock)
{
        KASSERT(lock != NULL);

        // add stuff here as needed
        spinlock_cleanup(&lock->l_lock);
        spinlock_cleanup(&lock->l_lock2);
        wchan_destroy(lock->l_wchan);
        kfree(lock->lk_name);
        kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
        // Write this
	KASSERT(lock != NULL);
        KASSERT(curthread->t_in_interrupt == false);
	bool ifHold = lock_do_i_hold(lock);
	KASSERT(!ifHold);                   // can not acquire a lock that this thread already has
	spinlock_acquire(&lock->l_lock);            // lock the rest in the beginning
	while (lock->flag == 1) {  // acquire a thread that has already been owned 
                wchan_lock(lock->l_wchan);
                spinlock_release(&lock->l_lock);
                wchan_sleep(lock->l_wchan);

                spinlock_acquire(&lock->l_lock);

	}
	/*The lock is not owned, owner_thread_name == NULL  */
	/*Should set the flag to 1, and store the owner's address*/
	lock->flag = 1;
        lock->owner_thread = curthread;
	spinlock_release(&lock->l_lock);
}

void
lock_release(struct lock *lock)
{
        // Write this
	KASSERT(lock != NULL);
	bool ifHold = lock_do_i_hold(lock);
	KASSERT(ifHold);   // anohter thread who does not hold the lock cannot try to release it
	KASSERT(lock->flag == 1) ;	 // cannot release a lock that has already been released
  	spinlock_acquire(&lock->l_lock);
	lock->flag = 0;
        KASSERT(lock->flag == 0);
	wchan_wakeone(lock->l_wchan);    // wake up one
	lock->owner_thread = NULL; // Set owner thread to be null to indicate that no one owns the lock right now
   	spinlock_release(&lock->l_lock);
}

bool
lock_do_i_hold(struct lock *lock)
{
        // Write this
	KASSERT(lock != NULL);
   	spinlock_acquire(&lock->l_lock2);
	int comp = 0;
	if (lock->owner_thread == curthread) {
		comp = 1;
	}
	spinlock_release(&lock->l_lock2);
        return comp == 1; // if comp is 1, then the two address is equal, so currrent_thread does hold the lock
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
        struct cv *cv;

        cv = kmalloc(sizeof(struct cv));
        if (cv == NULL) {
                return NULL;
        }

        cv->cv_name = kstrdup(name);
        if (cv->cv_name==NULL) {
                kfree(cv);
                return NULL;
        }
        
        cv->cv_wchan = wchan_create(cv->cv_name);    // create a wait channel
        if (cv->cv_wchan == NULL) {
                kfree(cv->cv_name);
                kfree(cv);
                return NULL;
        }

	cv->associate_lock = NULL;
	cv->num_sleepers = 0;
        // add stuff here as needed
        
        return cv;
}

void
cv_destroy(struct cv *cv)
{
        KASSERT(cv != NULL);

        // add stuff here as needed
        
        kfree(cv->cv_name);
        kfree(cv->cv_wchan);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
        // Write this
	KASSERT(cv != NULL);
	KASSERT(lock != NULL);
	KASSERT(cv->num_sleepers >= 0);
        bool ifHold = lock_do_i_hold(lock);
        KASSERT(ifHold);   // check the if the current thread that calls this function does hold the lock. If not, there is something wrong with the user program, that 
                           // it is calling cv_wait outside of the critical section
        cv->num_sleepers ++;
        cv->associate_lock = lock;
	wchan_lock(cv->cv_wchan);
        lock_release(lock);
	wchan_sleep(cv->cv_wchan);

        lock_acquire(lock);   //  acquire the lock again when wake up
	
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
        // Write this
	KASSERT(cv != NULL);
        KASSERT(lock != NULL);
	KASSERT(cv->num_sleepers >= 0);
	bool ifHold = lock_do_i_hold(lock);
        KASSERT(ifHold);   // check the if the current thread that calls this function does hold the lock. If not, there is something wrong with the user program, that 
			   // it is calling cv_signal outside of the critical section

	KASSERT(lock == cv->associate_lock || cv->associate_lock == NULL); // check if cv_signal is calling the correct lock. It will catch the error where the user is calling this cv with the wrong lock.
								           // Note that NULL value is acceptable because it is possible that no one is in the blocking state when signal is called
	if (cv->num_sleepers > 0) {
	   wchan_wakeone(cv->cv_wchan);
	   cv->num_sleepers --;
	}
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	// Write this
        KASSERT(cv != NULL);
        KASSERT(lock != NULL);
        KASSERT(cv->num_sleepers >= 0);
        bool ifHold = lock_do_i_hold(lock);
        KASSERT(ifHold);   // check the if the current thread that calls this function does hold the lock. If not, there is something wrong with the user program, that 
                           // it is calling cv_signal outside of the critical section. Catch the error calling cv outside of the critical section. 

        KASSERT(lock == cv->associate_lock || cv->associate_lock == NULL);    // check if cv_signal is calling the correct lock. It will catch the error where the user is calling this cv with the wrong lock
									      // note that NULL value is acceptable because it is possible that no one is in the blocking state when signal is called
	wchan_wakeall(cv->cv_wchan);  // wake up everyone in the wait channel
	cv->num_sleepers = 0;
}
