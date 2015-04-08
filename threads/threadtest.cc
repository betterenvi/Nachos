// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create two threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustratethe inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"

// testnum is set in main.cc
int testnum = 1;

//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------

void
SimpleThread(int which)
{
    int num;
    
    for (num = 0; num < 3; num++) {
	printf("*** thread %d(tid %d, uid %d, pri %d) looped %d times\n", which, currentThread->getTid(), currentThread->getUid(), currentThread->getPriority(), num);
        currentThread->Yield();
    }
}

//----------------------------------------------------------------------
// ThreadTest1
// 	Set up a ping-pong between two threads, by forking a thread 
//	to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest1()
{
    DEBUG('t', "Entering ThreadTest1");

    //Thread *t = new Thread("forked thread");
    Thread *t = createThread("forked thread");
    if (t == NULL)
        return;
    t->Fork(SimpleThread, 1);
    SimpleThread(0);
}
//
void
SimpleThread2ForMaxTid(int which)
{
    int num;
    
    for (num = 0; num < 130; num++) {
	printf("*** thread %d(tid %d, uid %d) looped %d times\n", which, currentThread->getTid(), currentThread->getUid(), num);
        currentThread->Yield();
    }
}

//Test TidManager
void
ThreadTest2ForMaxTid(){
    for (int i = 0; i < 130; ++i){
        Thread *t = createThread("Tid");
        if (t == NULL)
            continue;
        t->Fork(SimpleThread2ForMaxTid, t->getTid());
    }
    //tidManager->ts();
//    t->Fork(SimpleThread, 1);
}
//

void
ThreadTest3ForTs()
{
    DEBUG('t', "Entering ThreadTest3ForTs");

    //Thread *t = new Thread("TS");
    for (int i = 0; i < 4; i++){
        Thread *t = createThread("TS");
        if (t == NULL)
            continue;
        t->Fork(SimpleThread, t->getTid());
    }
    tidManager->ts();
}
//----------------------------------------------------------------------
// ThreadTest
// 	Invoke a test routine.
//----------------------------------------------------------------------
void
ThreadTest23(){
    DEBUG('t', "Entering ThreadTest23");
    for (int i = 0; i < 3; ++i){
        Thread *t = createThread("T23", 4 - i % 5);
        if (t == NULL)
            continue;
        t->Fork(SimpleThread, t->getTid());
    }
    tidManager->ts();
}
void
SimpleThread231(int tid){
    for (int i = 0; i < 2; ++i){
        Thread *t = createThread("T231", 3 - i);
        if (t == NULL)
            return;
        printf("*** thread %d(tid %d, uid %d, pri %d)created thread %d(tid %d, uid %d, pri %d)\n", tid, tid, currentThread->getUid(), currentThread->getPriority(), t->getTid(), t->getTid(), t->getUid(), t->getPriority());
        t->Fork(SimpleThread, t->getTid());
    }
}
void 
ThreadTest231(){
    DEBUG('t', "Entering ThreadTest231");
    Thread* t = createThread("T231", 4);
    if (t == NULL)
        return;
    t->Fork(SimpleThread231, t->getTid());
    tidManager->ts();
}

void midrun(int which){
    for (int i = 0; i < 20; i++){
        printf("%s\n", currentThread->getName());
        interrupt->OneTick();
    }
}
void mid(){
    Thread *t1 = createThread("Bat", 4);
    Thread *t2 = createThread("sup", 4);
    t1->Fork(midrun, 1);
    t2->Fork(midrun, 2);
}

//in synchtest.cc
extern int synch_test_choice;
extern void producer_cosumer_test();
extern void barrier_test();
extern bool synch_test_yield;
extern bool synch_test_yield_writer;
extern void read_write_lock_test();
void
ThreadTest()
{
    switch (testnum) {
    case 1:
	    ThreadTest1();
        break;
    case 2:
        ThreadTest2ForMaxTid();
        break;
    case 3:
        ThreadTest3ForTs();
        break;
    case 23:
        ThreadTest23();
        break;
    case 231:
        ThreadTest231();
        break;
    case 31:
        synch_test_choice = 0;
        producer_cosumer_test();
        break;
    case 32:
        synch_test_choice = 1;
        producer_cosumer_test();
        break;
    case 33:
        synch_test_choice = 2;
        producer_cosumer_test();
        break;
    case 34:
        synch_test_choice = 3;
        producer_cosumer_test();
        break;
    case 35:
        barrier_test();
        break;
    case 36:
        synch_test_yield = false;
        read_write_lock_test();
        break;
    case 37:
        synch_test_yield = true;
        read_write_lock_test();
        break;
    case 38:
        synch_test_yield = false;
        synch_test_yield_writer = true;
        read_write_lock_test();
        break;
    case 39:
        mid();
        break;
    default:
	    printf("No test specified.\n");
        printf("CQY added a test.\n");
	    break;
    }
}

