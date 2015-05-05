// filesys.cc 
//	Routines to manage the overall operation of the file system.
//	Implements routines to map from textual file names to files.
//
//	Each file in the file system has:
//	   A file header, stored in a sector on disk 
//		(the size of the file header data structure is arranged
//		to be precisely the size of 1 disk sector)
//	   A number of data blocks
//	   An entry in the file system directory
//
// 	The file system consists of several data structures:
//	   A bitmap of free disk sectors (cf. bitmap.h)
//	   A directory of file names and file headers
//
//      Both the bitmap and the directory are represented as normal
//	files.  Their file headers are located in specific sectors
//	(sector 0 and sector 1), so that the file system can find them 
//	on bootup.
//
//	The file system assumes that the bitmap and directory files are
//	kept "open" continuously while Nachos is running.
//
//	For those operations (such as Create, Remove) that modify the
//	directory and/or bitmap, if the operation succeeds, the changes
//	are written immediately back to disk (the two files are kept
//	open during all this time).  If the operation fails, and we have
//	modified part of the directory and/or bitmap, we simply discard
//	the changed version, without writing it back to disk.
//
// 	Our implementation at this point has the following restrictions:
//
//	   there is no synchronization for concurrent accesses
//	   files have a fixed size, set when the file is created
//	   files cannot be bigger than about 3KB in size
//	   there is no hierarchical directory structure, and only a limited
//	     number of files can be added to the system
//	   there is no attempt to make the system robust to failures
//	    (if Nachos exits in the middle of an operation that modifies
//	    the file system, it may corrupt the disk)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "disk.h"
#include "bitmap.h"
#include "directory.h"
#include "filehdr.h"
#include "filesys.h"
#include "system.h" //..
#include "fileac.h" //..

// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known 
// sectors, so that they can be located on boot-up.
#define FreeMapSector 		0
#define DirectorySector 	1

// Initial file sizes for the bitmap and directory; until the file system
// supports extensible files, the directory size sets the maximum number 
// of files that can be loaded onto the disk.
#define FreeMapFileSize 	(NumSectors / BitsInByte)
#define NumDirEntries 		10
#define DirectoryFileSize 	(sizeof(DirectoryEntry) * NumDirEntries)

static Lock sectorAllocateLock("super lock");
//static Lock dirAllocateLock;

//----------------------------------------------------------------------
// FileSystem::FileSystem
// 	Initialize the file system.  If format = TRUE, the disk has
//	nothing on it, and we need to initialize the disk to contain
//	an empty directory, and a bitmap of free sectors (with almost but
//	not all of the sectors marked as free).  
//
//	If format = FALSE, we just have to open the files
//	representing the bitmap and the directory.
//
//	"format" -- should we initialize the disk?
//----------------------------------------------------------------------

FileSystem::FileSystem(bool format)
{ 
    DEBUG('f', "Initializing the file system.\n");
    if (format) {
        BitMap *freeMap = new BitMap(NumSectors);
        Directory *directory = new Directory(NumDirEntries);
	FileHeader *mapHdr = new FileHeader;
	FileHeader *dirHdr = new FileHeader;

        DEBUG('f', "Formatting the file system.\n");

    // First, allocate space for FileHeaders for the directory and bitmap
    // (make sure no one else grabs these!)
	freeMap->Mark(FreeMapSector);	    
	freeMap->Mark(DirectorySector);

    // Second, allocate space for the data blocks containing the contents
    // of the directory and bitmap files.  There better be enough space!

	ASSERT(mapHdr->Allocate(freeMap, FreeMapFileSize));
	ASSERT(dirHdr->Allocate(freeMap, DirectoryFileSize));
    //.
    mapHdr->initialize(REGULAR_FILE, NO_PATH_SECTOR);
    dirHdr->initialize(DIRECTORY, NO_PATH_SECTOR);

    //testMaxFileSize(freeMap, directory);

    //..
    // Flush the bitmap and directory FileHeaders back to disk
    // We need to do this before we can "Open" the file, since open
    // reads the file header off of disk (and currently the disk has garbage
    // on it!).

        DEBUG('f', "Writing headers back to disk.\n");
	mapHdr->WriteBack(FreeMapSector);    
	dirHdr->WriteBack(DirectorySector);

    // OK to open the bitmap and directory files now
    // The file system operations assume these two files are left open
    // while Nachos is running.
        DEBUG('f', "Open freeMapFile directoryFile.\n");
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
     
    // Once we have the files "open", we can write the initial version
    // of each file back to disk.  The directory at this point is completely
    // empty; but the bitmap has been changed to reflect the fact that
    // sectors on the disk have been allocated for the file headers and
    // to hold the file data for the directory and bitmap.

        DEBUG('f', "Writing bitmap and directory back to disk.\n");
	freeMap->WriteBack(freeMapFile);	 // flush changes to disk
	directory->WriteBack(directoryFile);

	if (DebugIsEnabled('f')) {
	    freeMap->Print();
	    directory->Print(FALSE);

        delete freeMap; 
	delete directory; 
	delete mapHdr; 
	delete dirHdr;
	}
    } else {
    // if we are not formatting the disk, just open the files representing
    // the bitmap and directory; these are left open while Nachos is running
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
    }
    currentDirHeaderSector = DirectorySector;
}

