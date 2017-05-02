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

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include "opt-A3.h"
/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground.
 */

/* under dumbvm, always have 48k of user stack */
#define DUMBVM_STACKPAGES    12

/*
 * Wrap rma_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

#if OPT_A3
struct page_table_entry {  // page table entry store a physical address, represents the address of the physical frame 
	paddr_t p;
};

struct cEntry {
	int free;
	int size;
	paddr_t pa;	// physical  address
};
struct cEntry* coremap = NULL;
unsigned frameNum = 0;
unsigned frameSize = 0;
#endif // OPT_A3

#if OPT_A3
void print_stuff(void) {
	for (unsigned i = 0; i < frameNum; i++) {
                DEBUG(DB_VM, " %d ", coremap[i].free);
        } 
	DEBUG(DB_VM, " \n");
}
#endif // OPT_A3

void
vm_bootstrap(void)
{
#if OPT_A3
	/* Do somthing. */
	paddr_t lo;
	paddr_t hi;
	ram_getsize(&lo , &hi);
	// do not call ram_stealmem again!
	frameNum = (hi -lo) / PAGE_SIZE;
	frameSize = ROUNDUP(sizeof(struct cEntry) * frameNum, PAGE_SIZE )/ PAGE_SIZE;
	coremap = (struct cEntry*)PADDR_TO_KVADDR(lo);
	for (unsigned i =0; i< frameNum; i++) {
		coremap[i].free = 1; // it is not used, i.e, the frame is free
		coremap[i].size = 0;
		if (i <= frameSize) { // first three blocks give to the coremap
			coremap[i].free = 0; // they are used, so do not touch them
		}
		coremap[i].pa =  lo  + i * PAGE_SIZE;
	}
#endif // OPT-A3
}

