#include "syscall.h"

int main(){
    char name[4], into[4];
    OpenFileId wfid, rfid;
    name[0] = 't';
    name[1] = 'a';
    name[2] = '\0';
    Create(name);
    wfid = Open(name);
    Print(wfid);
    Write(name, 2, wfid);
    Close(wfid);
    rfid = Open(name);
    Print(rfid);
    Read(into, 2, rfid);
    Print(into[0]);
    Print(into[1]);
    Close(rfid);
    Exit(0);
}
