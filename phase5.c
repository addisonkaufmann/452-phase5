/*
 * skeleton.c
 *
 * This is a skeleton for phase5 of the programming assignment. It
 * doesn't do much -- it is just intended to get you started.
 */

#include <usloss.h>
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

int debugFlag5 = 0;

extern int Mbox_Create(int numslots, int slotsize, int *mboxID);
extern void mbox_create(USLOSS_Sysargs *args_ptr);
extern void mbox_release(USLOSS_Sysargs *args_ptr);
extern void mbox_send(USLOSS_Sysargs *args_ptr);
extern void mbox_receive(USLOSS_Sysargs *args_ptr);
extern void mbox_condsend(USLOSS_Sysargs *args_ptr);
extern void mbox_condreceive(USLOSS_Sysargs *args_ptr);
extern int MboxCreate(int slots, int size);
extern int semcreateReal();

static void FaultHandler(int type, void * offset);
static void vmInit(USLOSS_Sysargs *USLOSS_SysargsPtr);
static void vmDestroy(USLOSS_Sysargs *USLOSS_SysargsPtr);
static int Pager(char *buf);
int numPages;
int startingFrame = 0;

int enterUserMode();
void * vmInitReal(int mappings, int pages, int frames, int pagers);
void initProcTable();
void vmDestroyReal(void);
void initFrameTable();
int scanForFrame();
int clockSweep();
Process* getProc();

void * vmRegion; //FIXME: not sure what this is supposed to be
static Process procTable[MAXPROC];

FaultMsg faults[MAXPROC]; /* Note that a process can have only
													 * one fault at a time, so we can
													 * allocate the messages statically
													 * and index them by pid. */
VmStats  vmStats;
FTE* frameTable;
int numFrames;
int faultMbox;
int * pagerPids;
int numPagers;
int statsMutex;
int initialized;
int destroy = 0;

FaultMsg* faultQueue;




/*
 *----------------------------------------------------------------------
 *
 * start4 --
 *
 * Initializes the VM system call handlers. 
 *
 * Results:
 *      MMU return status
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
int start4(char *arg)
{
	int pid;
	int result;
	int status;

	initialized = 0; // Have not done VmInit yet
	
	/* to get user-process access to mailbox functions */
	systemCallVec[SYS_MBOXCREATE]      = mbox_create;
	systemCallVec[SYS_MBOXRELEASE]     = mbox_release;
	systemCallVec[SYS_MBOXSEND]        = mbox_send;
	systemCallVec[SYS_MBOXRECEIVE]     = mbox_receive;
	systemCallVec[SYS_MBOXCONDSEND]    = mbox_condsend;
	systemCallVec[SYS_MBOXCONDRECEIVE] = mbox_condreceive;

	/* user-process access to VM functions */
	systemCallVec[SYS_VMINIT]    = vmInit;
	systemCallVec[SYS_VMDESTROY] = vmDestroy; 
	result = Spawn("Start5", start5, NULL, 8*USLOSS_MIN_STACK, 2, &pid);
	if (result != 0) {
			USLOSS_Console("start4(): Error spawning start5\n");
			Terminate(1);
	}
	result = Wait(&pid, &status);
	if (result != 0) {
			USLOSS_Console("start4(): Error waiting for start5\n");
			Terminate(1);
	}
	Terminate(0);
	return 0; // not reached

} /* start4 */

/*
 *----------------------------------------------------------------------
 *
 * VmInit --
 *
 * Stub for the VmInit system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is initialized.
 *
 *----------------------------------------------------------------------
 */
static void vmInit(USLOSS_Sysargs * args)
{
	CheckMode();

	int mappings = (long) (args->arg1);
	int pages = (long)(args->arg2);
	int frames = (long)(args->arg3);
	int pagers = (long)(args->arg4);

	void * addr = vmInitReal(mappings, pages, frames, pagers);
	long status = (long)addr;
	
	if (status == -1) { 
		args->arg4 = (void *) (-1);
	}
	else if (status == -2) {
		args->arg4 = (void *) (-2);
	}
	else {
		args->arg4 = (void *) (0);
		args->arg1 = addr;
		vmRegion = addr;
		initialized = 1;

	}

	if (debugFlag5) {
		USLOSS_Console("vmInit(): Returning with error of: %d and address: %d\n.", (long)args->arg4, addr);
	}
	
	enterUserMode();
} /* vmInit */


