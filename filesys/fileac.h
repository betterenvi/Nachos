//.
#ifndef FILEAC_H
#define FILEAC_H
#include "synch.h"
#include "synchlist.h"
// file access control entry
class FileACEntry
{
public:
  FileACEntry(int headerSector_);
  ~FileACEntry();
  int headerSector;
  int numThreads;
  Lock * numLock;
  ReadWriteLock * readWriteLock;
};

class FileACList
{
public:
	FileACList();
	~FileACList();
	void * getACEntry(int headerSector);
	void UpdateFileACListWhenOpenFile(int headerSector);
	void UpdateFileACListWhenCloseFile(int headerSector);

private:
	SynchList * list;
	Lock * superLock;
	/* data */
};
//..
#endif