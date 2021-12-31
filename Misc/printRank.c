#include <mpi.h>
#include <stdio.h>

int main(int argc, char **argv) {
    for (int i = 0; i < argc; ++i) {
        printf("argv[%i] = %s\n", i, argv[i]);
    }

    MPI_Init(&argc, &argv);

    int rank, size;
    char processorName[MPI_MAX_PROCESSOR_NAME];
    int processorNameLen;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Get_processor_name(processorName, &processorNameLen);

    printf("%ith process / %i from MPI_COMM_WORLD is running on host %s!\n", 
           rank, 
           size, 
           processorName);

    MPI_Finalize();
    return 0;
}