/*
 *----------------------------------------------------------------------
 *
 * vmDestroy --
 *
 * Stub for the VmDestroy system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is cleaned up.
 *
 *----------------------------------------------------------------------
 */

static void vmDestroy(USLOSS_Sysargs *USLOSS_SysargsPtr)
{
	CheckMode();
	if (!initialized) {
		return;
	}
	vmDestroyReal();
	enterUserMode();
} /* vmDestroy */


/*
 *----------------------------------------------------------------------
 *
 * vmInitReal --
 *
 * Called by vmInit.
 * Initializes the VM system by configuring the MMU and setting
 * up the page tables.
 *
 * Results:
 *      Address of the VM region.
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
void * vmInitReal(int mappings, int pages, int frames, int pagers)
{
	int status;
	int dummy;

	CheckMode();

	// Check incorrect arguments
	if (mappings != pages || mappings < 0 || pages < 0 || frames < 0 || pagers < 0 || pagers > MAXPAGERS) {
		return (void *) -1;
	}
	// Check if the VM region has already been initialized
	if (USLOSS_MmuRegion(&dummy) != NULL) {
		return (void *) -2;
	}

	if (debugFlag5) {
		USLOSS_Console("vmInitReal(): called and passed by error handeling.\n");
	}

	status = USLOSS_MmuInit(mappings, pages, frames, USLOSS_MMU_MODE_TLB);
	if (status != USLOSS_MMU_OK) {
		USLOSS_Console("vmInitReal: couldn't initialize MMU, status %d\n", status);
		abort();
	}
	USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;



	
	/*
	* Initialize the frame table.
	*/
	initProcTable();
	initFrameTable(frames);


	/*
	* Initialize page tables.
	*/
	for (int i = 0; i < MAXPROC; ++i) {
		procTable[i].numPages = pages; // Set every procs' numPages but wait for them to be forked to actually initialize the page table.
		numPages = pages;
	}

	/* 
	* Create the fault mailbox.
	*/
	faultMbox = MboxCreate(1, 0);

	/*
	* Fork the pagers.
	*/
	pagerPids = malloc(pagers * sizeof(int));
	char temp[16];
	char name[16];
	for (int i = 0; i < pagers; ++i) {
		sprintf(temp, "%d", i);
		sprintf(name, "Pager%d", i);
		int pid = fork1(name, Pager, temp, 8*USLOSS_MIN_STACK, 2);
		procTable[pid % MAXPROC].status = OCCUPIED;
		procTable[pid % MAXPROC].pid = pid;
		pagerPids[i] = pid; // Track pager pids to be able to kill them later
	}
	numPagers = pagers;

	/*
	* Zero out, then initialize, the vmStats structure
	*/
	memset((char *) &vmStats, 0, sizeof(VmStats));

	/*
	* Initialize vmStats fields.
	*/
	int sector; 
	int track; 
	int disk;

	diskSizeReal(1, &sector, &track, &disk);
	// USLOSS_Console("%d %d %d %d\n", diskStatus, sector, track, disk);

	int pageSize = USLOSS_MmuPageSize();
	// USLOSS_Console("%d\n", pageSize);
	
	vmStats.pages = pages;
	vmStats.frames = frames;
	vmStats.diskBlocks = (sector*track*disk)/pageSize; // TODO: Disk stuff
	vmStats.freeFrames = frames;
	vmStats.freeDiskBlocks = vmStats.diskBlocks;
	vmStats.switches = 0;
	vmStats.faults = 0;
	vmStats.new = 1; //FIXME: should be 0
	vmStats.pageIns = 0;
	vmStats.pageOuts = 0;
	vmStats.replaced = 0;

	return USLOSS_MmuRegion(&dummy);
} /* vmInitReal */


