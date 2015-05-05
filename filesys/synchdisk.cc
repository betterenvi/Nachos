// synchdisk.cc 
//	Routines to synchronously access the disk.  The physical disk 
//	is an asynchronous device (disk requests return immediately, and
//	an interrupt happens later on).  This is a layer on top of
//	the disk providing a synchronous interface (requests wait until
//	the request completes).
//
//	Use a semaphore to synchronize the interrupt handlers with the
//	pending requests.  And, because the physical disk can only
//	handle one operation at a time, use a lock to enforce mutual
//	exclusion.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synchdisk.h"
#include "system.h"
//----------------------------------------------------------------------
// DiskRequestDone
// 	Disk interrupt handler.  Need this to be a C routine, because 
//	C++ can't handle pointers to member functions.
//----------------------------------------------------------------------

static void
DiskRequestDone (int arg)
{
    SynchDisk* disk = (SynchDisk *)arg;

    disk->RequestDone();
}

//----------------------------------------------------------------------
// SynchDisk::SynchDisk
// 	Initialize the synchronous interface to the physical disk, in turn
//	initializing the physical disk.
//
//	"name" -- UNIX file name to be used as storage for the disk data
//	   (usually, "DISK")
//----------------------------------------------------------------------

SynchDisk::SynchDisk(char* name)
{
    semaphore = new Semaphore("synch disk", 0);
    lock = new Lock("synch disk lock");
    disk = new Disk(name, DiskRequestDone, (int) this);
}

//----------------------------------------------------------------------
// SynchDisk::~SynchDisk
// 	De-allocate data structures needed for the synchronous disk
//	abstraction.
//----------------------------------------------------------------------

SynchDisk::~SynchDisk()
{
    delete disk;
    delete lock;
    delete semaphore;
}

//----------------------------------------------------------------------
// SynchDisk::ReadSector
// 	Read the contents of a disk sector into a buffer.  Return only
//	after the data has been read.
//
//	"sectorNumber" -- the disk sector to read
//	"data" -- the buffer to hold the contents of the disk sector
//----------------------------------------------------------------------

void
SynchDisk::ReadSector(int sectorNumber, char* data)
{
    lock->Acquire();			// only one disk I/O at a time
    disk->ReadRequest(sectorNumber, data);
    semaphore->P();			// wait for interrupt
    lock->Release();
}

//----------------------------------------------------------------------
// SynchDisk::WriteSector
// 	Write the contents of a buffer into a disk sector.  Return only
//	after the data has been written.
//
//	"sectorNumber" -- the disk sector to be written
//	"data" -- the new contents of the disk sector
//----------------------------------------------------------------------

void
SynchDisk::WriteSector(int sectorNumber, char* data)
{
    lock->Acquire();			// only one disk I/O at a time
    disk->WriteRequest(sectorNumber, data);
    semaphore->P();			// wait for interrupt
    lock->Release();
}

//----------------------------------------------------------------------
// SynchDisk::RequestDone
// 	Disk interrupt handler.  Wake up any thread waiting for the disk
//	request to finish.
//----------------------------------------------------------------------

void
SynchDisk::RequestDone()
{ 
    semaphore->V();
}

DiskCacheEntry::DiskCacheEntry(){
    data = new char[SectorSize];
    timeStamp = 0;
    inUse = FALSE;
    dirty = FALSE;
}
DiskCacheEntry::~DiskCacheEntry(){
    delete data;

}

CacheSynchDisk::CacheSynchDisk(SynchDisk * synDisk_):SynchDisk("CacheSynchDisk"){
    readWriteLock = new ReadWriteLock("CacheSynchDisk");
    synDisk = synDisk_;
}
CacheSynchDisk::~CacheSynchDisk(){
    delete readWriteLock;
}
//private 
int CacheSynchDisk::GetDstIndex(){
    for (int i = 0; i < DISK_CACHE_SIZE; ++i){
        if (!cache[i].inUse)
            return i;
    }
    int dstIndex = GetDstIndexByLRU();
    if (cache[dstIndex].dirty)
        WriteBack(dstIndex);
    return dstIndex;
}
int CacheSynchDisk::GetDstIndexByLRU(){
    int dstTime = cache[0].timeStamp;
    int dstIndex = 0;
    for (int i = 1; i < DISK_CACHE_SIZE; ++i){
        if (cache[i].timeStamp < dstTime){
            dstTime = cache[i].timeStamp;
            dstIndex = i;
        }
    }
    return dstIndex;
}
//private 
void CacheSynchDisk::WriteBack(int index){
    synDisk->WriteSector(cache[index].sectorNumber, cache[index].data);
    cache[index].inUse = FALSE;
}
//public
void CacheSynchDisk::ReadSector(int sectorNumber, char * data){
    readWriteLock->BeforeRead();            // a bit problematic, 
                    //because updating timestamp is some kind of writing
    printf("********************^^^^^^^^^^\n");
    DEBUG('f', "Thread %d enter CacheSynchDisk::ReadSector.\n", currentThread->getTid());
    for (int i = 0; i < DISK_CACHE_SIZE; ++i){
        if (cache[i].inUse && cache[i].sectorNumber == sectorNumber){
            bcopy(cache[i].data, data, SectorSize);//.  read entry
            cache[i].timeStamp = stats->totalTicks;//   write entry
            readWriteLock->AfterRead();
            return;
        }
    }
    readWriteLock->AfterRead();
    readWriteLock->BeforeWrite();
    int dstIndex = GetDstIndex();                   //write cache
    synDisk->ReadSector(cache[dstIndex].sectorNumber, cache[dstIndex].data);//write
    bcopy(cache[dstIndex].data, data, SectorSize);  //read 
    cache[dstIndex].dirty = FALSE;                  //write cache
    cache[dstIndex].inUse = TRUE;                   //write cache
    cache[dstIndex].timeStamp = stats->totalTicks;  //write
    readWriteLock->AfterWrite();
}

//public
void CacheSynchDisk::WriteSector(int sectorNumber, char * data){
    readWriteLock->BeforeWrite();
    for (int i = 0; i < DISK_CACHE_SIZE; ++i){
        if (cache[i].inUse && cache[i].sectorNumber == sectorNumber){
            bcopy(data, cache[i].data, SectorSize); //write
            cache[i].dirty = TRUE;                  //write
            cache[i].timeStamp = stats->totalTicks; //write
            readWriteLock->AfterWrite();
            return;
        }
    }
    int dstIndex = GetDstIndex();                   //write
    //synDisk->ReadSector(cache[dstIndex].sectorNumber, cache[dstIndex].data);
    bcopy(data, cache[dstIndex].data, SectorSize);  //write
    cache[dstIndex].dirty = TRUE;                   //write
    cache[dstIndex].inUse = TRUE;                   //write
    cache[dstIndex].timeStamp = stats->totalTicks;  //write
    readWriteLock->AfterWrite();
}
SynchDisk * CacheSynchDisk::GetUncachedSynchDisk(){
    return synDisk;
}