FileSystem::~FileSystem(){
    delete freeMapFile;
    delete directoryFile;
}
//----------------------------------------------------------------------
// FileSystem::Create
// 	Create a file in the Nachos file system (similar to UNIX create).
//	Since we can't increase the size of files dynamically, we have
//	to give Create the initial size of the file.
//
//	The steps to create a file are:
//	  Make sure the file doesn't already exist
//        Allocate a sector for the file header
// 	  Allocate space on disk for the data blocks for the file
//	  Add the name to the directory
//	  Store the new file header on disk 
//	  Flush the changes to the bitmap and the directory back to disk
//
//	Return TRUE if everything goes ok, otherwise, return FALSE.
//
// 	Create fails if:
//   		file is already in directory
//	 	no free space for file header
//	 	no free entry for file in directory
//	 	no free space for data blocks for the file 
//
// 	Note that this implementation assumes there is no concurrent access
//	to the file system!
//
//	"name" -- name of file to be created
//	"initialSize" -- size of file to be created
//----------------------------------------------------------------------

bool
FileSystem::Create(char *name, int initialSize)
{//.
    DEBUG('f', "Enter FileSystem::Create.\n");
    bool success = CreateFileOrDir(name, initialSize, REGULAR_FILE);
    DEBUG('f', "Leave FileSystem::Create with success == %d.\n", success);
    return success;
/*    
    Directory *directory;
    BitMap *freeMap;
    FileHeader *hdr;
    int sector;
    bool success;

    DEBUG('f', "Creating file %s, size %d\n", name, initialSize);

    directory = new Directory(NumDirEntries);
    directory->FetchFrom(directoryFile);

    if (directory->Find(name) != -1)
        success = FALSE;			// file is already in directory
    else {	
        freeMap = new BitMap(NumSectors);
        freeMap->FetchFrom(freeMapFile);
        sector = freeMap->Find();	// find a sector to hold the file header
    	if (sector == -1) 		
            success = FALSE;		// no free block for file header 
        else if (!directory->Add(name, sector))
            success = FALSE;	// no space in directory
        else {
    	    hdr = new FileHeader;
            if (!hdr->Allocate(freeMap, initialSize)){
            	success = FALSE;	// no space on disk for data
                //.
                directory->Remove(name);
                //..
            }
    	    else {	
    	    	success = TRUE;
                // everthing worked, flush all changes back to disk
    	    	hdr->WriteBack(sector); 		
    	    	directory->WriteBack(directoryFile);
    	    	freeMap->WriteBack(freeMapFile);
            }
            delete hdr;
        }
        delete freeMap;
    }
    delete directory;
    return success;*///..
}

//----------------------------------------------------------------------
// FileSystem::Open
// 	Open a file for reading and writing.  
//	To open a file:
//	  Find the location of the file's header, using the directory 
//	  Bring the header into memory
//
//	"name" -- the text name of the file to be opened
//----------------------------------------------------------------------

OpenFile *
FileSystem::Open(char *name)
{ 
    DEBUG('t', "Enter FileSystem::Open.\n");
    sectorAllocateLock.Acquire();
    OpenFile *currentDirFile = new OpenFile(currentDirHeaderSector);
    Directory *directory = new Directory(NumDirEntries);
    OpenFile *openFile = NULL;
    int sector;

    DEBUG('f', "Opening file %s\n", name);
    
    directory->FetchFrom(currentDirFile);
    sector = directory->Find(name); 

    if (sector >= 0) {		
        openFile = new OpenFile(sector);	// name was found in directory 
    }
    sectorAllocateLock.Release();
    delete directory;
    delete currentDirFile;
    DEBUG('t', "Leave FileSystem::Open.\n");
    return openFile;				// return NULL if not found
}

