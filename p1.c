
#include "usloss.h"
#include <usyscall.h>
#include <assert.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <phase5.h>
#include <libuser.h>
#include "providedPrototypes.h"
#include <vm.h>
#include <string.h>

int p1debug = 0;

extern Process * getProc();
extern int initialized;


void
p1_fork(int pid)
{
    if (!initialized && pid != 8) {
        return;
    }
    if (p1debug)
        USLOSS_Console("p1_fork() called: pid = %d\n", pid);

    //get my process
    Process * me = getProc(pid);

    //set my proc table stuff
    me->status = OCCUPIED;
    me->pid = pid;

    //allocate the pagetable
    me->pageTable = (PTE *) malloc(me->numPages * sizeof(PTE));

} /* p1_fork */

void
p1_switch(int old, int new)
{
    if (!initialized) {
        return;
    }
    if (p1debug)
        USLOSS_Console("p1_switch() called: old = %d, new = %d\n", old, new);
    if (initialized){
        vmStats.switches++;
    }
} /* p1_switch */

void
p1_quit(int pid)
{
    if (!initialized) {
        return;
    }
    if (p1debug)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);
} /* p1_quit */
