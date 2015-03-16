/*************************************************************************
	> File Name: tid.h
	> Author: CQY
	> Mail: 
	> Created TimZ*********************************************************/

#ifndef _TID_H
#define _TID_H
#endif

#include<set>
#include<map>
#include "thread.h"

#define MAX_NUM_THREAD 128
using namespace std;
class TidManager{
    private:
        set<int> idPool;
        map<int, Thread*> allThreads;
    public:
        TidManager();
        int genId();
        int putBack(int tid);
        void addThread(Thread * t);
        void ts();
        
};
