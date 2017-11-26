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
#include <vm.h>
#include <string.h>

int debugFlag5 = 1;

extern void mbox_create(USLOSS_Sysargs *args_ptr);
extern void mbox_release(USLOSS_Sysargs *args_ptr);
extern void mbox_send(USLOSS_Sysargs *args_ptr);
extern void mbox_receive(USLOSS_Sysargs *args_ptr);
extern void mbox_condsend(USLOSS_Sysargs *args_ptr);
extern void mbox_condreceive(USLOSS_Sysargs *args_ptr);
extern int semcreateReal();
extern void sempReal();
extern void semvReal();

static void FaultHandler(int type, void * offset);
static void vmInit(USLOSS_Sysargs *USLOSS_SysargsPtr);
static void vmDestroy(USLOSS_Sysargs *USLOSS_SysargsPtr);
static int Pager(char *buf);

int enterUserMode();
void * vmInitReal(int mappings, int pages, int frames, int pagers);
void initProcTable();
void vmDestroyReal(void);

void * vmRegion; //FIXME: not sure what this is supposed to be
static Process procTable[MAXPROC];

FaultMsg faults[MAXPROC]; /* Note that a process can have only
													 * one fault at a time, so we can
													 * allocate the messages statically
													 * and index them by pid. */
VmStats  vmStats;
FTE* frameTable;
int faultMbox;
int * pagerPids;
int numPagers;
int statsMutex;
int initialized;



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

	initProcTable();
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
	frameTable = malloc(sizeof(FTE) * frames);
	for (int i = 0; i < frames; ++i) {
		frameTable[i].state = UNUSED;
		frameTable[i].clean = 1;
		frameTable[i].pid = -1;
		frameTable[i].page = -1;
	}

	/*
	* Initialize page tables.
	*/
	for (int i = 0; i < MAXPROC; ++i) {
		procTable[i].numPages = pages; // Set every procs' numPages but wait for them to be forked to actually initialize the page table.
	}

	/* 
	* Create the fault mailbox.
	*/
	faultMbox = MboxCreate(0, sizeof(FaultMsg));

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
	vmStats.pages = pages;
	vmStats.frames = frames;
	vmStats.diskBlocks = 0; // TODO: Disk stuff
	vmStats.freeFrames = frames;
	vmStats.freeDiskBlocks = 0;
	vmStats.switches = 0;
	vmStats.faults = 0;
	vmStats.new = 0;
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

	CheckMode();
	int mmu = USLOSS_MmuDone();
	if (mmu != USLOSS_MMU_OK){
		USLOSS_Console("vmDestroyReal(): USLOSS MMU error. Terminating...\n");
		Terminate(1);
	}

	/*
	* Kill the pagers here.
	*/
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
	int cause;

	assert(type == USLOSS_MMU_INT);
	cause = USLOSS_MmuGetCause();
	assert(cause == USLOSS_MMU_FAULT);
	vmStats.faults++;
	 /*
		* Fill in faults[pid % MAXPROC], send it to the pagers, and wait for the
		* reply.
		*/
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
				/* Look for free frame */
				/* If there isn't one then use clock algorithm to
				 * replace a page (perhaps write to disk) */
				/* Load page into frame from disk, if necessary */
				/* Unblock waiting (faulting) process */
		}
		return 0;
} /* Pager */


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
        //procTable[i].diskMboxId = MboxCreate(0, sizeof(int));
    }
}