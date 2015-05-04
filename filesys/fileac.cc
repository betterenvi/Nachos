#include "fileac.h"

FileACEntry::FileACEntry(int headerSector_){
    readWriteLock = new ReadWriteLock("File AC entry");
    numLock = new Lock("FileACEntry numLock");
    headerSector = headerSector_;
    numThreads = 0;
}
FileACEntry::~FileACEntry(){
    delete readWriteLock;
    delete numLock;
}