//----------------------------------------------------------------------
// FileSystem::Remove
// 	Delete a file from the file system.  This requires:
//	    Remove it from the directory
//	    Delete the space for its header
//	    Delete the space for its data blocks
//	    Write changes to directory, bitmap back to disk
//
//	Return TRUE if the file was deleted, FALSE if the file wasn't
//	in the file system.
//
//	"name" -- the text name of the file to be removed
//----------------------------------------------------------------------

bool
FileSystem::Remove(char *name)
{ 
    DEBUG('t', "Enter FileSystem::Remove.\n");
    sectorAllocateLock.Acquire();
    OpenFile * currentDirFile = new OpenFile(currentDirHeaderSector);
    Directory *directory = new Directory(NumDirEntries);
    directory->FetchFrom(currentDirFile);
    BitMap *freeMap;
    FileHeader *fileHdr;
    int sector;
    
    sector = directory->Find(name);
    if (sector == -1) {
        printf("in thread %d, file '%s' not found.\n", currentThread->getTid(), name);
       delete directory;
       delete currentDirFile;
        sectorAllocateLock.Release();
       return FALSE;			 // file not found 
    }
    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);
    //. check if any thread is using this file
    FileACEntry * entry = (FileACEntry *) fileACList->getACEntry(sector);
    ASSERT(entry != NULL);
    entry->numLock->Acquire();
    if (entry->numThreads > 0){
        printf("in thread %d, file '%s' is in use.\n", currentThread->getTid(), name);
        entry->numLock->Release();
        sectorAllocateLock.Release();
        delete directory;
        delete currentDirFile;
        delete fileHdr;
        return FALSE;
    }
    freeMap = new BitMap(NumSectors);

    freeMap->FetchFrom(freeMapFile);

    fileHdr->Deallocate(freeMap);  		// remove data blocks
    freeMap->Clear(sector);			// remove header block
    directory->Remove(name);

    freeMap->WriteBack(freeMapFile);		// flush to disk

    directory->WriteBack(currentDirFile);        // flush to disk
    
    entry->numLock->Release();
    sectorAllocateLock.Release();
    delete fileHdr;
    delete directory;
    delete freeMap;
    delete currentDirFile;
    printf("File '%s' removed successfully by thread %d.\n", name, currentThread->getTid());
    DEBUG('t', "Leave FileSystem::Remove.\n");
    return TRUE;
} 

//----------------------------------------------------------------------
// FileSystem::List
// 	List all the files in the file system directory.
//----------------------------------------------------------------------

void
FileSystem::List()
{
    //.
    sectorAllocateLock.Acquire();
    OpenFile * currentDirFile = new OpenFile(currentDirHeaderSector);
    Directory *currentDir = new Directory(NumDirEntries);
    currentDir->FetchFrom(currentDirFile);
    currentDir->List();
    sectorAllocateLock.Release();
    delete currentDir;
    delete currentDirFile;
}

//----------------------------------------------------------------------
// FileSystem::Print
// 	Print everything about the file system:
//	  the contents of the bitmap
//	  the contents of the directory
//	  for each file in the directory,
//	      the contents of the file header
//	      the data in the file
//----------------------------------------------------------------------

