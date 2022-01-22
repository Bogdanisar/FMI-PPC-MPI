#ifndef lock_free_queue_cpp
#define lock_free_queue_cpp

// This implementation is from Anthony Williams's book Concurrency in Action

#include <cstring>
#include <memory>
#include <atomic>

template<typename T>
class lock_free_queue
{
private:
    struct node;

    struct counted_node_ptr {
        long long external_count;
        node* ptr;

        counted_node_ptr() noexcept {
            external_count = 0;
            ptr = nullptr;
        }
    };

    struct node_counter {
        unsigned long long internal_count:62;
        unsigned long long external_counters:2;

        node_counter() noexcept {
            internal_count = 0;
            external_counters = 0;
        }
    };

    struct node {
        std::atomic<T*> data;
        std::atomic<node_counter> count;
        std::atomic<counted_node_ptr> next;

        node() {
            data.store(nullptr);

            node_counter new_count;
            new_count.internal_count=0;
            new_count.external_counters=2;
            count.store(new_count);

            counted_node_ptr nextInit;
            nextInit.ptr = nullptr;
            nextInit.external_count = 0;
            next.store(nextInit);
        }

        void release_ref() {
            node_counter old_counter = count.load();

            node_counter new_counter;
            do {
                new_counter=old_counter;
                --new_counter.internal_count;
            }
            while( !count.compare_exchange_strong(old_counter,new_counter) );

            if( !new_counter.internal_count && !new_counter.external_counters ) {
                delete this;
            }
        }
    };

    std::atomic<counted_node_ptr> head;
    std::atomic<counted_node_ptr> tail;


    static void increase_external_count(std::atomic<counted_node_ptr>& counter, counted_node_ptr& old_counter){
        counted_node_ptr new_counter;
        do {
            new_counter=old_counter;
            ++new_counter.external_count;
        }
        while( !counter.compare_exchange_strong(old_counter,new_counter) );

        old_counter.external_count=new_counter.external_count;
    }


    static void free_external_counter(counted_node_ptr &old_node_ptr) {
        node* const ptr=old_node_ptr.ptr;
        long long const count_increase=old_node_ptr.external_count-2;
        node_counter old_counter=ptr->count.load();

        node_counter new_counter;
        do {
            new_counter=old_counter;
            --new_counter.external_counters;
            new_counter.internal_count+=count_increase;
        }
        while( !ptr->count.compare_exchange_strong(old_counter, new_counter) );

        if(!new_counter.internal_count && !new_counter.external_counters) {
            delete ptr;
        }
    }


    void set_new_tail(counted_node_ptr &old_tail, counted_node_ptr const &new_tail) {
        node* const current_tail_ptr=old_tail.ptr;
        while(!tail.compare_exchange_weak(old_tail,new_tail) &&
              old_tail.ptr==current_tail_ptr);

        if(old_tail.ptr==current_tail_ptr)
            free_external_counter(old_tail);
        else
            current_tail_ptr->release_ref();
    }


public:
    lock_free_queue() {
        counted_node_ptr cnp;
        cnp.external_count = 0;
        cnp.ptr = new node;

        head.store(cnp);
        tail.store(cnp);
    }


    void push(T new_value) {
        std::unique_ptr<T> new_data(new T(new_value));
        counted_node_ptr new_next;
        new_next.ptr=new node;
        new_next.external_count=1;
        counted_node_ptr old_tail=tail.load();

        for(;;) {
            increase_external_count(tail,old_tail);
            T* old_data=nullptr;

            assert(old_tail.ptr != nullptr);

            if( old_tail.ptr->data.compare_exchange_strong(old_data,new_data.get()) ) {
                counted_node_ptr old_next;

                if( old_tail.ptr -> next.compare_exchange_strong(old_next,new_next) == false ) {
                    delete new_next.ptr;
                    new_next=old_next;
                }

                set_new_tail(old_tail, new_next);
                new_data.release();
                break;
            }
            else {
                counted_node_ptr old_next;

                if(old_tail.ptr->next.compare_exchange_strong(old_next,new_next)) {
                    old_next=new_next;
                    new_next.ptr=new node;
                }

                set_new_tail(old_tail, old_next);
            }
        }
    }


    std::shared_ptr<T> pop()
    {
        counted_node_ptr old_head=head.load();
        for(;;) {
            increase_external_count(head,old_head);
            node* const ptr=old_head.ptr;
            if(ptr == tail.load().ptr) {
                return std::shared_ptr<T>();
            }

            counted_node_ptr next=ptr->next.load();
            if( head.compare_exchange_strong(old_head,next) ) {
                T* const res=ptr->data.exchange(nullptr);
                free_external_counter(old_head);
                return std::shared_ptr<T>(res);
            }

            ptr->release_ref();
        }
    }
};


#endif // lock_free_queue_cpp