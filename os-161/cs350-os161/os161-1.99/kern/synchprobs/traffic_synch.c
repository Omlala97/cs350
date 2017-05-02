#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
//static struct semaphore *intersectionSem;






static int *volatile situation[12];      // array that represents the actual traffic inside the crital section
					 // 0,1,2 represents north to south, east, west;
					 // 3,4,5 represents south to north, east, west;
					 // 6,7,8 represents east to  north, south, west;
					 // 9,10,11 represents west to north, south, east;
static struct lock *guard;   //   only allow one thread to modify the situation array.

static struct cv *n_to_s;    //   cv for each 12 different directions
static struct cv *n_to_e;
static struct cv *n_to_w;

static struct cv *s_to_n;
static struct cv *s_to_e;
static struct cv *s_to_w;

static struct cv *e_to_n;
static struct cv *e_to_s;
static struct cv *e_to_w;

static struct cv *w_to_n;
static struct cv *w_to_s;
static struct cv *w_to_e;
/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */

  guard = lock_create("guard");
  if (guard == NULL) {
    panic("could not create guard lock");
  }
// north region ***************************************
  n_to_s = cv_create("n_to_s");
  if (n_to_s == NULL) {
    panic("could not create cv n_to_s");
  }
  n_to_e = cv_create("n_to_e");
  if (n_to_e == NULL) {
    panic("could not create cv n_to_e");
  }
  n_to_w = cv_create("n_to_w");
  if (n_to_w == NULL) {
    panic("could not create cv n_to_w");
  }


// south region **************************************
  s_to_e = cv_create("s_to_e");
  if (s_to_e == NULL) {
    panic("could not create cv s_to_e");
  }
  s_to_n = cv_create("s_to_n");
  if (s_to_n == NULL) {
    panic("could not create cv s_to_n");
  }
  s_to_w = cv_create("s_to_w");
  if (s_to_w == NULL) {
    panic("could not create cv s_to_w");
  }


// east region *****************************************
  e_to_s = cv_create("e_to_s");
  if (e_to_s == NULL) {
    panic("could not create cv e_to_s");
  }
  e_to_n = cv_create("e_to_n");
  if (e_to_n == NULL) {
    panic("could not create cv e_to_n");
  }
  e_to_w = cv_create("e_to_w");
  if (e_to_w == NULL) {
    panic("could not create cv e_to_w");
  }