void
FileSystem::Print(bool printContent)
{
    DEBUG('t', "Enter FileSystem::Print.\n");
    sectorAllocateLock.Acquire();
    OpenFile * currentDirFile = new OpenFile(currentDirHeaderSector);
    FileHeader *bitHdr = new FileHeader;
    FileHeader *dirHdr = new FileHeader;
    BitMap *freeMap = new BitMap(NumSectors);
    Directory *directory = new Directory(NumDirEntries);

    printf("Bit map file header:\n");
    bitHdr->FetchFrom(FreeMapSector);
    bitHdr->Print(printContent);

    printf("Directory file header:\n");
    dirHdr->FetchFrom(DirectorySector);
    dirHdr->Print(printContent);

    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();

    directory->FetchFrom(currentDirFile);
    directory->Print(printContent);
    sectorAllocateLock.Release();
    delete bitHdr;
    delete dirHdr;
    delete freeMap;
    delete directory;
    delete currentDirFile;
    DEBUG('t', "Leave FileSystem::Print.\n");
} 
void FileSystem::testMaxFileSize(void *freeMap_, void * directory_){
    sectorAllocateLock.Acquire();
    BitMap * freeMap = (BitMap *)freeMap_;
    Directory * directory = (Directory *)directory_;
    int headerSector = freeMap->Find();
    FileHeader *fileHdr = new FileHeader;
    ASSERT(fileHdr->Allocate(freeMap, MaxFileSize));
    fileHdr->initialize(REGULAR_FILE, DirectorySector);
    fileHdr->WriteBack(headerSector);
    directory->Add("testMax", headerSector);
    sectorAllocateLock.Release();
    delete fileHdr;
}

//mkdir at current dir
bool FileSystem::mkdir(char * name){
    DEBUG('t', "Enter FileSystem::mkdir.\n");
    CreateFileOrDir(name, DirectoryFileSize, DIRECTORY);
    DEBUG('t', "Leave FileSystem::mkdir.\n");
    return TRUE;
}

bool FileSystem::CreateFileOrDir(char * name, int initialSize, int fileType){
    DEBUG('f', "Enter FileSystem::CreateFileOrDir.\n");
    sectorAllocateLock.Acquire();
    OpenFile * currentDirFile = new OpenFile(currentDirHeaderSector);
    Directory * currentDir = new Directory(NumDirEntries);
    currentDir->FetchFrom(currentDirFile);


    BitMap * freeMap = new BitMap(NumSectors);
    // get lock before we can allocate or recycle sectors

    freeMap->FetchFrom(freeMapFile);

    bool success = TRUE;

    int headerSector = freeMap->Find();
    if (headerSector < 0){
        success = FALSE;
    } else if (!currentDir->Add(name, headerSector)){
        success = FALSE;
    } else {
        FileHeader *hdr = new FileHeader;
        if (!hdr->Allocate(freeMap, initialSize)){
            success = FALSE;
            currentDir->Remove(name);
        } else {
            hdr->initialize(fileType, currentDirHeaderSector);
            hdr->WriteBack(headerSector);
            currentDir->WriteBack(currentDirFile);
            freeMap->WriteBack(freeMapFile);
        }
        delete hdr;
    }
    sectorAllocateLock.Release();
    delete freeMap;
    delete currentDir;
    delete currentDirFile;
    DEBUG('f', "Leave FileSystem::CreateFileOrDir with success == %d.\n", success);
    return success;
}

// rm dir if empty
bool FileSystem::rmdir(char * name){
    DEBUG('t', "Enter FileSystem::rmdir.\n");
    sectorAllocateLock.Acquire();
    OpenFile * currentDirFile = new OpenFile(currentDirHeaderSector);
    Directory * currentDir = new Directory(NumDirEntries);
    currentDir->FetchFrom(currentDirFile);

    bool success = TRUE;

    //get sub dir
    int dstHeaderSector = currentDir->Find(name);
    if (dstHeaderSector < 0){
        printf("Dir '%s' not found.\n", name);
        success = FALSE;
    } else {
        OpenFile * dstDirFile = new OpenFile(dstHeaderSector);
        Directory * dstDir = new Directory(NumDirEntries);
        dstDir->FetchFrom(dstDirFile);
        if (dstDir->numUsed() > 0){
            printf("Dir '%s' is not empty.\n", name);
            success = FALSE;
        } else {
            BitMap * freeMap = new BitMap(NumSectors);

            freeMap->FetchFrom(freeMapFile);

            FileHeader * dstHdr = new FileHeader;
            dstHdr->FetchFrom(dstHeaderSector);
            dstHdr->Deallocate(freeMap);
            freeMap->Clear(dstHeaderSector);
            currentDir->Remove(name);

            freeMap->WriteBack(freeMapFile);

            currentDir->WriteBack(currentDirFile);

            delete freeMap;
            delete dstHdr;
            printf("Dir '%s' removed successfully.\n", name);
        }
        delete dstDir;
        delete dstDirFile;
    }
    sectorAllocateLock.Release();
    delete currentDir;
    delete currentDirFile;
    DEBUG('t', "Leave FileSystem::rmdir.\n");
    return success;
}

