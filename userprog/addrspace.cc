// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"
#ifdef HOST_SPARC
#include <strings.h>
#endif

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable, int tid)
{
    NoffHeader noffH;
    unsigned int i, size;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

    // when using lazy-loading, no need to check this.
 //.   ASSERT(numPages <= NumPhysPages);		// check we're not trying
						// to run anything too big --
						// at least until we have
						// virtual memory

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numPages, size);
// first, set up the translation 
    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
	pageTable[i].virtualPage = i;	// for now, virtual page # = phys page #
    //.cqy
/*//	pageTable[i].physicalPage = machine->memBitMap->Find();
    pageTable[i].physicalPage = memBitMap->Find();
    ASSERT(pageTable[i].physicalPage != -1);*/
    pageTable[i].physicalPage = -1; // not the judge.
    //..
	pageTable[i].valid = FALSE;    // as the judge of whether this page entry is useful
	pageTable[i].use = FALSE;      //      
	pageTable[i].dirty = FALSE;
	pageTable[i].readOnly = FALSE;  // if the code segment was entirely on 
					// a separate page, we could set its 
					// pages to be read-only
    }
// zero out the entire address space, to zero the unitialized data segment 
// and the stack segment
//.    bzero(machine->mainMemory, size);
    swapFileName = new char[10];
    swapFileName = my_itoa(tid, swapFileName);
    CreateSwapFile(executable, size);
    codeAndDataLoaded = FALSE;

//    for (int i = 0; i < numPages; ++i){
//        bzero(&(machine->mainMemory[pageTable[i].physicalPage * PageSize]), PageSize);
//    }
//..


//// then, copy in the code and data segments into memory
//    if (noffH.code.size > 0) {
//        /*. DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
//            noffH.code.virtualAddr, noffH.code.size);*/
//        
//          /*  executable->ReadAt(&(machine->mainMemory[noffH.code.virtualAddr]),
//                noffH.code.size, noffH.code.inFileAddr);*/
//
//        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
//            VAddr2PAddr(noffH.code.virtualAddr), noffH.code.size);
//        for (int i = 0; i < noffH.code.size; ++i){
//            executable->ReadAt(&(machine->mainMemory[VAddr2PAddr(noffH.code.virtualAddr + i)]),
//                1, noffH.code.inFileAddr + i);
//        }//..
//    }
//    if (noffH.initData.size > 0) {
//        /*DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
//			noffH.initData.virtualAddr, noffH.initData.size);*/
//        /*executable->ReadAt(&(machine->mainMemory[noffH.initData.virtualAddr]),
//			noffH.initData.size, noffH.initData.inFileAddr);*/
//        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
//            VAddr2PAddr(noffH.initData.virtualAddr), noffH.initData.size);
//        for (int i = 0; i < noffH.initData.size; ++i){
//            executable->ReadAt(&(machine->mainMemory[VAddr2PAddr(noffH.initData.virtualAddr + i)]),
//                1, noffH.initData.inFileAddr + i);//..
//        }
//    }
    //machine->DumpState();
    //machine->DumpMem();
   // DumpPageTable();
}

//. for Fork syscall
AddrSpace::AddrSpace(int tid, void * fatherSpace_){
    AddrSpace * fatherSpace = (AddrSpace *)fatherSpace_;
    numPages = fatherSpace->numPages;
    pageTable = new TranslationEntry[numPages];                    // for now!
    for (int i = 0; i < numPages; i++) {
        pageTable[i].virtualPage = i;   
        pageTable[i].physicalPage = -1; // not the judge.
        pageTable[i].valid = FALSE;    // as the judge of whether this page entry is useful
        pageTable[i].use = FALSE;      //      
        pageTable[i].dirty = FALSE;
        pageTable[i].readOnly = FALSE; 
    }
    swapFileName = new char[10];
    swapFileName = my_itoa(tid, swapFileName);
    //   CreateSwapFile when using fork
    fileSystem->Copy(fatherSpace->swapFileName, numPages * PageSize, swapFileName);
    OpenFile * fileHandler = fileSystem->Open(swapFileName);
    for (int vpn = 0; vpn < numPages; ++vpn){
        if (fatherSpace->pageTable[vpn].dirty){
            int fatherppn = fatherSpace->pageTable[vpn].physicalPage;
            fileHandler->WriteAt(&(machine->mainMemory[fatherppn * PageSize]),
                PageSize, vpn * PageSize);
        }
    }
    delete fileHandler;
}
//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
   delete pageTable;
   //.
   machine->AcquireLock();
   fileSystem->Remove(swapFileName);
   for (int i = 0; i < numPages; ++i){
        if (pageTable[i].valid){
            memBitMap->Clear(pageTable[i].physicalPage);

            // although the statement has little influence on Nachos because of previous statement,
            //  we would love to be perfect!
            machine->pageUsageTable[pageTable[i].physicalPage].space = NULL;
        }
   }
   machine->ReleaseLock();
   delete swapFileName;
   //..
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    //DEBUG('d', "AddrSpace::RestoreState\n");
    DEBUG('v', "AddrSpace::RestoreState\n");
    //***********************//
    machine->InvalidAllEntryInTLB();    //cose me so much time!!!!!!!
                                        // must invalid all before update pageTable.
    //**********************//
    machine->pageTable = pageTable;         
    machine->pageTableSize = numPages;
    //.cqy
    //machine->DumpPageTable();
    DEBUG('v', "leave AddrSpace::RestoreState\n");
}

