#include "system.h"
#include "tid.h"

TidManager::TidManager(){
    for (int i = 0; i < MAX_NUM_THREAD; ++i){
        idPool.insert(i);
    }
    ctrlLock = new Lock("TidManagerLock");
}
TidManager::~TidManager(){
    delete ctrlLock;
}
//if return value is -1, it means all ids are being used.

int TidManager::genId(){
    ctrlLock->Acquire();
    if (idPool.size() <= 0)
        return -1;
    int retId = *(idPool.begin());
    idPool.erase(retId);
    ctrlLock->Release();
    return retId;
}
int TidManager::putBack(int tid){
    if (tid < 0 || tid >= MAX_NUM_THREAD)
        return -1;
    ctrlLock->Acquire();
    allThreads.erase(tid);
    idPool.insert(tid);
    ctrlLock->Release();
    return 0;
}
void TidManager::addThread(Thread * t){
    ctrlLock->Acquire();
    allThreads.insert(pair<int, Thread*>(t->getTid(), t));
    ctrlLock->Release();
}
void TidManager::ts(){
    ctrlLock->Acquire();
    map<int, Thread*>::iterator itr;
    printf("Tid\tUid\tPri\tName\tStatus\n");
    for (itr = allThreads.begin(); itr != allThreads.end(); ++itr){
        Thread *t = itr->second;
        printf("%d\t%d\t%d\t%s\t%s\n", t->getTid(), t->getUid(), t->getPriority(), t->getName(), t->getStatusName());
        
    }
    ctrlLock->Release();
}
bool TidManager::addToJoinTable(int tid){
    ctrlLock->Acquire();
    map<int, Thread*>::iterator threadIter;
    threadIter = allThreads.find(tid);
    if (threadIter == allThreads.end()){
        ctrlLock->Release();
        return FALSE;
    }
    map<int, List&>::iterator iter;
    iter = joinThreadTable.find(tid);
    if (iter == joinThreadTable.end()){
        List lst;
        lst.Append((void *)currentThread);
        joinThreadTable.insert(pair<int, List&>(tid, lst));
    } else {
        List lst = iter->second;
        lst.Append((void *)currentThread);
    }
    ctrlLock->Release();
    return TRUE;
}
void TidManager::awakeJoinThreads(int tid){
    ctrlLock->Acquire();
    map<int, List&>::iterator iter;
    iter = joinThreadTable.find(tid);
    if (iter != joinThreadTable.end()){
        List lst = iter->second;
        Thread * thread;
        while ((thread = (Thread *)lst.Remove()) != NULL){
            scheduler->ReadyToRun(thread);
        }
        joinThreadTable.erase(tid);
    }
    ctrlLock->Release();
}