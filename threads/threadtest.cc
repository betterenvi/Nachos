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
	printf("*** thread %d(tid %d, uid %d) looped %d times\n", which, currentThread->getTid(), currentThread->getUid(), num);
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

//Test TidManager
void
ThreadTest2ForTid(){
    for (int i = 0; i < 3; ++i){
        Thread *t = createThread("Tid");
        if (t == NULL)
            return;
        tidManager->addThread(t);
        t->Fork(SimpleThread, 0);
    }
    tidManager->ts();
//    t->Fork(SimpleThread, 1);
}
//----------------------------------------------------------------------
// ThreadTest
// 	Invoke a test routine.
//----------------------------------------------------------------------

void
ThreadTest()
{
    switch (testnum) {
    case 1:
//	ThreadTest1();
    ThreadTest2ForTid();
	break;
    default:
	printf("No test specified.\n");
    printf("CQY added a test.\n");
	break;
    }
}