/*
 *----------------------------------------------------------------------
 *
 * PrintStats --
 *
 *      Print out VM statistics.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Stuff is printed to the USLOSS_Console.
 *
 *----------------------------------------------------------------------
 */
void PrintStats(void)
{
	USLOSS_Console("VmStats\n");
	USLOSS_Console("pages:          %d\n", vmStats.pages);
	USLOSS_Console("frames:         %d\n", vmStats.frames);
	USLOSS_Console("diskBlocks:     %d\n", vmStats.diskBlocks);
	USLOSS_Console("freeFrames:     %d\n", vmStats.freeFrames);
	USLOSS_Console("freeDiskBlocks: %d\n", vmStats.freeDiskBlocks);
	USLOSS_Console("switches:       %d\n", vmStats.switches);
	USLOSS_Console("faults:         %d\n", vmStats.faults);
	USLOSS_Console("new:            %d\n", vmStats.new);
	USLOSS_Console("pageIns:        %d\n", vmStats.pageIns);
	USLOSS_Console("pageOuts:       %d\n", vmStats.pageOuts);
	USLOSS_Console("replaced:       %d\n", vmStats.replaced);
} /* PrintStats */


/*
 *----------------------------------------------------------------------
 *
 * vmDestroyReal --
 *
 * Called by vmDestroy.
 * Frees all of the global data structures
 *
 * Results:
 *      None
 *
 * Side effects:
 *      The MMU is turned off.
 *
 *----------------------------------------------------------------------
 */
void vmDestroyReal(void)
{
	if (debugFlag5){
		USLOSS_Console("vmDestroyReal(): called\n");
	}

	CheckMode();
	int mmu = USLOSS_MmuDone();
	if (mmu != USLOSS_MMU_OK){
		USLOSS_Console("vmDestroyReal(): USLOSS MMU error. Terminating...\n");
		Terminate(1);
	}

	/*
	* Kill the pagers here.
	*/
	destroy = 1;
	for (int i = 0; i < numPagers; ++i) {
		MboxSend(faultMbox, NULL, 0);
		zap(pagerPids[i]);
	}

	/* 
	* Print vm statistics.
	*/
	PrintStats();

	initialized = 0;
} /* vmDestroyReal */

/*
 *----------------------------------------------------------------------
 *
 * FaultHandler
 *
 * Handles an MMU interrupt. Simply stores information about the
 * fault in a queue, wakes a waiting pager, and blocks until
 * the fault has been handled.
 *
 * Results:
 * None.
 *
 * Side effects:
 * The current process is blocked until the fault is handled.
 *
 *----------------------------------------------------------------------
 */
