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

static int headerSectorToFind;
static FileACEntry * foundACEntry;
static void FindACEntry(int acEntry){
	FileACEntry * entry = (FileACEntry *)acEntry;
    if (entry->headerSector == headerSectorToFind)
        foundACEntry = entry;
};

FileACList::FileACList(){
	list = new SynchList;
	superLock = new Lock("FileACList");
	foundACEntry = NULL;
}
FileACList::~FileACList(){
	delete list;
	delete superLock;
}

void *FileACList::getACEntry(int headerSector){
    headerSectorToFind = headerSector;
    foundACEntry = NULL;
    list->Mapcar(FindACEntry);
    return foundACEntry;
};
// invoked by OpenFile::OpenFile()
void FileACList::UpdateFileACListWhenOpenFile(int headerSector){
    superLock->Acquire();
    FileACEntry * entry = (FileACEntry *)getACEntry(headerSector);
    if (entry == NULL){// need to synchronize
        entry = new FileACEntry(headerSector);
        list->Append((void *) entry);
    }
    entry->numLock->Acquire();
    entry->numThreads += 1;
    entry->numLock->Release();
    superLock->Release();
};
// invoked by OpenFile::~OpenFile()
void FileACList::UpdateFileACListWhenCloseFile(int headerSector){
    DEBUG('f', "Enter UpdateFileACListWhenCloseFile\n");
    superLock->Acquire();
    FileACEntry * entry = (FileACEntry *)getACEntry(headerSector);
    ASSERT(entry != NULL);
    entry->numLock->Acquire();
    entry->numThreads -= 1;
    ASSERT(entry->numThreads >= 0);
    entry->numLock->Release();
    superLock->Release();
    DEBUG('f', "Leave UpdateFileACListWhenCloseFile\n");
};