
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
extern int statsMutex;

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
        MboxSend(statsMutex, NULL, 0);
        vmStats.switches++;
        MboxReceive(statsMutex, NULL, 0);
    

        Process* oldProc = getProc(old);
        Process* newProc = getProc(new);



        PTE* oldPages = oldProc->pageTable;

        if (p1debug){
            USLOSS_Console("p1_switch(): starting to unmap old %p\n", oldPages);
        }

        int mmuStatus;
        //unmap old process pages
        if (oldPages != NULL){
            for (int i = 0; i < oldProc->numPages; i++){

                if (oldPages[i].state == INFRAME){
                    mmuStatus = USLOSS_MmuUnmap(TAG, i);
                    if (p1debug){
                        USLOSS_Console("p1_switch(): unmapped page %d from old proc %d\n", i, old);
                    }
                    if (mmuStatus != USLOSS_MMU_OK){
                        if (p1debug){
                            USLOSS_Console("p1_switch(): mmu unmap failed\n");
                        }
                        quit(1);
                    } 
                }
                
            }
        }

        if (p1debug){
            USLOSS_Console("p1_switch(): finished unmapping old\n");
        }

        PTE* newPages = newProc->pageTable;

        //map new process pages
        if (newPages != NULL){
            for (int i = 0; i < newProc->numPages; i++){
                // TODO: Check if two pages are mapped to the same frame
                if (newPages[i].state == INFRAME){
                    mmuStatus = USLOSS_MmuMap(TAG, i, newPages[i].frame, USLOSS_MMU_PROT_RW );
                    if (p1debug){
                        USLOSS_Console("p1_switch(): mapped page %d to frame %d for new proc %d\n", i, newPages[i].frame, new);
                    }
                    if (mmuStatus != USLOSS_MMU_OK){
                        if (p1debug){
                            USLOSS_Console("p1_switch(): mmu map failed\n");
                        }
                        quit(1);
                    }
                }
               
            }
        }

        if (p1debug){
            USLOSS_Console("p1_switch(): finished mapping new\n");
        }
    }
    

    // int    USLOSS_MmuUnmap(int tag, int page)
} /* p1_switch */

int otherProcsUsingFrame(int myPid, int frame) {
    for (int i = 11; i < MAXPROC; i++) {
        if (i == myPid) {
            continue;
        }
        Process * proc = getProc(i);
        if (proc != NULL && proc->pageTable != NULL) {
            for (int j = 0; j < proc->numPages; ++j) {
                if (proc->pageTable[j].frame == frame) {
                    return 1;
                }
            }
        }
    }

    return 0;
}

void
p1_quit(int pid)
{
    if (!initialized) {
        return;
    }
    if (p1debug)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);

    Process * me = getProc(pid);


    int mmuStatus;
    // Clean out frame table entries
    if (me->pageTable != NULL) {
        for (int i = 0; i < me->numPages; ++i) {
            if (me->pageTable[i].frame != -1) {
                mmuStatus = USLOSS_MmuUnmap(TAG, i);
                if (mmuStatus != USLOSS_MMU_OK){
                    if (p1debug){
                        USLOSS_Console("p1_quit(): mmu map failed\n");
                    }
                    //Terminate(1);
                }
                if (!otherProcsUsingFrame(pid, me->pageTable[i].frame)) {
                    MboxSend(statsMutex, NULL, 0);
                    vmStats.freeFrames++;
                    MboxReceive(statsMutex, NULL, 0);
                }
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
