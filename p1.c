
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
    for (int i = 0; i < me->numPages; ++i) {
        me->pageTable[i].state = UNUSED;
        me->pageTable[i].frame = -1;
        me->pageTable[i].diskBlock = -1;
    }

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

    Process * me = getProc(pid);

    int tag;
    int mmuStatus = USLOSS_MmuGetTag(&tag);
    if (mmuStatus != USLOSS_MMU_OK){
        if (p1debug){
            USLOSS_Console("Pager(): mmu gettag failed\n");
        }
        //Terminate(1);
    }

    // Clean out frame table entries
    if (me->pageTable != NULL) {
        for (int i = 0; i < me->numPages; ++i) {
            if (me->pageTable[i].frame != -1) {
                mmuStatus = USLOSS_MmuUnmap(tag, i);
                if (mmuStatus != USLOSS_MMU_OK){
                    if (p1debug){
                        USLOSS_Console("Pager(): mmu map failed\n");
                    }
                    //Terminate(1);
                }
                vmStats.freeFrames++;
            }
        }
    }

    // TODO: Clean out disk entries?


    me->pid = -1;
    MboxRelease(me->faultMbox);
    me->status = UNUSED;
    me->numPages = -1;
    free(me->pageTable);

} /* p1_quit */