// rm dir recursive
bool FileSystem::rmdirRecursive(char * name){
    //TODO
}

// to sub or father dir.
bool FileSystem::cd(char * name){
    DEBUG('t', "Enter FileSystem::cd.\n");
    if (!strcmp(name, "/")){
        currentDirHeaderSector = DirectorySector;
        return TRUE;
    }
    sectorAllocateLock.Acquire();

    OpenFile * currentDirFile = new OpenFile(currentDirHeaderSector);
    Directory * currentDir = new Directory(NumDirEntries);
    currentDir->FetchFrom(currentDirFile);
    bool success = TRUE;
    FileHeader * currentHdr = new FileHeader;
    currentHdr->FetchFrom(currentDirHeaderSector);

    //father dir
    if (!strcmp(name, "..")){
        int fatherPathSector = currentHdr->getPathSector();
        if (fatherPathSector == NO_PATH_SECTOR){
            printf("Dir '%s' has no father directory.\n", name);
            success = FALSE;
        } else {
            currentDirHeaderSector = fatherPathSector;
        }
    } else {//get sub dir
        int dstHeaderSector = currentDir->Find(name);
        if (dstHeaderSector < 0){
            printf("Dir '%s' not found.\n", name);
            success = FALSE;
        } else {
            DEBUG('a', "cd dst sector %d", dstHeaderSector);
            currentDirHeaderSector = dstHeaderSector;
        }
    }
    sectorAllocateLock.Release();
    delete currentHdr;
    delete currentDir;
    delete currentDirFile;
    DEBUG('t', "Leave FileSystem::cd.\n");
    return success;
}
// asynchronous
char *FileSystem::getCurrentDirName(){
    if (currentDirHeaderSector == DirectorySector)
        return "/";
    FileHeader *curHdr = new FileHeader;
    curHdr->FetchFrom(currentDirHeaderSector);
    OpenFile * fathDirFile = new OpenFile(curHdr->getPathSector());
    DEBUG('a', "getPathSector is %d\n", curHdr->getPathSector());
    Directory * fathDir = new Directory(DirectoryFileSize);
    fathDir->FetchFrom(fathDirFile);
    char * name = fathDir->getFileName(currentDirHeaderSector); 
    return name;
}
// asynchronous
void FileSystem::testDirOps(){
    char * dirName = getCurrentDirName();
    printf("%s $ mkdir test1\n", dirName);
    mkdir("test1");
    printf("%s $ cd test1\n", dirName);
    cd("test1");
    dirName = getCurrentDirName();
    printf("%s $ mkdir sub1\n", dirName);
    mkdir("sub1");
    printf("%s $ cd sub1\n", dirName);
    cd("sub1");
    dirName = getCurrentDirName();
    printf("%s $ ls\n", dirName);
    List();
    printf("%s $ create txt\n", dirName);
    Create("txt", 8);
    printf("%s $ ls\n", dirName);
    List();
    printf("%s $ rm txt\n", dirName);
    Remove("txt");
    printf("%s $ ls\n", dirName);
    List();
    printf("%s $ cd ..\n", dirName);
    cd("..");
    dirName = getCurrentDirName();
    printf("%s $ rm sub1\n", dirName);
    Remove("sub1");
    printf("%s $ ls\n", dirName);
    List();
    printf("%s $ cd /\n", dirName);
    cd("/");
    dirName = getCurrentDirName();
    printf("%s $\n", dirName);
}

