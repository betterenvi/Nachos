#include "copyright.h"
#include "synchconsole.h"

static void ConsoleReadAvail(int arg){
	SynchConsole * consl = (SynchConsole *)arg;
	consl->ReadAvailHandler();
}
static void ConsoleWriteDone(int arg){
	SynchConsole * consl = (SynchConsole *)arg;
	consl->WriteDoneHandler();
}
SynchConsole::SynchConsole(char * consoleName){
    readAvail = new Semaphore("synch console readAvail", 0);
    writeDone = new Semaphore("synch console writeDone", 0);
    readLock = new Lock("synch console readLock");
    writeLock = new Lock("synch console writeLock");
    console = new Console(NULL, NULL, ConsoleReadAvail,
    	ConsoleWriteDone, (int)this);
}
SynchConsole::~SynchConsole(){
	delete readAvail;
	delete writeDone;
	delete readLock;
	delete writeLock;
	delete console;
}
char SynchConsole::ReadChar(){
	readLock->Acquire();
	readAvail->P();
	char ch = console->GetChar();
	readLock->Release();
	return ch;
}
void SynchConsole::ReadAvailHandler(){
	readAvail->V();
}
void SynchConsole::WriteChar(char ch){
	writeLock->Acquire();
	writeDone->P();
	console->PutChar(ch);
	writeLock->Release();
}
void SynchConsole::WriteDoneHandler(){
	writeDone->V();
}
