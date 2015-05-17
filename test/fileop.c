#include "syscall.h"

int main(){
    char name[4];
    name[0] = 't';
    name[1] = 't';
    name[2] = '\0';
    Create(name);

    Exit(0);
}
