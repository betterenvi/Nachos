//.
#ifndef FILEAC_H
#define FILEAC_H
#include "synch.h"
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
//..
#endif