//only responsible for calculating PAddr.
// If addr space is not allocated at the beginning, pageTable[vpn] may be -1(NA).
int AddrSpace::VAddr2PAddr(int vAddr){
    int vpn = (unsigned) vAddr / PageSize;
    int offSet = (unsigned) vAddr % PageSize;
    return pageTable[vpn].physicalPage * PageSize + offSet;
}

void AddrSpace::DumpPageTable(){
    for (int i = 0; i < numPages; ++i){
        printf("%d\t%d\n", i, pageTable[i].physicalPage);
    }   
}

// invoked after the thread is running, but not
void AddrSpace::CreateSwapFile(OpenFile *executable, int fileSize){

    NoffHeader noffH;
    executable -> ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && (WordToHost(noffH.noffMagic) == NOFFMAGIC))
        SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

    int bufferSize = fileSize + 10;
    char buffer[bufferSize];
    bzero(buffer, bufferSize);

    if (noffH.code.size > 0){
        executable->ReadAt(buffer, noffH.code.size, noffH.code.inFileAddr);
    }
    if (noffH.initData.size > 0){
        executable->ReadAt(buffer + noffH.code.size, noffH.initData.size, noffH.initData.inFileAddr);
    }

    fileSystem->Create(swapFileName, fileSize);
    OpenFile * fileHandler = fileSystem->Open(swapFileName);
    fileHandler->WriteAt(buffer, fileSize, 0);
    delete fileHandler;
}

void AddrSpace::ForcedSwapPageToFile(int vpn){
    DEBUG('d', "Enter AddrSpace::ForcedSwapPageToFile\n");
    int ppn = pageTable[vpn].physicalPage;
    if (pageTable[vpn].dirty){
        OpenFile * fileHandler = fileSystem->Open(swapFileName);
        fileHandler->WriteAt(&(machine->mainMemory[ppn * PageSize]), PageSize, vpn * PageSize);
    //    pageTable[vpn].valid = FALSE; //if not dirty, we also need to invalidate it!!!!
    //      cost me so much time!!!!!
        pageTable[vpn].physicalPage= -1;
        delete fileHandler;
        DEBUG('d', "Dirty page %d, write back.\n", ppn);
    }else{
        DEBUG('d', "Not a dirty page %d.\n", ppn);
    }

    pageTable[vpn].valid = FALSE;

    DEBUG('d', "Leave AddrSpace::ForcedSwapPageToFile\n");
}

void AddrSpace::ForcedLoadPageToMemory(int vpn, int ppn){
    DEBUG('d', "Enter AddrSpace::ForcedLoadPageToMemory\n");
    OpenFile * fileHandler = fileSystem->Open(swapFileName);
    fileHandler->ReadAt(&(machine->mainMemory[ppn * PageSize]), PageSize, vpn * PageSize);
    pageTable[vpn].physicalPage = ppn;
    pageTable[vpn].valid = TRUE;
    pageTable[vpn].dirty = FALSE;
    pageTable[vpn].use = FALSE;
    pageTable[vpn].readOnly = FALSE;  // how to save its value.?
    delete fileHandler;
    DEBUG('d', "Leave AddrSpace::ForcedLoadPageToMemory\n");
}

// base = 10;
// val < 1000
char* AddrSpace::my_itoa(int val, char * str){
    ASSERT(val < 1000);
    int a[3];
    a[0] = val / 100;
    a[1] = (val % 100) / 10;
    a[2] = (val % 10);
    for (int i = 0; i < 3; ++i){
        str[i] = (char)(a[i] + '0');
    }
    str[3] = '\0';
    return str;
}

void AddrSpace::SwapAllPagesToFile(){
    DEBUG('d', "Enter AddrSpace::SwapAllPagesToFile\n");
    for (int i = 0; i < numPages; ++i){
        if (pageTable[i].valid){
            ForcedSwapPageToFile(i);
        }
    }
    DEBUG('d', "Leave AddrSpace::SwapAllPagesToFile\n");
}
