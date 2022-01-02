#include <mpi.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <algorithm>
#include <fstream>
#include <iostream>

using namespace std;


#define MPIPrintf(format, ...) printf("[%i]: " format, rank, ##__VA_ARGS__); fflush(stdout)

void __MPIAssert(int rank, bool condition, const char * const cond_str, const char * const func, int line) {
    if (!condition) {
        MPIPrintf("Assert condition [ %s ] failed at (%s):%i. Aborting...\n", cond_str, func, line);
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
}

#define MPIAssert(condition) __MPIAssert(rank, (condition), #condition, __FUNCTION__, __LINE__)
#define MPIPv(var) cout << "[" << rank << "]: " << #var << " = " << var << std::flush
#define MPIPn cout << endl

#define MASTER_RANK 0
const char * const INPUT_FILE = "suman.in";
const char * const OUTPUT_FILE = "suman.out";
const int NUM_CHUNKS = 16;

enum MY_MPI_TAGS {
    MY_MPI_TAGS_MASTER_TO_SLAVE_TASK,
    MY_MPI_TAGS_MASTER_TO_SLAVE_TERMINATE,
    MY_MPI_TAGS_SLAVE_TO_MASTER_RESPONSE,
};


// algoritmul lui Euclid
int cmmdc(int a, int b) {
    if (b == 0) {
        return a;
    }

    return cmmdc(b, a % b);
}

// cel mai mare multiplu comun a 2 numere
long long cmmmc(int a, int b) {
    return (long long)(a / cmmdc(a, b)) * b;
}

struct InputInformation {
    int N, numDivisors;
    int *divisors;
};

InputInformation getInput(int rank) {
    int N, numDivisors;
    int *divisors;

    if (rank == MASTER_RANK) {
        ifstream in(INPUT_FILE);

        in >> N >> numDivisors;
        divisors = (int*)malloc(sizeof(int) * numDivisors);
        MPIAssert(divisors != NULL);

        for (int i = 0; i < numDivisors; ++i) {
            in >> divisors[i];
        }

        in.close();
    }

    MPI_Bcast(&N, 1, MPI_INT, MASTER_RANK, MPI_COMM_WORLD);
    MPI_Bcast(&numDivisors, 1, MPI_INT, MASTER_RANK, MPI_COMM_WORLD);

    if (rank != MASTER_RANK) {
        divisors = (int*)malloc(sizeof(int) * numDivisors);
        MPIAssert(divisors != NULL);
    }

    MPI_Bcast(divisors, numDivisors, MPI_INT, MASTER_RANK, MPI_COMM_WORLD);

    InputInformation input = {.N = N, .numDivisors = numDivisors, .divisors = divisors};
    return input;
}

inline long long int computeSumForRange(int rank, int chunkStart, int chunkSize, InputInformation input, int debug) {
    const int chunkEnd = chunkStart + chunkSize;
    long long int localSum = 0;

    for (int mask = max(chunkStart, 1); mask < chunkEnd; ++mask) { // iterate over subsets
        int elementCount = 0;
        long long commonMultiple = 1;
        bool tooBig = false;

        for (int b = 0; b < input.numDivisors; ++b) { // iterate over the possible elements of the current subset
            if (mask & (1 << b)) { // element is in the subset
                elementCount += 1;
                commonMultiple = cmmmc(commonMultiple, input.divisors[b]);
                if (commonMultiple > input.N) {
                    tooBig = true;
                    break;
                }
            }
        }

        if (tooBig) {
            continue;
        }

        long long subsetCardinality = input.N / commonMultiple;
        long long currSubsetSum = commonMultiple * (subsetCardinality * (subsetCardinality + 1) / 2);

        if (debug >= 2) {
            MPIPv(elementCount); MPIPn;
            MPIPv(commonMultiple); MPIPn;
            MPIPv(subsetCardinality); MPIPn;
            MPIPv(currSubsetSum); MPIPn;
        }

        if (elementCount & 1) { // odd
            localSum += currSubsetSum;
        }
        else { // even
            localSum -= currSubsetSum;
        }
    }

    return localSum;
}

void doMasterProc(int argc, char **argv, int rank, int proc_num, int debug, InputInformation input) {
    const int limit = 1<<(input.numDivisors);
    int num_chunks = NUM_CHUNKS;
    while (limit % num_chunks != 0) {
        num_chunks /= 2;
    }
    int chunkSize = limit / num_chunks;
    MPIAssert(proc_num <= num_chunks);

    MPIPv(num_chunks); MPIPn;
    MPIPv(chunkSize); MPIPn;

    MPI_Bcast(&chunkSize, 1, MPI_INT, MASTER_RANK, MPI_COMM_WORLD);

    MPI_Request req[proc_num - 1];
    MY_MPI_TAGS last_tag[proc_num - 1];
    int last_rank[proc_num - 1];
    int last_chunk_start[proc_num - 1];
    long long int last_result[proc_num - 1];
    int chunkStart = 0, req_count = 0;

    for (int r = 0; r < proc_num; ++r) {
        if (r == MASTER_RANK) {
            continue;
        }

        MPI_Isend(&chunkStart,
                  1,
                  MPI_INT,
                  r,
                  MY_MPI_TAGS_MASTER_TO_SLAVE_TASK,
                  MPI_COMM_WORLD,
                  &req[req_count]);

        last_tag[req_count] = MY_MPI_TAGS_MASTER_TO_SLAVE_TASK;
        last_rank[req_count] = r;
        last_chunk_start[req_count] = chunkStart;

        chunkStart += chunkSize;
        req_count += 1;
    }
    MPIAssert(req_count == proc_num - 1);
    int activeSlaves = req_count;

    long long int totalSum = 0;
    while (activeSlaves > 0) {
        int req_idx;
        MPI_Waitany(req_count, req, &req_idx, MPI_STATUS_IGNORE);
        MPIAssert(req_idx != MPI_UNDEFINED);

        // MPIPrintf("Request at index %i of tag %i with proc %i finished\n", req_idx, last_tag[req_idx], last_rank[req_idx]);

        if (last_tag[req_idx] == MY_MPI_TAGS_MASTER_TO_SLAVE_TASK) {
            // MPI_Isend just finished

            MPI_Irecv(&last_result[req_idx],
                      1,
                      MPI_LONG_LONG_INT,
                      last_rank[req_idx],
                      MY_MPI_TAGS_SLAVE_TO_MASTER_RESPONSE,
                      MPI_COMM_WORLD,
                      &req[req_idx]);

            last_tag[req_idx] = MY_MPI_TAGS_SLAVE_TO_MASTER_RESPONSE;
        }
        else if (last_tag[req_idx] == MY_MPI_TAGS_SLAVE_TO_MASTER_RESPONSE) {
            // MPI_Irecv just finished
            totalSum += last_result[req_idx];

            if (chunkStart < limit) {
                MPI_Isend(&chunkStart,
                          1,
                          MPI_INT,
                          last_rank[req_idx],
                          MY_MPI_TAGS_MASTER_TO_SLAVE_TASK,
                          MPI_COMM_WORLD,
                          &req[req_idx]);

                last_tag[req_idx] = MY_MPI_TAGS_MASTER_TO_SLAVE_TASK;
                last_chunk_start[req_idx] = chunkStart;
                chunkStart += chunkSize;
            }
            else {
                // there's nothing else to process;
                char dummy;
                MPI_Isend(&dummy,
                          1,
                          MPI_CHAR,
                          last_rank[req_idx],
                          MY_MPI_TAGS_MASTER_TO_SLAVE_TERMINATE,
                          MPI_COMM_WORLD,
                          &req[req_idx]);

                last_tag[req_idx] = MY_MPI_TAGS_MASTER_TO_SLAVE_TERMINATE;
            }
        }
        else {
            MPIAssert( last_tag[req_idx] == MY_MPI_TAGS_MASTER_TO_SLAVE_TERMINATE );
            MPIPrintf("Rank %i finished\n", last_rank[req_idx]);

            req[req_idx] = MPI_REQUEST_NULL;

            activeSlaves -= 1;
        }
    }

    MPIAssert(chunkStart == limit);

    MPIPrintf("Total sum = %lli\n", totalSum);
    ofstream out(OUTPUT_FILE);
    out << totalSum << '\n';
    out.close();
}

void doSlaveProc(int argc, char **argv, int rank, int proc_num, int debug, InputInformation input) {
    int chunkSize;
    MPI_Bcast(&chunkSize, 1, MPI_INT, MASTER_RANK, MPI_COMM_WORLD);
    MPIPrintf("Got chunk size(%i) in broadcast\n", chunkSize);

    while (true) {
        MPI_Status status;
        MPI_Probe(MASTER_RANK, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == MY_MPI_TAGS_MASTER_TO_SLAVE_TASK) {
            int chunkStart;
            MPI_Recv(&chunkStart,
                     1,
                     MPI_INT,
                     MASTER_RANK,
                     MY_MPI_TAGS_MASTER_TO_SLAVE_TASK,
                     MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);

            if (debug) { MPIPrintf("Got chunkStart: %i\n", chunkStart); }
            long long int localSum = computeSumForRange(rank, chunkStart, chunkSize, input, debug);
            if (debug) { MPIPrintf("Computed localSum: %lli\n\n", localSum); }

            MPI_Send(&localSum,
                     1,
                     MPI_LONG_LONG_INT,
                     MASTER_RANK,
                     MY_MPI_TAGS_SLAVE_TO_MASTER_RESPONSE,
                     MPI_COMM_WORLD);
        }
        else {
            MPIAssert(status.MPI_TAG == MY_MPI_TAGS_MASTER_TO_SLAVE_TERMINATE);
            break;
        }
    }
}


int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, proc_num;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &proc_num);
    MPIAssert(proc_num > 1);

    MPIAssert(argc == 2);
    int debug = atoi(argv[1]);

    MPIPrintf("I am rank %i out of %i running on pid %i\n", rank, proc_num, (int)getpid());

    InputInformation input = getInput(rank);
    if (rank == MASTER_RANK) {
        doMasterProc(argc, argv, rank, proc_num, debug, input);
    }
    else {
        doSlaveProc(argc, argv, rank, proc_num, debug, input);
    }

    free(input.divisors);
    MPI_Finalize();
    return 0;
}