static
paddr_t
getppages(unsigned long npages)
{
	paddr_t addr = 0;
#if OPT_A3
    	DEBUG(DB_VM, "guys I want this much npages %lu\n ", npages);
        unsigned count;
        bool flag;      // record if the next npages are all free
        spinlock_acquire(&stealmem_lock);
	if(coremap == NULL) {
		addr = ram_stealmem(npages);
	} else {
		KASSERT(frameNum != 0);   // frameNum is not 0
		for (unsigned i = 0; i < frameNum; i++) {
			if (i == frameNum -1 && coremap[i].free == 0) { // this is the last one and the last one is not free. Then obviously we cannot find any free mem. Crucial check because of page table
				DEBUG(DB_VM, "guys I am not able to allocate mem now %d \n", i);
                                print_stuff();
                                spinlock_release(&stealmem_lock);
				return 0;
			}
			if (coremap[i].free == 1) {   // if is free, we need check if we can allocate the next npages
				if (i + npages > frameNum) {	// this case, there are not enough memory left
					DEBUG(DB_VM, "guys I am not able to allocate mem now %d \n", i);
					print_stuff();
					spinlock_release(&stealmem_lock);
					return 0;
				} else {
					count = 0;
					flag = true;
					while ( count != npages) {					// this loop is look ahead, checking if the next npages are all free
						if (coremap[i+count].free != 1) {			// if any of the pages are not free, stop looking move to the next one
							flag = false;
							break;
						}
						count ++;
					}  // end of while loop
					if (flag) {     						// there are npages of free memory, so we can allocate it.
						count = 0;
						coremap[i].size = npages;
						while (count != npages) {
							coremap[i+count].free = 0;
							count++;
						}
						addr = coremap[i].pa;
 						DEBUG(DB_VM, " guys I just allocate some mem from %d \n", i);
						print_stuff();
						spinlock_release(&stealmem_lock);
                                        	return addr;
					} else {							// cannot allocate npages of free memory. Go searching for the next block
						count = 0;
						continue;
					}
				}
			}
		} // end of for loop
	}
#else
	addr = ram_stealmem(npages);
#endif // OPT_A3
#if OPT_A3 
	DEBUG(DB_VM, " guys the core map is not ready \n");
	print_stuff();
	spinlock_release(&stealmem_lock);
#endif // OPT_A3
	return addr;
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t 
alloc_kpages(int npages)
{
	paddr_t pa;
	pa = getppages(npages);
	if (pa==0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

void 
free_kpages(vaddr_t addr)
{
	/* nothing - leak the memory. */
#if OPT_A3
	int size = 0;
	paddr_t p = addr - MIPS_KSEG0; // physical address, we are from kseg0 so just minus that macro to get the physical address
	if (coremap == NULL) {		// coremap is not ready, so we have to leak the memory
		return;
	}
	spinlock_acquire(&stealmem_lock);
	DEBUG(DB_VM, " I am starting to free memory \n ");
	for (unsigned i =0; i < frameNum; i++) {
		if (coremap[i].pa == p) { // found the block we wanna free
			DEBUG(DB_VM, " hey coremap[i] size is %d \n ", coremap[i].size);
			while (size < coremap[i].size) {
				coremap[i+size].free = 1;
				size++;
			}
                        coremap[i].size = 0;
			break;
		}
	}
	print_stuff();
	DEBUG(DB_VM, "I am done freeing these stuff\n ");
	spinlock_release(&stealmem_lock);
#else
	(void)addr;
#endif // OPT_A3
}

void
vm_tlbshootdown_all(void)
{
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	faultaddress &= PAGE_FRAME;

//	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
		/* We always create pages read-write, so we can't get this */
#if OPT_A3
            return EFAULT;
#else
	panic("dumbvm: got VM_FAULT_READONLY\n");
#endif // OPT-A3
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = curproc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	KASSERT(as->as_vbase1 != 0);
	KASSERT(as->as_pbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	KASSERT(as->as_pbase2 != 0);
	KASSERT(as->as_npages2 != 0);
	KASSERT(as->as_stackpbase != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
#if OPT_A3
	// the assert does not apply as the type is diffrent
#else
        KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);
#endif // OPT_A3

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	if (faultaddress >= vbase1 && faultaddress < vtop1) {
#if OPT_A3
		paddr = as->as_pbase1[(faultaddress-vbase1) / PAGE_SIZE].p;     // calculate the page number, and go into the page table to get the actual physical address
#else
		paddr = (faultaddress - vbase1) + as->as_pbase1;
#endif // OPT_A3
	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
#if OPT_A3
		paddr = as->as_pbase2[(faultaddress-vbase2) / PAGE_SIZE].p;     // calculate the page number, and go into the page table to get the actual physical address
#else
		paddr = (faultaddress - vbase2) + as->as_pbase2;
#endif // OPT_A3
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
#if OPT_A3
		paddr = as->as_stackpbase[(faultaddress-stackbase) / PAGE_SIZE].p;
#else
		paddr = (faultaddress - stackbase) + as->as_stackpbase;
#endif// OPT_A3
	}
	else {
		return EFAULT;
	}

	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultaddress;
#if OPT_A3
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	 	if (as->flag  && faultaddress >= vbase1 && faultaddress < vtop1) {
                	elo &= ~TLBLO_DIRTY;
        	}
#else
  		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
#endif // OPT-A3
//		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}
#if OPT_A3
	ehi = faultaddress;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
       	if (as->flag && faultaddress >= vbase1 && faultaddress < vtop1) {
                       elo &= ~TLBLO_DIRTY;
        }
	tlb_random(ehi, elo);
	splx(spl);
	return 0;
#else
	kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
	splx(spl);
	return EFAULT;
#endif/* OPT_A3 */
}

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

	as->as_vbase1 = 0;
	as->as_pbase1 = 0;
	as->as_npages1 = 0;
	as->as_vbase2 = 0;
	as->as_pbase2 = 0;
	as->as_npages2 = 0;
	as->as_stackpbase = 0;
#if OPT_A3
	as->flag = 0;  // used to indicate if loadif is done
#endif/*OPT_A3*/
	return as;
}

void
as_destroy(struct addrspace *as)
{
#if OPT_A3
    DEBUG(DB_VM, " guys I am destroying this as \n");

/*After introducing page table, we need to free the page table one by one*/
for (unsigned i = 0; i < as->as_npages1; i++) {
	free_kpages(PADDR_TO_KVADDR(as->as_pbase1[i].p));
}
for (unsigned i = 0; i < as->as_npages2; i++) {
        free_kpages(PADDR_TO_KVADDR(as->as_pbase2[i].p));
}
for (unsigned i = 0; i < DUMBVM_STACKPAGES; i++) {
        free_kpages(PADDR_TO_KVADDR(as->as_stackpbase[i].p));
}
// now free the page table structure itself
kfree(as->as_stackpbase);
kfree(as->as_pbase1);
kfree(as->as_pbase2);
/**********Before introducing page tables, free each segment as a whole***************/
//    these code will work with segmentation only, no page table 
//    free_kpages(PADDR_TO_KVADDR(as->as_stackpbase));
//    free_kpages(PADDR_TO_KVADDR(as->as_pbase1));
//    free_kpages(PADDR_TO_KVADDR(as->as_pbase2));
    kfree(as);
#else
	kfree(as);
#endif // OPT_A3
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = curproc_getas();
#ifdef UW
        /* Kernel threads don't have an address spaces to activate */
#endif
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/* nothing */
}

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	size_t npages; 

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* We don't use these - all pages are read-write */
	(void)readable;
	(void)writeable;
	(void)executable;

	if (as->as_vbase1 == 0) {
		as->as_vbase1 = vaddr;
		as->as_npages1 = npages;
		return 0;
	}

	if (as->as_vbase2 == 0) {
		as->as_vbase2 = vaddr;
		as->as_npages2 = npages;
		return 0;
	}

	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return EUNIMP;
}

static
void
as_zero_region(paddr_t paddr, unsigned npages)
{
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}

int
as_prepare_load(struct addrspace *as)
{
#if OPT_A3
	// add page table
	// allcoate three page tables, one for each segment
	as->as_pbase1 = kmalloc(sizeof(struct page_table_entry) * as->as_npages1);
	if (as->as_pbase1 == NULL) {
		return ENOMEM;
	}
	as->as_pbase2 = kmalloc(sizeof(struct page_table_entry) * as->as_npages2);
        if (as->as_pbase2 == NULL) {
                return ENOMEM;
        }
	as->as_stackpbase = kmalloc(sizeof(struct page_table_entry) * DUMBVM_STACKPAGES);
        if (as->as_stackpbase == NULL) {
                return ENOMEM;
        }
	// allocate memory page by page, and store thier physical addres into the page table
	for (unsigned i = 0; i < as->as_npages1; i++) {
		as->as_pbase1[i].p = getppages(1);
		if (as->as_pbase1[i].p == 0) {
			return ENOMEM;
		}
		as_zero_region(as->as_pbase1[i].p, 1);
	}
        for (unsigned i = 0; i < as->as_npages2; i++) {
                as->as_pbase2[i].p = getppages(1);
                if (as->as_pbase2[i].p == 0) {
                        return ENOMEM;
                }
                as_zero_region(as->as_pbase2[i].p, 1);
        }
        for (unsigned i = 0; i < DUMBVM_STACKPAGES; i++) {
                as->as_stackpbase[i].p = getppages(1);
                if (as->as_stackpbase[i].p == 0) {
                        return ENOMEM;
                }
                as_zero_region(as->as_stackpbase[i].p, 1);
        }

	
#else		// original implementation
	KASSERT(as->as_pbase1 == 0);
        KASSERT(as->as_pbase2 == 0);
        KASSERT(as->as_stackpbase == 0);
	as->as_pbase1 = getppages(as->as_npages1);
	if (as->as_pbase1 == 0) {
		return ENOMEM;
	}

	as->as_pbase2 = getppages(as->as_npages2);
	if (as->as_pbase2 == 0) {
		return ENOMEM;
	}

	as->as_stackpbase = getppages(DUMBVM_STACKPAGES);
	if (as->as_stackpbase == 0) {
		return ENOMEM;
	}
	as_zero_region(as->as_pbase1, as->as_npages1);
	as_zero_region(as->as_pbase2, as->as_npages2);
	as_zero_region(as->as_stackpbase, DUMBVM_STACKPAGES);
#endif // OPT_A3
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	(void)as;
#if OPT_A3
	as_activate();
	as->flag = 1;
#endif // OPT_A3
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	KASSERT(as->as_stackpbase != 0);

	*stackptr = USERSTACK;
	return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *new;

	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

	new->as_vbase1 = old->as_vbase1;
	new->as_npages1 = old->as_npages1;
	new->as_vbase2 = old->as_vbase2;
	new->as_npages2 = old->as_npages2;
#if OPT_A3 
	// allocate space for the page tables
        new->as_pbase1 = kmalloc(sizeof(struct page_table_entry) * new->as_npages1);
        if (new->as_pbase1 == NULL) {
                return ENOMEM;
        }
        new->as_pbase2 = kmalloc(sizeof(struct page_table_entry) * new->as_npages2);
        if (new->as_pbase2 == NULL) {
                return ENOMEM;
        }
        new->as_stackpbase = kmalloc(sizeof(struct page_table_entry) * DUMBVM_STACKPAGES);
        if (new->as_stackpbase == NULL) {
                return ENOMEM;
        }
#endif // OPT_A3


	/* (Mis)use as_prepare_load to allocate some physical memory. */
	if (as_prepare_load(new)) {
		as_destroy(new);
		return ENOMEM;
	}

	KASSERT(new->as_pbase1 != 0);
	KASSERT(new->as_pbase2 != 0);
	KASSERT(new->as_stackpbase != 0);

#if OPT_A3
	for (unsigned i =0; i < new->as_npages1; i++) {			     // copy the stuff
		memmove((void *)PADDR_TO_KVADDR((new->as_pbase1[i]).p),      // destination
			(const void *)PADDR_TO_KVADDR(old->as_pbase1[i].p),  // src
			PAGE_SIZE);					     // size is page_size because we move page by page 
	}
        for (unsigned i =0; i < new->as_npages2; i++) {			     // copy the stuff
                memmove((void *)PADDR_TO_KVADDR((new->as_pbase2[i]).p),      // destination
                        (const void *)PADDR_TO_KVADDR(old->as_pbase2[i].p),  // src
                        PAGE_SIZE);                                          // size is page_size  because we move page by page 
        }
        for (unsigned i =0; i < DUMBVM_STACKPAGES; i++) {		     // copy the stuff in the stack
                memmove((void *)PADDR_TO_KVADDR((new->as_stackpbase[i]).p),      // destination
                        (const void *)PADDR_TO_KVADDR(old->as_stackpbase[i].p),  // src
                        PAGE_SIZE);                                          // size is page_size because we move page by page 
        }



#else
	memmove((void *)PADDR_TO_KVADDR(new->as_pbase1),
		(const void *)PADDR_TO_KVADDR(old->as_pbase1),
		old->as_npages1*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_pbase2),
		(const void *)PADDR_TO_KVADDR(old->as_pbase2),
		old->as_npages2*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase),
		(const void *)PADDR_TO_KVADDR(old->as_stackpbase),
		DUMBVM_STACKPAGES*PAGE_SIZE);
#endif// OPT_A3
	*ret = new;
	return 0;
}