static void FaultHandler(int type /* MMU_INT */,
						 void * offset  /* Offset within VM region */)  //FIXME: not sure if void* or int?
{
	if (debugFlag5) {
		USLOSS_Console("FaultHandler%d(): called by pid %d,", getpid(), getpid());
	}

	int cause;

	assert(type == USLOSS_MMU_INT);
	cause = USLOSS_MmuGetCause();
	assert(cause == USLOSS_MMU_FAULT);
	vmStats.faults++;

	if (debugFlag5) {
		USLOSS_Console("faults now = %d\n", vmStats.faults);
	}

	
	/*
	* Fill in faults[pid % MAXPROC], send it to the pagers, and wait for the
	* reply.
	*/
	int pid = getpid() % MAXPROC;
	faults[pid].pid = getpid();
	faults[pid].addr = offset;
	faults[pid].replyMbox = procTable[pid].faultMbox;

	// Add the fault to the queue
	if (faultQueue == NULL) {
		faultQueue = &faults[pid];
		faultQueue->next = NULL;
	}
	else {
		FaultMsg* prev = NULL;
		FaultMsg* curr = faultQueue;

		while (curr != NULL) {
			prev = curr;
			curr = curr->next;
		}
		prev->next = &faults[pid];
		prev->next->next = NULL;
	}

	// Wake up a waiting pager
	if (debugFlag5) {
		USLOSS_Console("FaultHandler%d(): Waking up a pager\n", getpid());
	}
	MboxSend(faultMbox, NULL, 0); //should not be a blocking send!

	// Block until the fault has been handled.
	if (debugFlag5) {
		USLOSS_Console("FaultHandler%d(): blocking to wait for a pager to process the fault (mbox %d)\n", getpid(), procTable[pid].faultMbox);
	}

	//which frame did I get?
	int myframe;
	MboxReceive(procTable[pid].faultMbox, &myframe, sizeof(int));

	if (debugFlag5) {
		USLOSS_Console("FaultHandler%d(): woke up after fault, given frame #%d by pager\n", getpid(), myframe);
	}

	//update MY pagetable
	int pageNumber = ((long)offset) / USLOSS_MmuPageSize();
	Process* me = getProc(getpid());
	me->pageTable[pageNumber].frame = myframe;
	me->pageTable[pageNumber].state = OCCUPIED; //FIXME: not sure


	//call map
	int tag;
	int mmuStatus = USLOSS_MmuGetTag(&tag);
	if (mmuStatus != USLOSS_MMU_OK){
		if (debugFlag5){
			USLOSS_Console("Pager(): mmu gettag failed\n");
		}
		Terminate(1);
	}

	mmuStatus = USLOSS_MmuMap(tag, pageNumber, myframe, USLOSS_MMU_PROT_RW );
	if (mmuStatus != USLOSS_MMU_OK){
		if (debugFlag5){
			USLOSS_Console("Pager(): mmu map failed\n");
		}
		Terminate(1);
	}

	vmStats.freeFrames--;


	//unlock the frame (set to INCORE or something)
	return;

} /* FaultHandler */


/*
 *----------------------------------------------------------------------
 *
 * Pager 
 *
 * Kernel process that handles page faults and does page replacement.
 *
 * Results:
 * None.
 *
 * Side effects:
 * None.
 *
 *----------------------------------------------------------------------
 */
static int Pager(char *buf)
{
	while(1) {
		/* Wait for fault to occur (receive from mailbox) */
		MboxReceive(faultMbox, NULL, 0);
		

		/* Pop message from faultqueue */
		FaultMsg* fault = faultQueue;
		if (fault == NULL){
			quit(1);
		}
		faultQueue = faultQueue->next;
		fault->next = NULL;

		if (debugFlag5){
			USLOSS_Console("Pager(): woke up with fault from pid %d\n", fault->pid);
		}

		/* find the frame that were gonna use */
		int frameIndex = scanForFrame();

		/* If there isn't one then use clock algorithm to
		   replace a page (perhaps write to disk) */
		if (frameIndex == -1){
			if (debugFlag5){
				USLOSS_Console("Pager(): no frames available, have to do page replacement\n");
				/* clock algorithm */	
			}	
			frameIndex = clockSweep();

			if (!frameTable[frameIndex].clean){
				USLOSS_Console("Pager(): the frame we found is dirty, writing to disk\n");
				//TODO: write page in frame to disk
			}
		} else {
			if (debugFlag5){
				USLOSS_Console("Pager(): available frame at index %d\n", frameIndex);
			}
		}


		/* Load page into frame from disk, if necessary */

		int tag;
		int mmuStatus = USLOSS_MmuGetTag(&tag);
		if (mmuStatus != USLOSS_MMU_OK){
			if (debugFlag5){
				USLOSS_Console("Pager(): mmu gettag failed\n");
			}
			Terminate(1);
		}

		int pageNumber = ((long)fault->addr) / USLOSS_MmuPageSize();

		/* say the the pager "has" this frame, (change status in frame table) */
		frameTable[frameIndex].state = OCCUPIED;
		frameTable[frameIndex].pid = fault->pid;
		frameTable[frameIndex].page = pageNumber;
		frameTable[frameIndex].referenced = 1;


		/* do the mapping and copy info */
		mmuStatus = USLOSS_MmuMap(tag, pageNumber, frameIndex, USLOSS_MMU_PROT_RW );
		if (mmuStatus != USLOSS_MMU_OK){
			if (debugFlag5){
				USLOSS_Console("Pager(): mmu map failed\n");
			}
			Terminate(1);
		}

		// memset or TODO: get info from disk
		memset( (char *)((long)vmRegion + (long)fault->addr), 0, USLOSS_MmuPageSize());
		//you cannot write to a frame if the frame is not mapped to a page


		//TODO: unmap
		mmuStatus = USLOSS_MmuUnmap(tag, pageNumber);
		if (mmuStatus != USLOSS_MMU_OK){
			if (debugFlag5){
				USLOSS_Console("Pager(): mmu map failed\n");
			}
			Terminate(1);
		}


		/* Unblock waiting (faulting) process */
		if (debugFlag5){
			USLOSS_Console("Pager(): waking up pid %d (mbox %d)\n", fault->pid, fault->replyMbox);
		}

		/* send "you get frame x" to waiting faulthandler, 
			then faulthandler "unlocks" it eventually */
		MboxSend(fault->replyMbox, &frameIndex, sizeof(int));
		if (debugFlag5){
			USLOSS_Console("Pager(): done with iteration\n");
		}

	}
	return 0;
} /* Pager */

