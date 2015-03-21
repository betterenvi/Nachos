#include"tid.h"

TidManager::TidManager(){
    for (int i = 0; i < MAX_NUM_THREAD; ++i){
        idPool.insert(i);
    }
}

//if return value is -1, it means all ids are being used.

int TidManager::genId(){
    if (idPool.size() <= 0)
        return -1;
    int retId = *(idPool.begin());
    idPool.erase(retId);
    return retId;
}
int TidManager::putBack(int tid){
    if (tid < 0 || tid >= MAX_NUM_THREAD)
        return -1;
    idPool.insert(tid);
    return 0;
}
void TidManager::addThread(Thread * t){
    allThreads.insert(pair<int, Thread*>(t->getTid(), t));
}
void TidManager::ts(){
    map<int, Thread*>::iterator itr;
    printf("Tid\tUid\tPri\tName\tStatus\n");
    for (itr = allThreads.begin(); itr != allThreads.end(); ++itr){
        Thread *t = itr->second;
        printf("%d\t%d\t%d\t%s\t%s\n", t->getTid(), t->getUid(), t->getPriority(), t->getName(), t->getStatus());
        
    }
}
