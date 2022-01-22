
#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <cassert>
#include "data_structures/lock_free_stack.cpp"

using namespace std;


#define pv(x) cout << #x << " = " << (x) << "; "; cout.flush()
#define pn cout << endl

const int DIV_MAX = 105;
const int NUM_CHUNKS = 32;
lock_free_stack<int> ConcurrentStackInput;
lock_free_stack<long long> ConcurrentStackOutput;


long long cmmdc(long long a, long long b) {
    if (b == 0) {
        return a;
    }

    return cmmdc(b, a % b);
}

// lowest common multiple
long long cmmmc(long long a, long long b) {
    return (a / cmmdc(a, b)) * (long long)b;
}


struct InputValues {
    int thread_number;
    int debug_level;

    long long N;
    int num_divisors;
    int divisors[DIV_MAX];
    int chunk_size;
};

long long computeValueForChunk(const InputValues& input, int chunk_start) {
    long long partial_sum = 0;
    int chunk_end = chunk_start + input.chunk_size;

    for (int mask = max(1, chunk_start); mask < chunk_end; ++mask) { // iterate over subsets

        long long nr_elemente = 0;
        long long multiplu_comun = 1;
        bool too_big = false;

        for (int b = 0; b < input.num_divisors; ++b) { // iterate over the possible elements of the current subset;
            if (mask & (1 << b)) { // check if the current element is in the subset
                nr_elemente += 1;
                multiplu_comun = cmmmc(multiplu_comun, input.divisors[b]);
                if (multiplu_comun > input.N) {
                    too_big = true;
                    break;
                }
            }
        }

        if (too_big) {
            continue;
        }

        long long cardinal_submultime = input.N / multiplu_comun;
        long long suma_submultime = multiplu_comun * (cardinal_submultime * (cardinal_submultime + 1) / 2);

        if (nr_elemente & 1) {
            partial_sum += suma_submultime;
        }
        else {
            partial_sum -= suma_submultime;
        }
    }

    return partial_sum;
}

void doWorkerThread(int rank, InputValues input) {
    shared_ptr<int> chunk_start;
    while (chunk_start = ConcurrentStackInput.pop()) {
        long long partial_sum = computeValueForChunk(input, *chunk_start);
        ConcurrentStackOutput.push(partial_sum);

        if (input.debug_level >= 2) {
            printf("%i: Computed answer for mask interval [%i,%i): %lli\n",
                   rank,
                   *chunk_start,
                   *chunk_start + input.chunk_size,
                   partial_sum);
        }
    }
}


int main(int argc, char *argv[]) {
    InputValues input;

    if (argc != 3) {
        printf("Usage: %s NUMBER_THREADS DEBUG_LEVEL", argv[0]);
        return -1;
    }

    input.thread_number = atoi(argv[1]);
    input.debug_level = atoi(argv[2]);


    // read input
    ifstream in("suman.in");
    ofstream out("suman.out");

    in >> input.N >> input.num_divisors;
    assert(input.num_divisors < DIV_MAX);

    for (int i = 0; i < input.num_divisors; ++i) {
        in >> input.divisors[i];
    }

    if (input.debug_level >= 1) {
        pv(input.N); pv(input.num_divisors); pn;
        for (int i = 0; i < input.num_divisors; ++i) {
            printf("divisors[%i] = %i\n", i, input.divisors[i]);
        }
    }


    // create chunks / inputs
    int limit_mask = (1<<input.num_divisors);
    int num_chunks = NUM_CHUNKS;
    while (limit_mask % num_chunks != 0) {
        num_chunks /= 2;
    }
    input.chunk_size = limit_mask / num_chunks;
    assert(input.thread_number <= num_chunks);

    if (input.debug_level >= 1) { pv(input.chunk_size); pn; }

    for (int chunk_start = 0; chunk_start < limit_mask; chunk_start += input.chunk_size) {
        ConcurrentStackInput.push(chunk_start);
    }


    // create threads
    vector<thread> worker_threads;
    for (int i = 1; i < input.thread_number; ++i) {
        worker_threads.push_back(
            thread(doWorkerThread, i, input)
        );
    }

    doWorkerThread(0, input); // main thread works as well

    for (thread& t : worker_threads) {
        t.join();
    }


    // get the answer
    long long total_sum = 0;
    shared_ptr<long long> partial_sum;
    int count = 0;
    while (partial_sum = ConcurrentStackOutput.pop()) {
        if (input.debug_level >= 1) {
            printf("0: Got partial sum from structure: %lli\n", *partial_sum);
        }

        total_sum += *partial_sum;
        count += 1;
    }

    if (input.debug_level >= 1) {
        printf("Got %i partial_sums from the worker threads\n", count);
    }

    out << total_sum << '\n';
    pv(total_sum); pn;

    return 0;
}