bool FileSystem::ExtendSize(char * name, int numExtendBytes){
    DEBUG('t', "Enter FileSystem::ExtendSize.\n");
    sectorAllocateLock.Acquire();
    OpenFile * currentDirFile = new OpenFile(currentDirHeaderSector);
    Directory *directory = new Directory(NumDirEntries);
    directory->FetchFrom(currentDirFile);

    BitMap *freeMap;
    FileHeader *fileHdr;
    int sector;
    
    sector = directory->Find(name);
    if (sector == -1) {
        printf("File '%s' not found.\n", name);
       delete directory;
       delete currentDirFile;
        sectorAllocateLock.Release();
       return FALSE;             // file not found 
    }
    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);
    // although we don't want to read or write the file, 
    // open the file when try to extend size. the reason is
    // extending size is also some kind of writing.
    OpenFile * dstFile = new OpenFile(sector);
    //. check if any thread is using this file
    FileACEntry * entry = (FileACEntry *) fileACList->getACEntry(sector);
    ASSERT(entry != NULL);
    entry->readWriteLock->BeforeWrite();

    freeMap = new BitMap(NumSectors);

    freeMap->FetchFrom(freeMapFile);

    bool success = fileHdr->extendSize(numExtendBytes, freeMap);

    if (success){
        freeMap->WriteBack(freeMapFile);        // flush to disk
        printf("File '%s' size extension succeeded.\n", name);
    } else {
        printf("File '%s' extended failed.\n", name);
    }

    entry->readWriteLock->AfterWrite();

    sectorAllocateLock.Release();

    delete freeMap;
    delete dstFile;
    delete fileHdr;
    delete directory;
    delete currentDirFile;
    DEBUG('t', "Leave FileSystem::ExtendSize.\n");
    return TRUE;
}
bool FileSystem::ShrinkSize(char * name, int numShrinkBytes){
    DEBUG('t', "Enter FileSystem::ShrinkSize.\n");
    sectorAllocateLock.Acquire();
    OpenFile * currentDirFile = new OpenFile(currentDirHeaderSector);
    Directory *directory = new Directory(NumDirEntries);
    directory->FetchFrom(currentDirFile);

    BitMap *freeMap;
    FileHeader *fileHdr;
    int sector;
    
    sector = directory->Find(name);
    if (sector == -1) {
        printf("File '%s' not found.\n", name);
       delete directory;
       delete currentDirFile;
        sectorAllocateLock.Release();
       return FALSE;             // file not found 
    }
    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);
    // although we don't want to read or write the file, 
    // open the file when try to shrink size. the reason is
    // shrinking size is also some kind of writing.
    OpenFile * dstFile = new OpenFile(sector);
    //. check if any thread is using this file
    FileACEntry * entry = (FileACEntry *) fileACList->getACEntry(sector);
    ASSERT(entry != NULL);
    entry->readWriteLock->BeforeWrite();

    freeMap = new BitMap(NumSectors);

    freeMap->FetchFrom(freeMapFile);

    bool success = fileHdr->shrinkSize(numShrinkBytes, freeMap);

    if (success){
        freeMap->WriteBack(freeMapFile);        // flush to disk
        printf("File '%s' size shrinking succeeded.\n", name);
    } else {
        printf("File '%s' shrinking failed.\n", name);
    }

    entry->readWriteLock->AfterWrite();
    sectorAllocateLock.Release();

    delete freeMap;
    delete dstFile;
    delete fileHdr;
    delete directory;
    delete currentDirFile;
    DEBUG('t', "Leave FileSystem::ShrinkSize.\n");
    return TRUE;
}

// asynchronous
// concurrent op is not surpported.
void FileSystem::testExtensibleFileSize(){
    char testFile[10] = "extSize";
    Create(testFile, 100);
    BitMap * freeMap = new BitMap(NumSectors);
    freeMap->FetchFrom(freeMapFile);
    OpenFile * currentDirFile = new OpenFile(currentDirHeaderSector);
    Directory * currentDir = new Directory(NumDirEntries);
    currentDir->FetchFrom(currentDirFile);
    int dstHeaderSector = currentDir->Find(testFile);
    FileHeader * dstHdr = new FileHeader;
    dstHdr->FetchFrom(dstHeaderSector);

    printf("***** before extending *****\n");    
    dstHdr->Print(FALSE);
    freeMap->Print();

    dstHdr->extendSize(4000, freeMap);
    dstHdr->WriteBack(dstHeaderSector);
    printf("***** after extending 4000 bytes*****\n");    
    dstHdr->Print(FALSE);
    freeMap->Print();

    dstHdr->shrinkSize(50, freeMap);
    dstHdr->WriteBack(dstHeaderSector);
    printf("***** after shrinking 50 bytes *****\n");    
    dstHdr->Print(FALSE);
    freeMap->Print();

    dstHdr->shrinkSize(4000, freeMap);
    dstHdr->WriteBack(dstHeaderSector);
    printf("***** after shrinking 4000 bytes *****\n");    
    dstHdr->Print(FALSE);
    freeMap->Print();

    delete dstHdr;
    delete currentDir;
    delete currentDirFile;
    delete freeMap;
}


