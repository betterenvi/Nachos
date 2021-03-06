// filesys.h 
//	Data structures to represent the Nachos file system.
//
//	A file system is a set of files stored on disk, organized
//	into directories.  Operations on the file system have to
//	do with "naming" -- creating, opening, and deleting files,
//	given a textual file name.  Operations on an individual
//	"open" file (read, write, close) are to be found in the OpenFile
//	class (openfile.h).
//
//	We define two separate implementations of the file system. 
//	The "STUB" version just re-defines the Nachos file system 
//	operations as operations on the native UNIX file system on the machine
//	running the Nachos simulation.  This is provided in case the
//	multiprogramming and virtual memory assignments (which make use
//	of the file system) are done before the file system assignment.
//
//	The other version is a "real" file system, built on top of 
//	a disk simulator.  The disk is simulated using the native UNIX 
//	file system (in a file named "DISK"). 
//
//	In the "real" implementation, there are two key data structures used 
//	in the file system.  There is a single "root" directory, listing
//	all of the files in the file system; unlike UNIX, the baseline
//	system does not provide a hierarchical directory structure.  
//	In addition, there is a bitmap for allocating
//	disk sectors.  Both the root directory and the bitmap are themselves
//	stored as files in the Nachos file system -- this causes an interesting
//	bootstrap problem when the simulated disk is initialized. 
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef FS_H
#define FS_H

#include "copyright.h"
#include "openfile.h"

#ifdef FILESYS_STUB 		// Temporarily implement file system calls as 
				// calls to UNIX, until the real file system
				// implementation is available
class FileSystem {
  public:
    FileSystem(bool format) {}

    bool Create(char *name, int initialSize) { 
	int fileDescriptor = OpenForWrite(name);

	if (fileDescriptor == -1) return FALSE;
	Close(fileDescriptor); 
	return TRUE; 
	}

    OpenFile* Open(char *name) {
	  int fileDescriptor = OpenForReadWrite(name, FALSE);

	  if (fileDescriptor == -1) return NULL;
	  return new OpenFile(fileDescriptor);
      }

    bool Remove(char *name) { return Unlink(name) == 0; }
    //.
    bool isStub;
    bool Copy(char * src, int size, char * dst){
      Create(dst, size);
      OpenFile * srcOpenFile = Open(src);
      OpenFile * dstOpenFile = Open(dst);
      char * buffer = new char[size];
      srcOpenFile->Read(buffer, size);
      dstOpenFile->Write(buffer, size);
      delete buffer;
      delete srcOpenFile;
      delete dstOpenFile;
    }
    //..
};

#else // FILESYS
class FileSystem {
  public:
    FileSystem(bool format);		// Initialize the file system.
					// Must be called *after* "synchDisk" 
					// has been initialized.
    					// If "format", there is nothing on
					// the disk, so initialize the directory
    					// and the bitmap of free blocks.
    ~FileSystem();
    bool Create(char *name, int initialSize);  	
					// Create a file (UNIX creat)

    OpenFile* Open(char *name); 	// Open a file (UNIX open)

    bool Remove(char *name);  		// Delete a file (UNIX unlink)

    void List();			// List all the files in the file system

    void Print(bool printContent);			// List all the files and their contents
    //.
    bool CreateFileOrDir(char * name, int initialSize, int fileType);
    bool mkdir(char * name);  //mkdir at current dir
    bool rmdir(char * name);  // rm sub dir if empty
    bool rmdirRecursive(char * name); // rm dir recursive
    bool cd(char * name); // to sub or father dir.
    char *getCurrentDirName();
    bool ExtendSize(char * name, int numExtendBytes);
    bool ShrinkSize(char * name, int numShrinkBytes);
    // for synch
//    static void * getACEntry(int headerSector);
//    static void UpdateFileACListWhenOpenFile(int headerSector);
//    static void UpdateFileACListWhenCloseFile(int headerSector);
    //bool Close(int headerSector);
    void beforeRead(int headerSector);
    void afterRead(int headerSector);
    void beforeWrite(int headerSector);
    void afterWrite(int headerSector);

    //for test
    void testMaxFileSize(void *freeMap, void * directory);
    void testDirOps();
    void testExtensibleFileSize();
    void testConcurrentReadWrite();
    bool isReal;

    bool Copy(char * src, int size, char * dst);
    //..
  private:
   OpenFile* freeMapFile;		// Bit map of free disk blocks,
					// represented as a file
   OpenFile* directoryFile;		// "Root" directory -- list of 
					// file names, represented as a file
   //.
   int currentDirHeaderSector;    // current directory's header sector
   //..
};

#endif // FILESYS

#endif // FS_H