// west region ***************************************


  w_to_s = cv_create("w_to_s");
  if (w_to_s == NULL) {
    panic("could not create cv w_to_s");
  }
  w_to_e = cv_create("w_to_e");
  if (w_to_e == NULL) {
    panic("could not create cv w_to_e");
  }
  w_to_n = cv_create("w_to_n");
  if (w_to_n == NULL) {
    panic("could not create cv w_to_n");
  }

  for (int i = 0; i < 12; i++) {
    situation[i] = 0;
  }
  return;
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
  KASSERT(guard != NULL);

  KASSERT(n_to_s != NULL);
  KASSERT(n_to_e != NULL);
  KASSERT(n_to_w != NULL);

  KASSERT(s_to_e != NULL);
  KASSERT(s_to_n != NULL);
  KASSERT(s_to_w != NULL);

  KASSERT(e_to_s != NULL);
  KASSERT(e_to_n != NULL);
  KASSERT(e_to_w != NULL);

  KASSERT(w_to_s != NULL);
  KASSERT(w_to_n != NULL);
  KASSERT(w_to_e != NULL);

  cv_destroy(n_to_s);
  cv_destroy(n_to_e);
  cv_destroy(n_to_w);

  cv_destroy(s_to_e);
  cv_destroy(s_to_w);
  cv_destroy(s_to_n);

  cv_destroy(e_to_s);
  cv_destroy(e_to_n);
  cv_destroy(e_to_w);

  cv_destroy(w_to_s);
  cv_destroy(w_to_e);
  cv_destroy(w_to_n);

  lock_destroy(guard);
  return;
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
//  KASSERT(intersectionSem != NULL);
//  P(intersectionSem);
  KASSERT(n_to_s != NULL);
  KASSERT(n_to_e != NULL);
  KASSERT(n_to_w != NULL);

  KASSERT(s_to_e != NULL);
  KASSERT(s_to_n != NULL);
  KASSERT(s_to_w != NULL);

  KASSERT(e_to_s != NULL);
  KASSERT(e_to_n != NULL);
  KASSERT(e_to_w != NULL);

  KASSERT(w_to_s != NULL);
  KASSERT(w_to_n != NULL);
  KASSERT(w_to_e != NULL);

  KASSERT(guard != NULL);
  switch (origin)
    {
    case north:
	if (destination == south) {// north tp south
	   lock_acquire(guard);
	   while (1) {
	       if (situation[8] != 0 || situation[11] != 0 || situation[7] != 0 || situation[10] != 0 || situation[9] != 0 || situation[5] != 0) {// es, ws, ew, we, wn,sw
	       	     	cv_wait(n_to_s, guard);
	       } else  {
		     	situation[0] ++;
			break;
	       }
	   }
	   lock_release(guard);
	} else if ( destination == east) { // north to east, left turn
          lock_acquire(guard);
	  while(1)  {
               if (situation[4] != 0 || situation[11] != 0 || situation[5] != 0 || situation[3] != 0 || situation[7] != 0 || situation[9] != 0 || situation[8] != 0) { //se, se, sn, sw, es, ew, wn
                        cv_wait(n_to_e, guard);
               } else  {
                        situation[1] ++;
                        break;
               }
           }
           lock_release(guard);
	} else { // north to west, right turn
           lock_acquire(guard);
           while(1) {
               if (situation[8] != 0 || situation[5] != 0) {// ew, sw
                        cv_wait(n_to_w, guard);
               } else  {
                        situation[2] ++;
                        break;
               }
           }
           lock_release(guard);
       }
      break;

    case east:
        if (destination == south) { // east to south, left turn
           lock_acquire(guard);
           while(1) {
               if (situation[0] != 0 || situation[10] != 0 || situation[1] != 0 || situation[11] != 0 || situation[9] != 0 || situation[5] != 0 || situation[3] != 0) {// sw, wn, ne, we, ws, ns, sn
                        cv_wait(e_to_s, guard);
               } else  {
                        situation[7] ++;
                        break;
               }
           }
           lock_release(guard);
	} else if ( destination == west) {// east to west, straight line
           lock_acquire(guard);
           while(1) {
               if (situation[2] != 0 || situation[0] != 0 || situation[3] != 0 || situation[9] != 0 || situation[1] != 0 || situation[5] != 0) {//ns , sn, nw, sw, ne, wn
                        cv_wait(e_to_w, guard);
               } else  {
                        situation[8] ++;
                        break;
               }
           }
           lock_release(guard);
       } else {// east to north, right turn
	   lock_acquire(guard);
           while(1) {
               if (situation[3] != 0 || situation[9] != 0) {// wn, sn
                        cv_wait(e_to_n, guard);
               } else  {
                        situation[6] ++;
                        break;
               }
           }
           lock_release(guard);
        }
      break;

    case south:
        if (destination == north) {// south to north, straight line
           lock_acquire(guard);
           while(1) {
               if (situation[11] != 0 || situation[8] != 0 || situation[9] != 0 || situation[6] != 0 || situation[7] != 0 || situation[1] != 0) {// ew, we, wn, en, ne, es
                        cv_wait(s_to_n, guard);
               } else  {
                        situation[3] ++;
                        break;
               }
           }
           lock_release(guard);
        } else if ( destination == west) { // south to west, left turn
           lock_acquire(guard);
           while(1) {
               if (situation[8] != 0 || situation[2] != 0 || situation[11] != 0 || situation[0] != 0 || situation[7] != 0 || situation[1] != 0 || situation[9] != 0) {// wn, ne, es, ew, we, ns, nw
                        cv_wait(s_to_w, guard);
               } else  {
                        situation[5] ++;
                        break;
               }
           }
           lock_release(guard);
	} else { // south to east, right turn
           lock_acquire(guard);
           while(1) {
               if (situation[11] != 0 || situation[1] != 0) { // ne, we
                        cv_wait(s_to_e, guard);
               } else  {
                        situation[4] ++;
                        break;
               }
           }
           lock_release(guard);
      	}
      break;

    case west:
       if (destination == east) {// west to east, straight line
           lock_acquire(guard);
           while(1) {
               if (situation[0] != 0 || situation[3] != 0 || situation[1] != 0 || situation[4] != 0 || situation[7] != 0 || situation[5] != 0) {//ns, sn, ne, se, sw, es
                        cv_wait(w_to_e, guard);
               } else  {
                        situation[11] ++;
                        break;
               }
           }
           lock_release(guard);
	} else if ( destination == north) {// west to north, left turn
           lock_acquire(guard);
           while(1) {
               if (situation[3] != 0 || situation[6] != 0 || situation[7] != 0 || situation[1] != 0 || situation[5] != 0 || situation[8] != 0 || situation[0] != 0) { // ne, es,sw,ew,sn,ns,en
                        cv_wait(w_to_n, guard);
               } else  {
                        situation[9] ++;
                        break;
               }
           }
           lock_release(guard);
        } else {//west to south, right turn
           lock_acquire(guard);
           while(1) {
               if (situation[0] != 0 || situation[7] != 0) {// es, ns
                        cv_wait(w_to_s, guard);
               } else  {
                        situation[10] ++;
                        break;
               }
           }
           lock_release(guard);
        }
      break;
    }

    return;
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
  KASSERT(n_to_s != NULL);
  KASSERT(n_to_e != NULL);
  KASSERT(n_to_w != NULL);

  KASSERT(s_to_e != NULL);
  KASSERT(s_to_n != NULL);
  KASSERT(s_to_w != NULL);

  KASSERT(e_to_s != NULL);
  KASSERT(e_to_n != NULL);
  KASSERT(e_to_w != NULL);

  KASSERT(w_to_s != NULL);
  KASSERT(w_to_n != NULL);
  KASSERT(w_to_e != NULL);

  KASSERT(guard != NULL);
  lock_acquire(guard);
  switch (origin)
    {
    case north:
      	if (destination == east) {// ne,1
	    	situation[1] --;
                cv_broadcast(e_to_s, guard);
                cv_broadcast(e_to_w, guard);
                cv_broadcast(s_to_n, guard);
                cv_broadcast(s_to_w, guard);
                cv_broadcast(s_to_e, guard);
                cv_broadcast(w_to_e, guard);
                cv_broadcast(w_to_n, guard);
	} else if (destination == south) { // ns
		situation[0] --;
                cv_broadcast(e_to_s, guard);
                cv_broadcast(e_to_w, guard);
                cv_broadcast(s_to_w, guard);
                cv_broadcast(w_to_e, guard);
                cv_broadcast(w_to_n, guard);
                cv_broadcast(w_to_s, guard);
	} else {// nw
		situation[2] --;
                cv_broadcast(e_to_w, guard);
                cv_broadcast(s_to_w, guard);
	}
      break;
    case east:
        if (destination == west) { // east to west
                situation[8] --;
                cv_broadcast(n_to_s, guard);
                cv_broadcast(n_to_e, guard);
                cv_broadcast(n_to_w, guard);
                cv_broadcast(s_to_n, guard);
                cv_broadcast(s_to_w, guard);
                cv_broadcast(w_to_n, guard);
        } else if (destination == south) { // east to south
                situation[7] --;
                cv_broadcast(n_to_s, guard);
                cv_broadcast(n_to_e, guard);
                cv_broadcast(s_to_n, guard);
                cv_broadcast(s_to_w, guard);
                cv_broadcast(w_to_e, guard);
                cv_broadcast(w_to_n, guard);
                cv_broadcast(w_to_s, guard);
        } else { // east to north
                situation[6] --;
                cv_broadcast(s_to_n, guard);
                cv_broadcast(w_to_n, guard);
        }
      break;
    case south:
        if (destination == east) { // south to east
                situation[4] --;
                cv_broadcast(n_to_e, guard);
                cv_broadcast(w_to_e, guard);
        } else if (destination == west) { // south to west
                situation[5] --;
                cv_broadcast(n_to_s, guard);
                cv_broadcast(n_to_e, guard);
                cv_broadcast(n_to_w, guard);
                cv_broadcast(e_to_s, guard);
                cv_broadcast(e_to_w, guard);
                cv_broadcast(w_to_e, guard);
                cv_broadcast(w_to_n, guard);
        } else {  // south to north
                situation[3] --;
                cv_broadcast(n_to_e, guard);
                cv_broadcast(e_to_s, guard);
                cv_broadcast(e_to_w, guard);
                cv_broadcast(e_to_n, guard);
                cv_broadcast(w_to_e, guard);
                cv_broadcast(w_to_n, guard);
        }
      break;
    case west:
        if (destination == east) { // west to east
                situation[11] --;
                cv_broadcast(n_to_s, guard);
                cv_broadcast(n_to_e, guard);
                cv_broadcast(e_to_s, guard);
                cv_broadcast(s_to_n, guard);
                cv_broadcast(s_to_w, guard);
                cv_broadcast(s_to_e, guard);
        } else if (destination == south) { // west to south
                situation[10] --;
                cv_broadcast(n_to_s, guard);
                cv_broadcast(e_to_s, guard);
        } else { // west to north
                situation[9] --;
                cv_broadcast(n_to_s, guard);
                cv_broadcast(n_to_e, guard);
                cv_broadcast(e_to_s, guard);
                cv_broadcast(e_to_w, guard);
                cv_broadcast(e_to_n, guard);
                cv_broadcast(s_to_n, guard);
                cv_broadcast(s_to_w, guard);
        }
      break;
    }
    lock_release(guard);
    return;
}
