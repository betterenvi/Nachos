// synch.cc 
//	Routines for synchronizing threads.  Three kinds of
//	synchronization routines are defined here: semaphores, locks 
//   	and condition variables (the implementation of the last two
//	are left to the reader).
//
// Any implementation of a synchronization routine needs some
// primitive atomic operation.  We assume Nachos is running on
// a uniprocessor, and thus atomicity can be provided by
// turning off interrupts.  While interrupts are disabled, no
// context switch can occur, and thus the current thread is guaranteed
// to hold the CPU throughout, until interrupts are reenabled.
//
// Because some of these routines might be called with interrupts
// already disabled (Semaphore::V for one), instead of turning
// on interrupts at the end of the atomic operation, we always simply
// re-set the interrupt state back to its original value (whether
// that be disabled or enabled).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synch.h"
#include "system.h"

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	Initialize a semaphore, so that it can be used for synchronization.
//
//	"debugName" is an arbitrary name, useful for debugging.
//	"initialValue" is the initial value of the semaphore.
//----------------------------------------------------------------------

Semaphore::Semaphore(char* debugName, int initialValue)
{
    name = debugName;
    value = initialValue;
    queue = new List;
}

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	De-allocate semaphore, when no longer needed.  Assume no one
//	is still waiting on the semaphore!
//----------------------------------------------------------------------

Semaphore::~Semaphore()
{
    delete queue;
}

//----------------------------------------------------------------------
// Semaphore::P
// 	Wait until semaphore value > 0, then decrement.  Checking the
//	value and decrementing must be done atomically, so we
//	need to disable interrupts before checking the value.
//
//	Note that Thread::Sleep assumes that interrupts are disabled
//	when it is called.
//----------------------------------------------------------------------

void
Semaphore::P()
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts
    
    while (value == 0) { 			// semaphore not available
	queue->Append((void *)currentThread);	// so go to sleep
    DEBUG('d', "Thread %d goes to sleep because of P()\n", currentThread->getTid());
	currentThread->Sleep();
    } 
    value--; 					// semaphore available, 
						// consume its value
    
    (void) interrupt->SetLevel(oldLevel);	// re-enable interrupts
}

//----------------------------------------------------------------------
// Semaphore::V
// 	Increment semaphore value, waking up a waiter if necessary.
//	As with P(), this operation must be atomic, so we need to disable
//	interrupts.  Scheduler::ReadyToRun() assumes that threads
//	are disabled when it is called.
//----------------------------------------------------------------------

void
Semaphore::V()
{
    Thread *thread;
    IntStatus oldLevel = interrupt->SetLevel(IntOff);

    thread = (Thread *)queue->Remove();
    if (thread != NULL){	   // make thread ready, consuming the V immediately
        if (thread->getStatus() == BLOCKED)
	       scheduler->ReadyToRun(thread);
        else //SUSPEND_BLK
            thread->setStatus(SUSPENDED_RDY);
    }
    value++;
    (void) interrupt->SetLevel(oldLevel);
}

// Dummy functions -- so we can compile our later assignments 
// Note -- without a correct implementation of Condition::Wait(), 
// the test case in the network assignment won't work!
Lock::Lock(char* debugName) {
    name = debugName;
    semaph = new Semaphore(name, 1);
    lockHolder = NULL;
}
Lock::~Lock() {
    delete semaph;
}
void Lock::Acquire() {
    semaph->P();
    lockHolder = currentThread;
}
void Lock::Release() {
//    A lock Acquired by a thread does not need to be Released by the same thread.
//    ASSERT(isHeldByCurrentThread());
    lockHolder = NULL;
    semaph->V();
}
bool Lock::isHeldByCurrentThread(){
    return (lockHolder == currentThread);
}

Condition::Condition(char* debugName) {
    name = debugName;
    queue = new List;
}

Condition::~Condition() { 
    delete queue;
}
void Condition::Wait(Lock* conditionLock) { 
    //no need to disable interrupt? I think yes if we Append currentThread to queue before Releasing the conditionLock.
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    ASSERT(conditionLock->isHeldByCurrentThread());
    conditionLock->Release();
    queue->Append((void *)currentThread);
    DEBUG('d', "Condition.Wait: Thread %d sleep.\n", currentThread->getTid());
    currentThread->Sleep();
    conditionLock->Acquire();
    (void) interrupt->SetLevel(oldLevel);
}
void Condition::Signal(Lock* conditionLock) {
    //no need to disable interrupt? I think yes.
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    ASSERT(conditionLock->isHeldByCurrentThread());
    Thread* t = (Thread *)queue->Remove();
    if (t != NULL){
        scheduler->ReadyToRun(t);
    }
    interrupt->SetLevel(oldLevel);
}
void Condition::Broadcast(Lock* conditionLock) { 
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    ASSERT(conditionLock->isHeldByCurrentThread());
    Thread* t = NULL;
    while (!(queue->IsEmpty())){
        t = (Thread *)queue->Remove();
        scheduler->ReadyToRun(t);
    }
    interrupt->SetLevel(oldLevel);
}


ReadWriteLock::ReadWriteLock(char *lockName){
    readerCnt = 0;
    name = lockName;
    cntLock = new Lock("cnt lock");
    writeLock = new Lock("write lock");
    superLock = new Lock("super Lock");
    curWritingThread = NULL;
}

ReadWriteLock::~ReadWriteLock(){
    delete cntLock;
    delete writeLock;
    delete superLock;
}

void ReadWriteLock::BeforeRead(){
    cntLock->Acquire();
    if (readerCnt <= 0)
        writeLock->Acquire();
    readerCnt += 1;
    cntLock->Release();
}
void ReadWriteLock::AfterRead(){
    cntLock->Acquire();
    readerCnt -= 1;
    if (readerCnt <= 0)
        writeLock->Release();
    cntLock->Release();
}
void ReadWriteLock::BeforeWrite(){
    writeLock->Acquire();
}
void ReadWriteLock::AfterWrite(){
    writeLock->Release();
}
void ReadWriteLock::AcquireSuperLock(){
    superLock->Acquire();
}
void ReadWriteLock::ReleaseSuperLock(){
    superLock->Release();
}