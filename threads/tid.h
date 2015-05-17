/*************************************************************************
	> File Name: tid.h
	> Author: CQY
	> Mail: 
	> Created TimZ*********************************************************/

#ifndef _TID_H
#define _TID_H

#include<set>
#include<map>
#include "thread.h"
#include "synch.h"
#include "list.h"
#define MAX_NUM_THREAD 128
using namespace std;
class TidManager{
    private:
        Lock *ctrlLock;
        set<int> idPool;
        map<int, Thread*> allThreads;
        map<int, List&> joinThreadTable;
    public:
        TidManager();
        ~TidManager();
        int genId();
        int putBack(int tid);
        void addThread(Thread * t);
        void ts();
        bool addToJoinTable(int tid);
        void awakeJoinThreads(int tid);
        
};
#endif