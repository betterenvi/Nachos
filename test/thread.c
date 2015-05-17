#include "syscall.h"

void func(){
	Print(99);
	Exit(100);
}
int main(){
	Fork(func);
	Print(88);
	Exit(89);
	return 0;
}