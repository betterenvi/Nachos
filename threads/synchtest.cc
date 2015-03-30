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



//barrier

int thread_num = 3; // the 3 threads cannot run forward unless all 3 have arrived at a given stop point.
int arrived = 0;
Lock barrier_lock("barrier_lock");
Condition barrier_condition("barrier_condition");

void barrier_run(int arg){
    DEBUG('t', "Entering barrier_run");
    printf("barrier_run: thread %d started.\n", currentThread->getTid());

    barrier_lock.Acquire();
    printf("barrier_run: thread %d arrived at barrier.\n", currentThread->getTid());
    ++arrived;
    if (arrived >= thread_num){
        barrier_condition.Broadcast(&barrier_lock);
    }
    else{
        while (arrived < thread_num){
            printf("barrier_run: thread %d stopped at barrier.\n", currentThread->getTid());
            barrier_condition.Wait(&barrier_lock);
        }
        printf("barrier_run: thread %d restarted at barrier.\n", currentThread->getTid());
    }
    barrier_lock.Release();
}

void barrier_test(){
    DEBUG('t', "Entering barrier_test");
    for (int i = 0; i < 3; i++){
        Thread* t = createThread("barrier", 4);
        if (t == NULL)
            continue;
        t->Fork(barrier_run, 0);
    }
}

//  read/write lock
//
// first come, first service
// not reader first, not writer first.
bool synch_test_yield = false;
bool synch_test_yield_writer = false;
int read_cnt = 0;
int val = 10;
Lock cnt_lock("cnt_lock");
Lock write_lock("write_lock");

void lock_read(int arg){
    printf("lock_read: thread %d wanted to read value.\n", currentThread->getTid());
    cnt_lock.Acquire();
    if (read_cnt > 0){
        read_cnt++;
        printf("lock_read: thread %d read value %d, current %d readers.\n", currentThread->getTid(), val, read_cnt);
        cnt_lock.Release();
         if (synch_test_yield)
            currentThread->Yield();
   }else{
        write_lock.Acquire();
        read_cnt++;
        printf("lock_read: thread %d read value %d, current %d readers.\n", currentThread->getTid(), val, read_cnt);
        cnt_lock.Release();
        if (synch_test_yield)
            currentThread->Yield();
    }
    cnt_lock.Acquire();
    read_cnt--;
    printf("lock_read: thread %d finished reading value, remaining %d readers.\n", currentThread->getTid(), read_cnt);
    if (read_cnt == 0)
        write_lock.Release();
    cnt_lock.Release();
}

void lock_write(int arg){
    printf("lock_write: thread %d wanted to write value.\n", currentThread->getTid());
    write_lock.Acquire();
    val = arg;
    printf("lock_write: thread %d wrote value %d.\n", currentThread->getTid(), val);
    if (synch_test_yield_writer)
        currentThread->Yield();
    write_lock.Release();
    printf("lock_write: Thread %d finished writing.\n", currentThread->getTid());
}

VoidFunctionPtr readers_writers[10] = {
    lock_read, lock_read, lock_read, lock_write, lock_read, 
    lock_write, lock_read, lock_read, lock_write, lock_read
};
void read_write_lock_test(){
    DEBUG('t', "Entering read_write_lock_test");
    for (int i = 0; i < 10; ++i){
        Thread* t = createThread("rw_lock", 4);
        if (t == NULL)
            continue;
        t->Fork(readers_writers[i], 1001 + i);
    }
}