int clockSweep(){
	//iterate from 0 or starting index (from last clock run) through all frames
		//if frame x referenced -> set to 0
		//else frameIndex = x
	//next clock starting point = x+1 % numFrames

	//unreferenced and clean is our favorite page to replace
		//hasn't been referenced since the last clock sweep
		//hasn't been changed since it was last written to the disk

	//first check referenced
	//if everyone's been referenced then loop again check clean vs. dirty


	//loop over each frame, set referenced to false, find first unreferenced
	int i = startingFrame; 
	for (int x = 0; x < numFrames; x++){ //loop through all the frames
		if (frameTable[i].referenced){
			frameTable[i].referenced = 0;
		} else {
			startingFrame = (i+1)%numFrames;
			if (debugFlag5){
				USLOSS_Console("Pager(): found unreferenced frame #%d\n", i);
			}
			return i;
		}
		i = (i+1)%numFrames;
	}

	//no unreferenced frames, find clean frame
	if (debugFlag5){
		USLOSS_Console("Pager(): all frames are referenced, have to check clean/dirty\n");
	}

	for (int j = 0; j < numFrames; j++){
		if (frameTable[j].clean){
			if (debugFlag5){
				USLOSS_Console("Pager(): found referenced but clean frame #%d\n", i);
			}
			startingFrame = (j+1)%numFrames;
			return j;
		}
	}
	return 0;
}

int scanForFrame(){
/* Look for free frame */
	for (int i = 0; i < numFrames; i++){
		if (frameTable[i].state == UNUSED){
			return i;
		}
	}
	return -1;
}


/*
Enters user mode by calling psrget()
*/
int enterUserMode() {
    unsigned int psr = USLOSS_PsrGet();
    unsigned int op = 0xfffffffe;
    int result = USLOSS_PsrSet(psr & op);
    if (result == USLOSS_ERR_INVALID_PSR) {
        return -1;
    }
    else {
        return 0;
    }
}

void initProcTable() {
    for (int i = 0; i < MAXPROC; ++i) {
        procTable[i].pid = -1;
        procTable[i].status = UNUSED;
        procTable[i].pageTable = NULL;
        procTable[i].numPages = -1;
        //procTable[i].semId = semcreateReal(0);
       	//Mbox_Create(1, 0, &procTable[i].faultMbox);
       	procTable[i].faultMbox = MboxCreate(1,sizeof(int));
    }
}

void initFrameTable(int frames){
	numFrames = frames;
	//malloc frametable
	frameTable =(FTE*) malloc(frames*sizeof(FTE));
	for (int i = 0; i < frames; i++){
		frameTable[i].pid = -1;
		frameTable[i].page = -1;
		frameTable[i].state = UNUSED;
		frameTable[i].clean = 1;
		frameTable[i].referenced = 0;
	}
}

Process* getProc(int pid){
	return &procTable[pid % MAXPROC];
}
