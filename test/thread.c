#include "syscall.h"

int val = 100;
void func(){
	Print(val);
	Exit(val);
}
int main(){
	int choice = 2;
	int try_join = 1;
	char filename[10];
	int tid;

	if (choice == 1){//Fork
		val += 1;
		Fork(func);
		val += 1;
		Print(val);
		Yield();
		val += 1;
		Exit(val);
		return 0;
	} else if (choice == 2){//Exec
		filename[0] = 'p';
		filename[1] = 'r';
		filename[2] = 'i';
		filename[3] = 'n';
		filename[4] = 't';
		filename[5] = '\0';
		//filename is 'print'
		tid = Exec(filename);
		Print(tid);
		if (try_join)
			Join(tid);
		Print(998);
		Exit(0);
	} 
}