#include <mpi.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <algorithm>

#include "saaapf_common.h"


void __MPIAssert(bool condition, const char * const cond_str, const char * const func, int line) {
    if (!condition) {
        printf("Assert condition [ %s ] failed at (%s):%i. Aborting...\n", cond_str, func, line);
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
}

#define MPIAssert(condition) __MPIAssert((condition), #condition, __FUNCTION__, __LINE__)
#define MASTER_RANK 0
#define MPIPrintf(format, ...) printf("[%i]: " format, rank, ##__VA_ARGS__)

enum MY_MPI_TAGS {
    MY_MPI_TAGS_MASTER_TO_SLAVE_TASK,
    MY_MPI_TAGS_MASTER_TO_SLAVE_TERMINATE,
    MY_MPI_TAGS_SLAVE_TO_MASTER_RESPONSE,
};


void doMasterProc(int argc, char **argv, int rank, int proc_num) {
    const int totalSize = saaapf::getTotalSize();
    const int num_chunks = 20;
    MPIAssert(totalSize % num_chunks == 0);
    MPIPrintf("totalSize = %i\n", totalSize);

    int chunkSize = totalSize / num_chunks;
    MPIAssert( MPI_Bcast(&chunkSize, 1, MPI_INT, MASTER_RANK, MPI_COMM_WORLD) == MPI_SUCCESS );

    int *totalArray = (int*)malloc(totalSize * sizeof(int));
    MPIAssert(totalArray != NULL);

    for (int i = 0; i < totalSize; ++i) {
        totalArray[i] = i;
    }


    MPI_Request req[proc_num - 1];
    MY_MPI_TAGS last_tag[proc_num - 1];
    int last_rank[proc_num - 1];
    int last_chunk_start[proc_num - 1];
    int chunkStart = 0, req_count = 0;

    for (int r = 0; r < proc_num; ++r) {
        if (r == MASTER_RANK) {
            continue;
        }

        int send_ret = MPI_Isend(totalArray + chunkStart,
                                 chunkSize,
                                 MPI_INT,
                                 r,
                                 MY_MPI_TAGS_MASTER_TO_SLAVE_TASK,
                                 MPI_COMM_WORLD,
                                 &req[req_count]);
        MPIAssert( send_ret == MPI_SUCCESS );

        last_tag[req_count] = MY_MPI_TAGS_MASTER_TO_SLAVE_TASK;
        last_rank[req_count] = r;
        last_chunk_start[req_count] = chunkStart;

        chunkStart += chunkSize;
        req_count += 1;
    }
    MPIAssert(req_count == proc_num - 1);


    while (req_count > 0) {
        int req_idx;
        int wait_ret = MPI_Waitany(req_count, req, &req_idx, MPI_STATUS_IGNORE);
        MPIAssert( wait_ret == MPI_SUCCESS );

        // MPIPrintf("Request at index %i of tag %i with proc %i finished\n", req_idx, last_tag[req_idx], last_rank[req_idx]);

        if (last_tag[req_idx] == MY_MPI_TAGS_MASTER_TO_SLAVE_TASK) {
            // MPI_Isend just finished

            int recv_ret = MPI_Irecv(totalArray + last_chunk_start[req_idx],
                                     chunkSize,
                                     MPI_INT,
                                     last_rank[req_idx],
                                     MY_MPI_TAGS_SLAVE_TO_MASTER_RESPONSE,
                                     MPI_COMM_WORLD,
                                     &req[req_idx]);
            MPIAssert( recv_ret == MPI_SUCCESS );

            last_tag[req_idx] = MY_MPI_TAGS_SLAVE_TO_MASTER_RESPONSE;
        }
        else if (last_tag[req_idx] == MY_MPI_TAGS_SLAVE_TO_MASTER_RESPONSE) {
            // MPI_Irecv just finished

            if (chunkStart < totalSize) {
                int send_ret = MPI_Isend(totalArray + chunkStart,
                                         chunkSize,
                                         MPI_INT,
                                         last_rank[req_idx],
                                         MY_MPI_TAGS_MASTER_TO_SLAVE_TASK,
                                         MPI_COMM_WORLD,
                                         &req[req_idx]);
                MPIAssert( send_ret == MPI_SUCCESS );

                last_tag[req_idx] = MY_MPI_TAGS_MASTER_TO_SLAVE_TASK;
                last_chunk_start[req_idx] = chunkStart;
                chunkStart += chunkSize;
            }
            else {
                // there's nothing else to process;
                char dummy;
                int send_ret = MPI_Isend(&dummy,
                                         1,
                                         MPI_CHAR,
                                         last_rank[req_idx],
                                         MY_MPI_TAGS_MASTER_TO_SLAVE_TERMINATE,
                                         MPI_COMM_WORLD,
                                         &req[req_idx]);
                MPIAssert( send_ret == MPI_SUCCESS );

                last_tag[req_idx] = MY_MPI_TAGS_MASTER_TO_SLAVE_TERMINATE;
            }
        }
        else {
            MPIAssert( last_tag[req_idx] == MY_MPI_TAGS_MASTER_TO_SLAVE_TERMINATE );

            std::swap(req[req_idx], req[req_count - 1]);
            std::swap(last_tag[req_idx], last_tag[req_count - 1]);
            std::swap(last_rank[req_idx], last_rank[req_count - 1]);
            std::swap(last_chunk_start[req_idx], last_chunk_start[req_count - 1]);
            req_count -= 1;
        }
    }

    MPIAssert(chunkStart == totalSize);


    // To check that the implementation is ok; It's not part of the idea;
    long long sum = 0;
    for (int i = 0; i < totalSize; ++i) {
        sum = (sum + totalArray[i]) % totalSize;
    }
    MPIPrintf("Total sum = %lli\n", sum);


    free(totalArray);
}

void doSlaveProc(int argc, char **argv, int rank, int proc_num) {
    int chunkSize;
    MPIAssert( MPI_Bcast(&chunkSize, 1, MPI_INT, MASTER_RANK, MPI_COMM_WORLD) == MPI_SUCCESS );
    MPIPrintf("Got chunk size(%i) in broadcast\n", chunkSize);

    int *localArray = (int*)malloc(chunkSize * sizeof(int));
    MPIAssert(localArray != NULL);

    
    while (true) {
        MPI_Status status;
        int probe_ret = MPI_Probe(MASTER_RANK, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        MPIAssert( probe_ret == MPI_SUCCESS );

        if (status.MPI_TAG == MY_MPI_TAGS_MASTER_TO_SLAVE_TASK) {
            int recv_ret = MPI_Recv(localArray,
                                    chunkSize,
                                    MPI_INT,
                                    MASTER_RANK,
                                    MY_MPI_TAGS_MASTER_TO_SLAVE_TASK,
                                    MPI_COMM_WORLD,
                                    MPI_STATUS_IGNORE);
            MPIAssert( recv_ret == MPI_SUCCESS );

            saaapf::processArray(localArray, chunkSize);

            int send_ret = MPI_Send(localArray,
                                    chunkSize,
                                    MPI_INT,
                                    MASTER_RANK,
                                    MY_MPI_TAGS_SLAVE_TO_MASTER_RESPONSE,
                                    MPI_COMM_WORLD);
            MPIAssert( send_ret == MPI_SUCCESS );
        }
        else {
            MPIAssert( status.MPI_TAG == MY_MPI_TAGS_MASTER_TO_SLAVE_TERMINATE );
            break;
        }
    }

    free(localArray);
}


int main(int argc, char **argv) {
    MPIAssert( MPI_Init(&argc, &argv) == MPI_SUCCESS );

    int rank, proc_num;
    MPIAssert( MPI_Comm_rank(MPI_COMM_WORLD, &rank) == MPI_SUCCESS );
    MPIAssert( MPI_Comm_size(MPI_COMM_WORLD, &proc_num) == MPI_SUCCESS );

    MPIPrintf("I am rank %i out of %i\n", rank, proc_num);
    if (rank == MASTER_RANK) {
        doMasterProc(argc, argv, rank, proc_num);
    }
    else {
        doSlaveProc(argc, argv, rank, proc_num);
    }

    MPI_Finalize();
    return 0;
}