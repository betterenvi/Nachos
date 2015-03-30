// synchtest.cc

#include "system.h"
#include "synch.h"

//test_choice
//0: using Semaphore, cccppp
//1: using Semaphore, ppcccp
//2: using Condition variable, cccppp
//3: using Condition variable, ppcccp
int synch_test_choice = 0;


//producer and consumer: using Semaphore
// a buffer with size 1
int synch_buffer = 0; //value
Semaphore synch_buffer_useful =  Semaphore("synch_buffer_useful", 1);
Semaphore synch_buffer_product = Semaphore("synch_buffer_product", 0);

void consume_buffer(int arg){
    synch_buffer_product.P();
    printf("synchtest.cc: thread %d consumed value %d in buffer.\n", currentThread->getTid(), synch_buffer);
    synch_buffer_useful.V();
}
void produce_buffer(int arg){
    synch_buffer_useful.P();
    synch_buffer = arg;
    printf("synchtest.cc: thread %d produced value %d in buffer.\n", currentThread->getTid(), synch_buffer);
    synch_buffer_product.V();
}



//using Condition variable
bool is_empty = true;
int condition_buffer = 0;
Lock buffer_lock("buffer_lock");
Condition empty_cond_var("empty_cond_varn");
Condition full_cond_var("full_cond_var");

void consume_buffer_c(int arg){
    buffer_lock.Acquire();
    while(is_empty){
        full_cond_var.Wait(&buffer_lock);
    }
    is_empty = true;
    printf("synchtest.cc: thread %d consumed value %d in buffer.\n", currentThread->getTid(), condition_buffer);
    empty_cond_var.Signal(&buffer_lock);
    buffer_lock.Release();
}
void produce_buffer_c(int arg){
    buffer_lock.Acquire();
    while(!is_empty){
        empty_cond_var.Wait(&buffer_lock);
    }
    condition_buffer = arg;
    printf("synchtest.cc: thread %d produced value %d in buffer.\n", currentThread->getTid(), condition_buffer);
    is_empty = false;
    full_cond_var.Signal(&buffer_lock);
    buffer_lock.Release();
}

//test function

VoidFunctionPtr all_funcs[4][6] = {
    consume_buffer, consume_buffer, consume_buffer, produce_buffer, produce_buffer, produce_buffer,
    produce_buffer, produce_buffer, consume_buffer, consume_buffer, consume_buffer, produce_buffer,
    consume_buffer_c, consume_buffer_c, consume_buffer_c, produce_buffer_c, produce_buffer_c, produce_buffer_c,
    produce_buffer_c, produce_buffer_c, consume_buffer_c, consume_buffer_c, consume_buffer_c, produce_buffer_c
};
void producer_cosumer_test(){
    DEBUG('t', "Entering producer_cosumer");
    Thread* t = NULL;
    for (int i = 0; i < 6; i++){
        t = createThread("producer", 4);
        if (t == NULL)
            continue;
        t->Fork(all_funcs[synch_test_choice][i], 100 + i);
    }
}

