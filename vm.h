/*
 * vm.h
 */


/*
 * All processes use the same tag.
 */
#define TAG 0

/*
 * Different states for a page.
 */
#define UNUSED 500
#define INCORE 501
#define OCCUPIED 502
/* You'll probably want more states */


/*
 * Page table entry.
 */
typedef struct PTE {
    int  state;      // See above.
    int  frame;      // Frame that stores the page (if any). -1 if none.
    int  diskBlock;  // Disk block that stores the page (if any). -1 if none.
    // Add more stuff here
} PTE;


/*
 * Frame table entry.
 */
typedef struct FTE {
    int  state;      // See above.
    int  pid;        // Process using the frame.
    int  page;       // Page stored in the frame.
    int  clean;      // Specifies if the frame is clean or dirty (matches or has disparity with disk).
    int referenced;
} FTE;

/*
 * Per-process information.
 */
typedef struct Process {
    int  numPages;   // Size of the page table.
    PTE  *pageTable; // The page table for the process.
    int  status;     // UNUSED or OCCUPIED
    int  pid;
    int  faultMbox;
    // Add more stuff here */
} Process;

typedef struct FaultMsg* faultPtr;
/*
 * Information about page faults. This message is sent by the faulting
 * process to the pager to request that the fault be handled.
 */
typedef struct FaultMsg {
    int  pid;        // Process with the problem.
    void *addr;      // Address that caused the fault.
    int  replyMbox;  // Mailbox to send reply.
    faultPtr next;
    // Add more stuff here.
} FaultMsg;

#define CheckMode() assert(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)
