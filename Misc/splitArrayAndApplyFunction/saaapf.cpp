#include <mpi.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

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


void parseArgs(int argc, char **argv, int rank, int proc_num) {
//     if (argc != 2) {
//         if (rank == 0) {
//             MPIPrintf("Usage: %s sizeOfArray(must be divisible by proc_num)\n", argv[0]);
//             MPI_Abort(MPI_COMM_WORLD, -1);
//         }
//         else {
//             MPI_Finalize();
//             exit(-1);
//         }
//     }

//     sscanf(argv[1], "%i", &countOfArray);
//     MPIAssert(countOfArray > 0);
//     MPIAssert(countOfArray % proc_num == 0);
}


void doMasterProc(int argc, char **argv, int rank, int proc_num) {
    const int totalSize = saaapf::getTotalSize();
    MPIAssert(totalSize % proc_num == 0);
    MPIPrintf("totalSize = %i\n", totalSize);

    int chunkSize = totalSize / proc_num;
    MPIAssert( MPI_Bcast(&chunkSize, 1, MPI_INT, MASTER_RANK, MPI_COMM_WORLD) == MPI_SUCCESS );

    int *totalArray = (int*)malloc(totalSize * sizeof(int));
    int *localArray = (int*)malloc(chunkSize * sizeof(int));
    MPIAssert(totalArray != NULL);
    MPIAssert(localArray != NULL);

    for (int i = 0; i < totalSize; ++i) {
        totalArray[i] = i;
    }

    int scatter_ret = MPI_Scatter(totalArray, chunkSize, MPI_INT, localArray, chunkSize, MPI_INT, MASTER_RANK, MPI_COMM_WORLD);
    MPIAssert(scatter_ret == MPI_SUCCESS);

    saaapf::processArray(localArray, chunkSize);

    int gather_ret = MPI_Gather(localArray, chunkSize, MPI_INT, totalArray, chunkSize, MPI_INT, MASTER_RANK, MPI_COMM_WORLD);
    MPIAssert( gather_ret == MPI_SUCCESS );

    // MPIPrintf("First 10 elements are:\n");
    // for (int i = 0; i < 10; ++i) {
    //     MPIPrintf("totalArray[%i] = %i\n", i, totalArray[i]);
    // }

    // MPIPrintf("First 10 elements received from P1 are:\n");
    // for (int i = chunkSize; i < chunkSize + 10; ++i) {
    //     MPIPrintf("totalArray[%i] = %i\n", i, totalArray[i]);
    // }


    // To check that the implementation is ok; It's not part of the idea;
    long long sum = 0;
    for (int i = 0; i < totalSize; ++i) {
        sum = (sum + totalArray[i]) % totalSize;
    }
    MPIPrintf("Total sum = %lli\n", sum);

    free(totalArray);
    free(localArray);
}

void doSlaveProc(int argc, char **argv, int rank, int proc_num) {
    int chunkSize;
    MPIAssert( MPI_Bcast(&chunkSize, 1, MPI_INT, MASTER_RANK, MPI_COMM_WORLD) == MPI_SUCCESS );
    MPIPrintf("Got chunk size(%i) in broadcast\n", chunkSize);

    int *localArray = (int*)malloc(chunkSize * sizeof(int));
    MPIAssert(localArray != NULL);

    int scatter_ret = MPI_Scatter(NULL, 0, 0, localArray, chunkSize, MPI_INT, MASTER_RANK, MPI_COMM_WORLD);
    MPIAssert(scatter_ret == MPI_SUCCESS);

    // if (rank == 1) {
    //     MPIPrintf("First 10 elements received are:\n");
    //     for (int i = 0; i < 10; ++i) {
    //         MPIPrintf("localArray[%i] = %i\n", i, localArray[i]);
    //     }
    // }

    saaapf::processArray(localArray, chunkSize);

    int gather_ret = MPI_Gather(localArray, chunkSize, MPI_INT, NULL, 0, 0, MASTER_RANK, MPI_COMM_WORLD);
    MPIAssert( gather_ret == MPI_SUCCESS );

    free(localArray);
}


int main(int argc, char **argv) {
    MPIAssert( MPI_Init(&argc, &argv) == MPI_SUCCESS );

    int rank, proc_num;
    MPIAssert( MPI_Comm_rank(MPI_COMM_WORLD, &rank) == MPI_SUCCESS );
    MPIAssert( MPI_Comm_size(MPI_COMM_WORLD, &proc_num) == MPI_SUCCESS );

    parseArgs(argc, argv, rank, proc_num);

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