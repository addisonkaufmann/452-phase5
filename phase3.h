/*
 * These are the definitions for phase 3 of the project
 */

#ifndef _PHASE3_H
#define _PHASE3_H

#define MAXSEMS         200

extern int  Spawn(char *name, int (*func)(char *), char *arg, long stack_size,
                  long priority, int *pid);
extern int  Wait(int *pid, int *status);
extern void Terminate(long status);
extern void GetTimeofDay(int *tod);
extern void CPUTime(int *cpu);
extern void GetPID(int *pid);
extern int  SemCreate(long value, int *semaphore);
extern int  SemP(long semaphore);
extern int  SemV(long semaphore);
extern int  SemFree(long semaphore);

#endif /* _PHASE3_H */

