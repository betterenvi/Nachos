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

static Lock sectorAllocateLock;
static Lock dirAllocateLock;
static int headerSectorToFind;
static FileACEntry * foundACEntry;
static void FindACEntry(int acEntry){
    FileACEntry * entry = (FileACEntry *)acEntry;
    if (entry->headerSector == headerSectorToFind)
        foundACEntry = entry;
}

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
	    directory->Print();

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
    DEBUG('t', "Enter FileSystem::Create.\n");
    bool success = CreateFileOrDir(name, initialSize, REGULAR_FILE);
    DEBUG('t', "Leave FileSystem::Create.\n");
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
       return FALSE;			 // file not found 
    }
    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);
    //. check if any thread is using this file
    FileACEntry * entry = (FileACEntry *) getACEntry(sector);
    ASSERT(*entry);
    entry->numLock->Acquire();
    if (entry->numThreads > 0){
        printf("File '%s' is in use.\n", name);
        entry->numLock->Release();
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

    delete fileHdr;
    delete directory;
    delete freeMap;
    delete currentDirFile;
    printf("File '%s' removed successfully.\n", name);
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
    OpenFile * currentDirFile = new OpenFile(currentDirHeaderSector);
    Directory *currentDir = new Directory(NumDirEntries);
    currentDir->FetchFrom(currentDirFile);
    currentDir->List();
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
FileSystem::Print()
{
    DEBUG('t', "Enter FileSystem::Print.\n");
    OpenFile * currentDirFile = new OpenFile(currentDirHeaderSector);
    FileHeader *bitHdr = new FileHeader;
    FileHeader *dirHdr = new FileHeader;
    BitMap *freeMap = new BitMap(NumSectors);
    Directory *directory = new Directory(NumDirEntries);

    printf("Bit map file header:\n");
    bitHdr->FetchFrom(FreeMapSector);
    bitHdr->Print(FALSE);

    printf("Directory file header:\n");
    dirHdr->FetchFrom(DirectorySector);
    dirHdr->Print(FALSE);

    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();

    directory->FetchFrom(currentDirFile);
    directory->Print();

    delete bitHdr;
    delete dirHdr;
    delete freeMap;
    delete directory;
    delete currentDirFile;
    DEBUG('t', "Leave FileSystem::Print.\n");
} 
void FileSystem::testMaxFileSize(void *freeMap_, void * directory_){
    BitMap * freeMap = (BitMap *)freeMap_;
    Directory * directory = (Directory *)directory_;
    int headerSector = freeMap->Find();
    FileHeader *fileHdr = new FileHeader;
    ASSERT(fileHdr->Allocate(freeMap, MaxFileSize));
    fileHdr->initialize(REGULAR_FILE, DirectorySector);
    fileHdr->WriteBack(headerSector);
    directory->Add("testMax", headerSector);
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
    DEBUG('t', "Enter FileSystem::CreateFileOrDir.\n");
    OpenFile * currentDirFile = new OpenFile(currentDirHeaderSector);
    Directory * currentDir = new Directory(NumDirEntries);
    currentDir->FetchFrom(currentDirFile);

    BitMap * freeMap = new BitMap(NumSectors);
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
    delete freeMap;
    delete currentDir;
    delete currentDirFile;
    DEBUG('t', "Leave FileSystem::CreateFileOrDir.\n");
    return TRUE;
}

// rm dir if empty
bool FileSystem::rmdir(char * name){
    DEBUG('t', "Enter FileSystem::rmdir.\n");
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
    delete currentHdr;
    delete currentDir;
    delete currentDirFile;
    DEBUG('t', "Leave FileSystem::cd.\n");
    return success;
}

char *FileSystem::getCurrentDirName(){
    if (currentDirHeaderSector == DirectorySector)
        return "/";
    FileHeader *curHdr = new FileHeader;
    curHdr->FetchFrom(currentDirHeaderSector);
    OpenFile * fathDirFile = new OpenFile(curHdr->getPathSector());
    DEBUG('a', "getPathSector is %d\n", curHdr->getPathSector());
    Directory * fathDir = new Directory(DirectoryFileSize);
    fathDir->FetchFrom(fathDirFile);
    return fathDir->getFileName(currentDirHeaderSector); 
}
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
void * FileSystem::getACEntry(int headerSector){
    headerSectorToFind = headerSector;
    foundACEntry = NULL;
    fileACList->Mapcar(FindACEntry);
    return foundACEntry;
}

void FileSystem::beforeRead(int headerSector){
    FileACEntry * entry = (FileACEntry *)getACEntry(headerSector);
    ASSERT(entry != NULL);
    entry->readWriteLock->BeforeRead();
}
void FileSystem::afterRead(int headerSector){
    FileACEntry * entry = (FileACEntry *)getACEntry(headerSector);
    ASSERT(entry != NULL);
    entry->readWriteLock->AfterRead();
}
void FileSystem::beforeWrite(int headerSector){
    FileACEntry * entry = (FileACEntry *)getACEntry(headerSector);
    ASSERT(entry != NULL);
    entry->readWriteLock->BeforeWrite();    
}
void FileSystem::afterWrite(int headerSector){
    FileACEntry * entry = (FileACEntry *)getACEntry(headerSector);
    ASSERT(entry != NULL);
    entry->readWriteLock->AfterWrite();    
}
/*bool deletable(int headerSector){
    bool isDeletable = TRUE;
    FileACEntry * entry = (FileACEntry *) getACEntry(headerSector);
    ASSERT(entry != NULL);
    entry->numLock->Acquire();
    if (entry->numThreads > 0)
        isDeletable = FALSE;
    entry->numLock->Release();
    return isDeletable;
}*/
/*bool FileSystem::Close(int headerSector){
    FileACEntry * entry = (FileACEntry *)getACEntry(headerSector);
    ASSERT(entry != NULL);
    entry->numLock->Acquire();
    entry->numThreads -= 1;
    ASSERT(entry->numThreads >= 0);
    entry->numLock->Release();
    return TRUE;
}*/
void FileSystem::UpdateFileACListWhenOpenFile(int headerSector){
    FileACEntry * entry = (FileACEntry *)getACEntry(headerSector);
    if (entry == NULL){
        entry = new FileACEntry(headerSector);
        fileACList->Append((void *) entry);
    }
    entry->numLock->Acquire();
    entry->numThreads += 1;
    entry->numLock->Release();
}
void FileSystem::UpdateFileACListWhenCloseFile(int headerSector){
    FileACEntry * entry = (FileACEntry *)getACEntry(headerSector);
    ASSERT(entry != NULL);
    entry->numLock->Acquire();
    entry->numThreads -= 1;
    ASSERT(entry->numThreads >= 0);
    entry->numLock->Release();
}
void FileSystem::testConcurrentReadWrite(){
    char testFile[10] = "concurIO";
    Create(testFile, 256);
    BitMap * freeMap = new BitMap(NumSectors);
    freeMap->FetchFrom(freeMapFile);
    OpenFile * currentDirFile = new OpenFile(currentDirHeaderSector);
    Directory * currentDir = new Directory(NumDirEntries);
    currentDir->FetchFrom(currentDirFile);
    int dstHeaderSector = currentDir->Find(testFile);
    FileHeader * dstHdr = new FileHeader;
    dstHdr->FetchFrom(dstHeaderSector);
}
