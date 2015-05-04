// filehdr.cc 
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the 
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the 
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect 
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector, 
//
//      Unlike in a real system, we do not keep track of file permissions, 
//	ownership, last modification date, etc., in the file header. 
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"
#include "filehdr.h"

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------

bool
FileHeader::Allocate(BitMap *freeMap, int fileSize)
{ 
    DEBUG('a', "FileHeader::Allocate request fileSize %d\n", fileSize);
    ASSERT(fileSize <= MaxFileSize);
    numBytes = fileSize;
    numSectors  = divRoundUp(fileSize, SectorSize);
    if (freeMap->NumClear() < numSectors)
	return FALSE;		// not enough space
//.
 //   for (int i = 0; i < numSectors; i++)
//	dataSectors[i] = freeMap->Find();
    DEBUG('a', "FileHeader::Allocate enough bit map\n");
    int * firstLevelSector = NULL;
    if (numSectors > RealNumDirect){
        firstLevelSector = new int[NumFirstLevel];
        dataSectors[RealNumDirect] = freeMap->Find();
        DEBUG('a', "dataSectors[RealNumDirect] %d\n", dataSectors[RealNumDirect]);
        // set next unused pointer as invalid.
        if (numSectors < MaxFileSectors)
            firstLevelSector[numSectors - RealNumDirect] = INVALID_POINTER;
    }else{
        dataSectors[numSectors] = INVALID_POINTER;
    }
    for (int i = 0; i < numSectors; ++i){
        int * location = IndexToLocation(i, firstLevelSector);
        *location = freeMap->Find();
        DEBUG('a', "%d %d %d\n", i, (int)location, *location);
    }
    if (numSectors > RealNumDirect){
        synchDisk->WriteSector(dataSectors[RealNumDirect], (char *)firstLevelSector);
        DEBUG('a', "firstLevelSector %d\n", firstLevelSector[0]);
        delete firstLevelSector;
    }

/*    if (numSectors <= RealNumDirect){
        DEBUG('a', "FileHeader::Allocate enough direct sectors\n");
        for (int i = 0; i < numSectors; i++)
            dataSectors[i] = freeMap->Find();
        for (int i = numSectors; i < RealNumDirect; ++i)
            dataSectors[i] = INVALID_POINTER;
        dataSectors[RealNumDirect] = INVALID_POINTER;
    } else {
        DEBUG('a', "FileHeader::Allocate not enough direct sectors\n");
        for (int i = 0; i < RealNumDirect; i++)
            dataSectors[i] = freeMap->Find();
        dataSectors[RealNumDirect] = freeMap->Find();
        int remain = numSectors - RealNumDirect;
        int sector_data[NumFirstLevel];
        for (int i = 0; i < remain; ++i)
            sector_data[i] = freeMap->Find();
        for (int i = remain; i < NumFirstLevel; ++i)
            sector_data[i] = INVALID_POINTER;
        synchDisk->WriteSector(dataSectors[RealNumDirect], (char *)sector_data);
    }*/
//..
    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void 
FileHeader::Deallocate(BitMap *freeMap)
{
    //.
//    for (int i = 0; i < numSectors; i++) {
//	ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
//	freeMap->Clear((int) dataSectors[i]);
//    }
    int * firstLevelSector = getFirstLevelSector();
    for(int i = 0; i < numSectors; ++i){
        int sector = IndexToSector(i, firstLevelSector);
        ASSERT(freeMap->Test((int) sector));
        freeMap->Clear((int)sector);
    }
    if (numSectors > RealNumDirect){
        ASSERT(freeMap->Test((int)dataSectors[RealNumDirect]));
        freeMap->Clear((int)dataSectors[RealNumDirect]);
    }
    dataSectors[0] = INVALID_POINTER;
    numSectors = 0;
    deleteFirstLevelSector(firstLevelSector);
    //..
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk. 
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void
FileHeader::FetchFrom(int sector)
{
    DEBUG('f', "Enter FileHeader::FetchFrom %d\n", sector);
    synchDisk->ReadSector(sector, (char *)this);
    DEBUG('f', "Leave FileHeader::FetchFrom %d\n", sector);
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk. 
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void
FileHeader::WriteBack(int sector)
{
    synchDisk->WriteSector(sector, (char *)this); 
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

int
FileHeader::ByteToSector(int offset)
{
    //.return(dataSectors[offset / SectorSize]);
    if (offset < RealDirectMaxSize)
        return (dataSectors[offset / SectorSize]);
    int remain = offset - RealDirectMaxSize;
    int firstLevelSector[NumFirstLevel];
    synchDisk->ReadSector(dataSectors[RealNumDirect], (char *) firstLevelSector);
    return (firstLevelSector[remain / SectorSize]);
    //..
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int
FileHeader::FileLength()
{
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void
FileHeader::Print(bool printContent)
{
    int i, j, k;
    char *data = new char[SectorSize];
    //.
    printf("\n***** File meta info ******\n");
    printf("Type: %s\n", (type == REGULAR_FILE)?"file":"dir");
    printf("Created at: %d\n", creatTime);
    printf("Last Accessed at: %d\n", lastAccessTime);
    printf("Last Modified at: %d\n", lastModifyTime);
    printf("Path sector: %d\n", pathSector);
    printf("FileHeader contents.  File size: %d.  File blocks:\n", numBytes);
    int * firstLevelSector = getFirstLevelSector();
    for (i = 0; i < numSectors; ++i){
        printf("%d ", IndexToSector(i, firstLevelSector));
    }
    printf("\n");
    if (!printContent){
        delete [] data;
        deleteFirstLevelSector(firstLevelSector);
        return;
    }
    printf("File contents:\n");
    for (i = k = 0; i < numSectors; i++) {
        synchDisk->ReadSector(IndexToSector(i, firstLevelSector), data);
        for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
    	    if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
    	       printf("%c", data[j]);
            else
                printf("\\%x", (unsigned char)data[j]);
        }
        printf("\n"); 
    }
    delete [] data;
    deleteFirstLevelSector(firstLevelSector);
}

//.
void FileHeader::initialize(int fileType, int filePathSector){
    type = fileType;
    creatTime = lastAccessTime = lastModifyTime = time(NULL);
    pathSector = filePathSector;
}

// idx:0, 1, 2, ...., numSectors - 1
int FileHeader::IndexToSector(int idx, int * firstLevelSector){
    if (idx < RealNumDirect)
        return dataSectors[idx];
    ASSERT(firstLevelSector != NULL);
    int remain = idx - RealNumDirect;
    DEBUG('a', "\n%d FileHeader::IndexToSector %d %d %d\n", dataSectors[RealNumDirect], idx, remain,firstLevelSector[remain]);
    return firstLevelSector[remain]; 
}

// get where the idx sector id is stored.
int * FileHeader::IndexToLocation(int idx, int * firstLevelSector){
    if (idx < RealNumDirect)
        return (dataSectors + idx);
    ASSERT(firstLevelSector != NULL);
    int remain = idx - RealNumDirect;
    return (firstLevelSector + remain);
}

bool FileHeader::extendSector(int numExtendSectors, int * firstLevelSector, BitMap *freeMap){
    if (numExtendSectors < 0)
        return FALSE;
    if (numExtendSectors == 0)
        return TRUE;
    int newNumSectors = numSectors + numExtendSectors;
    if (newNumSectors > MaxFileSectors)
        return FALSE;
    if (numSectors <= RealNumDirect && newNumSectors > RealNumDirect){
        firstLevelSector = new int[NumFirstLevel];
        dataSectors[RealNumDirect] = freeMap->Find();
        if (newNumSectors < MaxFileSectors)
            firstLevelSector[newNumSectors - RealNumDirect] = INVALID_POINTER;
    } else {
        dataSectors[newNumSectors] = INVALID_POINTER;
    }
    for (int i = numSectors; i < newNumSectors; ++i){
        int * location = IndexToLocation(i, firstLevelSector);
        *location = freeMap->Find();
    }
    if (newNumSectors > RealNumDirect){
        synchDisk->WriteSector(dataSectors[RealNumDirect], (char *)firstLevelSector);
        //delete firstLevelSector;
    }
    numSectors = newNumSectors;
    return TRUE;
}

bool FileHeader::shrinkSector(int numShrinkSectors, int * firstLevelSector, BitMap *freeMap){
    if (numShrinkSectors < 0)
        return FALSE;
    if (numShrinkSectors == 0)
        return TRUE;
    int newNumSectors = numSectors - numShrinkSectors;
    if (newNumSectors < 0)
        return FALSE;
    for (int i = newNumSectors; i < numSectors; ++i){
        int sector = IndexToSector(i, firstLevelSector);
        ASSERT(freeMap->Test((int)sector));
        freeMap->Clear((int)sector);
    }
    if (newNumSectors < MaxFileSectors){//must be true
        int * location = IndexToLocation(newNumSectors, firstLevelSector);
        *location = INVALID_POINTER;
    }
    if (numSectors > RealNumDirect){
        if (newNumSectors <= RealNumDirect){
            ASSERT(freeMap->Test((int)dataSectors[RealNumDirect]));
            freeMap->Clear((int)dataSectors[RealNumDirect]);
        } else {
            synchDisk->WriteSector(dataSectors[RealNumDirect], (char *)firstLevelSector);
        }
    }
    numSectors = newNumSectors;    
    return TRUE;
}

int *FileHeader::getFirstLevelSector(){
    if (numSectors > RealNumDirect){
        int * firstLevelSector = new int[NumFirstLevel];
        synchDisk->ReadSector(dataSectors[RealNumDirect], (char *)firstLevelSector);
        return firstLevelSector;
    }
    return NULL;
}
bool FileHeader::deleteFirstLevelSector(int * firstLevelSector){
    if (numSectors > RealNumDirect)
        delete firstLevelSector;
}

bool FileHeader::extendSize(int numextendBytes, BitMap * freeMap){
    if (numextendBytes < 0)
        return FALSE;
    if (numextendBytes == 0)
        return TRUE;
    int newNumBytes = numBytes + numextendBytes;
    if (newNumBytes > MaxFileSize)
        return FALSE;
    int numExtendSectors = divRoundUp(newNumBytes, SectorSize) - numSectors;
    int * firstLevelSector = getFirstLevelSector();
    if (!extendSector(numExtendSectors, firstLevelSector, freeMap))
        return FALSE;
    numBytes = newNumBytes;
    deleteFirstLevelSector(firstLevelSector);
    return TRUE;
}

bool FileHeader::shrinkSize(int numShrinkBytes, BitMap * freeMap){
    if (numShrinkBytes < 0)
        return FALSE;
    if (numShrinkBytes == 0)
        return TRUE;
    int newNumBytes = numBytes - numShrinkBytes;
    if (newNumBytes < 0)
        return FALSE;
    int numShrinkSectors = numSectors - divRoundUp(newNumBytes, SectorSize);
    int * firstLevelSector = getFirstLevelSector();
    if (!shrinkSector(numShrinkSectors, firstLevelSector, freeMap))
        return FALSE;
    numBytes = newNumBytes;
    deleteFirstLevelSector(firstLevelSector);
    return TRUE;
}