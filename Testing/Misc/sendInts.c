#include <mpi.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#define amount 1000000

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_INT;

    int data[amount] = {};
    if (rank == 0) {
        for (int i = 0; i < amount; ++i) {
            data[i] = i + 100;
            // printf("[0] will send data[%i] = %i\n", i, data[i]);
        }

        MPI_Send(data, amount, MPI_INT, 1, 0, MPI_COMM_WORLD);
        printf("[%i]: After MPI_SEND!\n", rank);
    }
    else {
        // for (int i = 0; i < amount; ++i) {
        //     printf("[1] before Recv has data[%i] = %i\n", i, data[i]);
        // }

        MPI_Status status;
        sleep(10);
        int recv_ret = MPI_Recv(data, amount, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        assert(recv_ret == MPI_SUCCESS);

        printf("[1] status.MPI_SOURCE = %i\n", status.MPI_SOURCE);
        printf("[1] status.MPI_TAG = %i\n", status.MPI_TAG);
        printf("[1] status.MPI_ERROR = %i\n", status.MPI_ERROR);

        // for (int i = 0; i < amount; ++i) {
        //     printf("[1] after Recv has data[%i] = %i\n", i, data[i]);
        //     printf("[1]: Trying to sleep...\n"); sleep(3);
        // }
    }

    MPI_Finalize();
    return 0;
}