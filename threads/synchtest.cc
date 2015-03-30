// synchtest.cc

#include "system.h"

//producer and consumer: using Semaphore
// a buffer with size 1
int synch_buffer = 0; //value
Semaphore* synch_bufer_useful = new Semaphore("synch_bufer_useful", 1);
Semaphore* synch_bufer_product = new Semaphore("synch_bufer_product", 0);

void consume_buffer(int arg){
    synch_bufer_product->P();
    printf("synchtest.cc: thread %d consumed value %d in buffer.\n", currentThread->getTid(), synch_buffer);
    synch_bufer_useful->V();
}
void produce_buffer(int arg){
    synch_bufer_useful->P();
    synch_buffer = arg;
    printf("synchtest.cc: thread %d produced value %d in buffer.\n", currentThread->getTid(), synch_buffer);
    synch_bufer_product->V();
}

void producer_cosumer_test_cccppp(){
    DEBUG('t', "Entering producer_cosumer_test");
    //create 3 consumer
    for (int i = 0; i < 3; i++){
        Thread* t = createThread("consumer", 4);
        if (t == NULL)
            continue;
        t->Fork(consume_buffer, 0);
    }
    //create 3 producer
    for (int i = 0; i < 3; i++){
        Thread* t = createThread("producer", 4);
        if (t == NULL)
            continue;
        t->Fork(produce_buffer, i + 100);
    }
}

