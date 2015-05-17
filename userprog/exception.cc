// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.	Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.	
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.	See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
//.
#include "directory.h"
#include "openfile.h"
#define INIT_FILE_SIZE 256
//..

void TlbMissExceptionHandler(){
	DEBUG('d', "Enter TlbMissExceptionHandler\n");
	int badVAddr = machine->ReadRegister(BadVAddrReg);
	int vpn = (unsigned int) badVAddr / PageSize;
	machine->CachePageEntryInTLB(vpn);
	DEBUG('d', "Leave TlbMissExceptionHandler\n");
}

void PageFaultExceptionHandler(int vpn){
	DEBUG('d', "Enter PageFaultExceptionHandler\n");
	machine->LoadPageToMemory(vpn);
	DEBUG('d', "Leave PageFaultExceptionHandler\n");
}


void SysCallCreateHandler();
void SysCallOpenHandler();
void SysCallWriteHandler();
void SysCallReadHandler();
void SysCallCloseHandler();
void SysCallForkHandler();
void SysCallYieldHandler();   
void SysCallExecHandler();
void SysCallJoinHandler();   

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.	Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.	The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

void
ExceptionHandler(ExceptionType which)
{
		int type = machine->ReadRegister(2);

	 /* if ((which == SyscallException) && (type == SC_Halt)) {
	DEBUG('a', "Shutdown, initiated by user program.\n");
	 	interrupt->Halt();
		} else {
	printf("Unexpected user mode exception %d %d\n", which, type);
	ASSERT(FALSE);
		}*/

	switch(which){
			case SyscallException:
				switch(type){
					case SC_Halt:
						DEBUG('a', "Shutdown, initiated by user program.\n");
	 					interrupt->Halt();
	 					break;
	 				case SC_Exit:
	 					DEBUG('a', "Exit in ExceptionHandler.\n");
	 					currentThread->Finish();
	 					break;
					case SC_Print:
						printf("Value: %d\n",(int)machine->ReadRegister(4));
						break;
					case SC_Create:
            DEBUG('f', "SysCallCreateHandler\n");
            SysCallCreateHandler();
            break;
          case SC_Open:
            SysCallOpenHandler();
            break;
          case SC_Close:
            SysCallCloseHandler();
            break;
          case SC_Read:
            SysCallReadHandler();
            break;
          case SC_Write:
            SysCallWriteHandler();
            break;
          case SC_Exec:
            SysCallExecHandler();
            break;
          case SC_Join:
            SysCallJoinHandler();
            break;
          case SC_Fork:
            SysCallForkHandler();
            break;
          case SC_Yield:
            SysCallYieldHandler();
            break;
	 				default:
	 					break;
				}
				break;
			case TlbMissException:
				TlbMissExceptionHandler();
				break;

			default:
				printf("Unexpected user mode exception %d %d\n", which, type);
			ASSERT(FALSE);
		}

}
/*void SysCallCreateHandler(char * name);
OpenFileId SysCallOpenHandler(char *name);
void SysCallWriteHandler(char *buffer, int size, OpenFileId id);
int SysCallReadHandler(char *buffer, int size, OpenFileId id);
void SysCallCloseHandler(OpenFileId id);
void SysCallForkHandler(void (*func)());
void SysCallYieldHandler();   
SpaceId SysCallExecHandler(char *name);
int SysCallJoinHandler(SpaceId id);   
*/
void ReadStringFromUserAddrSpace(int startAddr, char * into){
  int ch;
  int i = 0;
  do{
    machine->ReadMem(startAddr + i, 1, &ch);
    into[i] = (char) ch;
    i += 1;
  } while (ch != 0);
}
void ReadCharsFromUserAddrSpace(int startAddr, int size, char * into){
  int ch;
  for (int i = 0; i < size; ++i){
    machine->ReadMem(startAddr + i, 1, &ch);
    into[i] = (char) ch;
  }
}
void WriteCharsIntoUserAddrSpace(int startAddr, int size, char * from){
  int ch;
  for (int i = 0; i < size; ++i){
    ch = (int)from[i];
    machine->WriteMem(startAddr + i, 1, ch);
  }
}

void SysCallCreateHandler(){
  int startAddr = (int) machine->ReadRegister(4);
  char name[FileNameMaxLen + 1];
  ReadStringFromUserAddrSpace(startAddr, name);
  if (fileSystem->Create(name, INIT_FILE_SIZE)){
    DEBUG('f', "SysCallCreateHandler: succ.\n");
  } else {
    DEBUG('f', "SysCallCreateHandler: fail.\n");
  }
}
void SysCallOpenHandler(){
  /*fileSystem->isStub;
  fileSystem->isReal;*/
  int startAddr = (int) machine->ReadRegister(4);
  char name[FileNameMaxLen + 1];
  ReadStringFromUserAddrSpace(startAddr, name);
  void * openFile = (void *) fileSystem->Open(name);
  OpenFileId fid = currentThread->addOpenFileEntry(openFile);
  machine->WriteRegister(2, fid);
}
// fid = 0, 1, 2?
void SysCallWriteHandler(){
  int bufferAddr = (int) machine->ReadRegister(4);
  int size = (int) machine->ReadRegister(5);
  OpenFileId fid = (OpenFileId) machine->ReadRegister(6);
  OpenFile *openFile = (OpenFile *) currentThread->getOpenFile(fid);
  if (openFile != NULL){
    char * content = new char[size];
    ReadCharsFromUserAddrSpace(bufferAddr, size, content);
    openFile->Write(content, size);
    delete content;
  }
}
// fid = 0, 1, 2?
void SysCallReadHandler(){
  int bufferAddr = (int) machine->ReadRegister(4);
  int size = (int) machine->ReadRegister(5);
  OpenFileId fid = (OpenFileId) machine->ReadRegister(6);
  OpenFile * openFile = (OpenFile *) currentThread->getOpenFile(fid);
  if (openFile != NULL){
    char * content = new char[size];
    int numRead = openFile->Read(content, size);
    WriteCharsIntoUserAddrSpace(bufferAddr, numRead, content);
    delete content;
    machine->WriteRegister(2, numRead);
  }
  machine->WriteRegister(2, -1);
}
void SysCallCloseHandler(){
  OpenFileId fid = (OpenFileId) machine->ReadRegister(4);
  OpenFile * openFile = (OpenFile *) currentThread->getOpenFile(fid);
  if (openFile != NULL){
    currentThread->removeOpenFileEntry(openFile);
    delete openFile;
  }
}
void SysCallForkHandler(){

}
void SysCallYieldHandler(){

}   
void SysCallExecHandler(){

}
void SysCallJoinHandler(){

}   