void FileSystem::beforeRead(int headerSector){
    FileACEntry * entry = (FileACEntry *)fileACList->getACEntry(headerSector);
    ASSERT(entry != NULL);
    entry->readWriteLock->BeforeRead();
}
void FileSystem::afterRead(int headerSector){
    FileACEntry * entry = (FileACEntry *)fileACList->getACEntry(headerSector);
    ASSERT(entry != NULL);
    entry->readWriteLock->AfterRead();
}
void FileSystem::beforeWrite(int headerSector){
    FileACEntry * entry = (FileACEntry *)fileACList->getACEntry(headerSector);
    ASSERT(entry != NULL);
    entry->readWriteLock->BeforeWrite();    
}
void FileSystem::afterWrite(int headerSector){
    FileACEntry * entry = (FileACEntry *)fileACList->getACEntry(headerSector);
    ASSERT(entry != NULL);
    entry->readWriteLock->AfterWrite();    
}
/*bool deletable(int headerSector){
    bool isDeletable = TRUE;
    FileACEntry * entry = (FileACEntry *) fileACList->getACEntry(headerSector);
    ASSERT(entry != NULL);
    entry->numLock->Acquire();
    if (entry->numThreads > 0)
        isDeletable = FALSE;
    entry->numLock->Release();
    return isDeletable;
}*/
/*bool FileSystem::Close(int headerSector){
    FileACEntry * entry = (FileACEntry *)fileACList->getACEntry(headerSector);
    ASSERT(entry != NULL);
    entry->numLock->Acquire();
    entry->numThreads -= 1;
    ASSERT(entry->numThreads >= 0);
    entry->numLock->Release();
    return TRUE;
}*/

void funcToTestConcurRW(int arg){
    char testFile[10] = "concurIO";
    printf("thread %d tried to create file '%s'.\n", currentThread->getTid(), testFile);
    if (fileSystem->Create(testFile, 256)){
        printf("thread %d succeeded in creating file '%s'.\n", currentThread->getTid(), testFile);
    } else {
        printf("thread %d failed to create file '%s'.\n", currentThread->getTid(), testFile);
    }
    OpenFile * openFl = fileSystem->Open(testFile);
    ASSERT(openFl != NULL);
    printf("thread %d opened file '%s'.\n", currentThread->getTid(), testFile);

    int bufLen = 250;
    char buf[250];
    char ach = 'a', bch = 'b';
    if (arg == 2){
        ach = 'c';
        bch = 'd';
    }
    for (int i = 0; i < bufLen; i += 2){
        buf[i] = ach;
        buf[i + 1] = bch;
    }
    printf("thread %d tried to write file '%s'.\n", currentThread->getTid(), testFile);
    openFl->Write(buf, bufLen);
    printf("thread %d wrote file '%s'.\n", currentThread->getTid(), testFile);
    printf("thread %d yielded.\n", currentThread->getTid());
    currentThread->Yield();
    printf("thread %d recovered from yield.\n", currentThread->getTid());
    for (int i = 0; i < bufLen; i += 2){
        buf[i] = bch;
        buf[i + 1] = ach;
    }
    printf("thread %d tried to write file '%s'.\n", currentThread->getTid(), testFile);
    openFl->Write(buf, bufLen);
    printf("thread %d wrote file '%s'.\n", currentThread->getTid(), testFile);
    printf("thread %d tried to remove file '%s'.\n", currentThread->getTid(), testFile);
    if (fileSystem->Remove(testFile)){
        printf("thread %d succeeded in removing file '%s'.\n", currentThread->getTid(), testFile);
    } else {
        printf("thread %d failed to remove file '%s'.\n", currentThread->getTid(), testFile);
    }
    delete openFl;
    printf("thread %d tried to remove file '%s'.\n", currentThread->getTid(), testFile);
    if (fileSystem->Remove(testFile)){
        printf("thread %d succeeded in removing file '%s'.\n", currentThread->getTid(), testFile);
    } else {
        printf("thread %d failed to remove file '%s'.\n", currentThread->getTid(), testFile);
    }
    fileSystem->Print(TRUE);
}
void FileSystem::testConcurrentReadWrite(){
    Thread * t1 = createThread("concurRW 1", 4);
    Thread * t2 = createThread("concurRW 2", 4);
    if (t1 == NULL || t2 == NULL)
        return;
    t1->Fork(funcToTestConcurRW, 1);
    t2->Fork(funcToTestConcurRW, 2);
}

    