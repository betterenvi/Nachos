#include "copyright.h"
#ifndef SYNCHCONSOLE_H
#define SYNCHCONSOLE_H

#include "console.h"
#include "synch.h"

class SynchConsole
{
public:
	SynchConsole(char * consoleName);
	~SynchConsole();

	char ReadChar();
	void WriteChar(char ch);	
    void WriteDoneHandler();
    void ReadAvailHandler();

  private:
    Console *console;
    Semaphore *readAvail; 	
    Semaphore *writeDone;
    Lock *readLock;
    Lock *writeLock;
};
#endif