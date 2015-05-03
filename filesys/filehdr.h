// filehdr.h 
//	Data structures for managing a disk file header.  
//
//	A file header describes where on disk to find the data in a file,
//	along with other information about the file (for instance, its
//	length, owner, etc.)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef FILEHDR_H
#define FILEHDR_H

#include "disk.h"
#include "bitmap.h"
//.
#include <time.h>
//. last entry as fisrt level index
#define NumFirstLevel (SectorSize / sizeof(int))
#define FisrtLevelMaxSize   (NumFirstLevel * SectorSize)
#define NumDirect 	((SectorSize  / sizeof(int)) - 7)
#define RealNumDirect (NumDirect - 1)
#define DirectMaxSize  (RealNumDirect * SectorSize)
#define MaxFileSectors   (RealNumDirect + NumFirstLevel)
#define MaxFileSize 	(MaxFileSectors * SectorSize)
//.
#define REGULAR_FILE 0
#define DIRECTORY 1
#define NO_PATH_SECTOR (-1)
#define INVALID_POINTER 0       // sector 0 is header of bitmap
//..
// The following class defines the Nachos "file header" (in UNIX terms,  
// the "i-node"), describing where on disk to find all of the data in the file.
// The file header is organized as a simple table of pointers to
// data blocks. 
//
// The file header data structure can be stored in memory or on disk.
// When it is on disk, it is stored in a single sector -- this means
// that we assume the size of this data structure to be the same
// as one disk sector.  Without indirect addressing, this
// limits the maximum file length to just under 4K bytes.
//
// There is no constructor; rather the file header can be initialized
// by allocating blocks for the file (if it is a new file), or by
// reading it from disk.

class FileHeader {
  public:
    bool Allocate(BitMap *bitMap, int fileSize);// Initialize a file header, 
						//  including allocating space 
						//  on disk for the file data
    void Deallocate(BitMap *bitMap);  		// De-allocate this file's 
						//  data blocks

    void FetchFrom(int sectorNumber); 	// Initialize file header from disk
    void WriteBack(int sectorNumber); 	// Write modifications to file header
					//  back to disk

    int ByteToSector(int offset);	// Convert a byte offset into the file
					// to the disk sector containing
					// the byte

    int FileLength();			// Return the length of the file 
					// in bytes

    void Print();			// Print the contents of the file.
    //.
    void initialize(int fileType, int filePathSector);
    int getFileType(){return type;}
    int getCreatTime(){return creatTime;}
    int getLastAccessTime(){return lastAccessTime;}
    void setLastAccessTime(int time){lastAccessTime = time;}
    void updateLastAccessTime(){lastAccessTime = time(NULL);}
    int getLastModifyTime(){return lastModifyTime;}
    void setLastModifyTime(int time){lastModifyTime = time;}
    void updateLastModifyTime(){lastModifyTime = time(NULL);}
    int getPathSector(){return pathSector;}
    void setPathSector(int filePathSector){pathSector = filePathSector;}
    int IndexToSector(int idx);
    int *IndexToLocation(int idx, int * firstLevelSector);

    //..
 // private:
    int numBytes;			// Number of bytes in the file
    int numSectors;			// Number of data sectors in the file
    int dataSectors[NumDirect];		// Disk sector numbers for each data 
					// block in the file
    //.
    int type;       //REGULAR_FILE or DIRECTORY
    int creatTime;
    int lastAccessTime;
    int lastModifyTime;
    int pathSector; //father directory's header sector // can be NO_PATH_SECTOR

    //..
};

#endif // FILEHDR_H
