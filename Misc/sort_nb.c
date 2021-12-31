#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

int cmpIntegers(const void * a, const void * b) {
   return ( *(int*)a - *(int*)b );
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    if (argc != 3) {
        printf("Usage: %s size_array_to_sort verboseLevel(>=0)\n", argv[0]);
        MPI_Finalize();
        return -1;
    }

    int rank, procNum, elementsInArray, verbose;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &procNum);
    sscanf(argv[1], "%i", &elementsInArray);
    sscanf(argv[2], "%i", &verbose);

    if (rank == 0) {
        printf("[%i]: procNum = %i\n", rank, procNum);
        printf("[%i]: elementsInArray = %i\n", rank, elementsInArray);
        printf("[%i]: verbose = %i\n", rank, verbose);
    }

    if (elementsInArray % procNum != 0) {
        printf("The number of processes(%i) must divide the size of the array(%i) !\n",
               procNum,
               elementsInArray);
        MPI_Finalize();
        return 0;
    }
    int eachSize = elementsInArray / procNum;

    if (verbose < 0) {
        printf("The verbose argument(%i) must be >= 0!\n", verbose);
        MPI_Finalize();
        return 0;
    }

    if (rank == 0) {

        int *array = (int*)malloc(elementsInArray * sizeof(int));
        int *merge = (int*)malloc(elementsInArray * sizeof(int));
        int *check = (int*)malloc(elementsInArray * sizeof(int));
        if (array == NULL || merge == NULL || check == NULL) {
            printf("Error allocating arrays!\n");
            MPI_Finalize();
            return 0;
        }

        srand(time(NULL));
        for (int i = 0; i < elementsInArray; ++i) {
            array[i] = rand() % elementsInArray;

            if (verbose >= 1) {
                printf("[%i]: array[%i] = %i\n", rank, i, array[i]);
            }
        }

        MPI_Request send_req[procNum];
        MPI_Request recv_req[procNum];
        for (int r = 1; r < procNum; ++r) {
            int ret = MPI_Isend(&array[r * eachSize], eachSize, MPI_INT, r, 0, MPI_COMM_WORLD, &send_req[r]);
            assert(ret == MPI_SUCCESS);
        }


        memcpy(check, array, elementsInArray * sizeof(int));
        qsort(check, elementsInArray, sizeof(int), cmpIntegers);

        if (verbose >= 2) {
            for (int j = 0; j < elementsInArray; ++j) {
                printf("[%i]: check[%i] = %i\n", rank, j, check[j]);
            }
        }

        qsort(array, eachSize, sizeof(int), cmpIntegers);


        for (int cnt = 0; cnt < procNum - 1; ++cnt) {
            int r;
            int ret_wait = MPI_Waitany(procNum - 1, send_req + 1, &r, MPI_STATUS_IGNORE);
            assert(ret_wait == MPI_SUCCESS);

            r += 1;

            int ret_recv = MPI_Irecv(&array[r * eachSize], eachSize, MPI_INT, r, 0, MPI_COMM_WORLD, &recv_req[r]);
            assert(ret_recv == MPI_SUCCESS);
        }

        int ret_wait = MPI_Waitall(procNum - 1, recv_req + 1, MPI_STATUSES_IGNORE);
        assert(ret_wait == MPI_SUCCESS);
        
        if (verbose >= 2) {
            printf("[%i]: After each Recv, array is:\n", rank);
            for (int i = 0; i < elementsInArray; ++i) {
                printf("[%i]: array[%i] = %i\n", rank, i, array[i]);
            }
        }


        // do merge
        int idx[procNum];
        memset(idx, 0, sizeof(idx));

        for (int i = 0; i < elementsInArray; ++i) {
            int nextElementIdx = -1, nextElement;
            for (int r = 0; r < procNum; ++r) {
                int startIndex = r * eachSize;
                if (idx[r] < eachSize && (nextElementIdx == -1 || array[startIndex + idx[r]] < nextElement)) {
                    nextElement = array[startIndex + idx[r]];
                    nextElementIdx = r;
                }
            }

            merge[i] = nextElement;
            idx[nextElementIdx] += 1;
        }

        if (verbose >= 1) {
            printf("[%i]: Resulting array is:\n", rank);
            for (int i = 0; i < elementsInArray; ++i) {
                printf("[%i]: merge[%i] = %i\n", rank, i, merge[i]);
            }
        }

        for (int i = 0; i < elementsInArray; ++i) {
            if (merge[i] != check[i]) {
                printf("[%i]: Got bad results at index %i !!\n", rank, i);

                if (verbose >= 1) {
                    for (int j = 0; j < elementsInArray; ++j) {
                        printf("[%i]: check[%i] = %i\n", rank, j, check[j]);
                    }
                }

                MPI_Finalize();
                return 0;
            }
        }

        free(array);
        free(merge);
        free(check);
    }
    else {
        int *worker_array = (int*)malloc(eachSize * sizeof(int));
        if (worker_array == NULL) {
            printf("Error allocating arrays!\n");
            MPI_Finalize();
            return 0;
        }

        int recv_ret = MPI_Recv(worker_array, eachSize, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        assert(recv_ret == MPI_SUCCESS);
        

        if (verbose >= 2) {
            printf("[%i]: Before sorting:\n", rank);
            for (int i = 0; i < eachSize; ++i) {
                printf("[%i]: worker_array[%i] = %i\n", rank, i, worker_array[i]);
            }
        }

        qsort(worker_array, eachSize, sizeof(int), cmpIntegers);

        if (verbose >= 2) {
            printf("[%i]: After sorting:\n", rank);
            for (int i = 0; i < eachSize; ++i) {
                printf("[%i]: worker_array[%i] = %i\n", rank, i, worker_array[i]);
            }
        }


        int send_ret = MPI_Send(worker_array, eachSize, MPI_INT, 0, 0, MPI_COMM_WORLD);
        assert(send_ret == MPI_SUCCESS);

        free(worker_array);
    }

    MPI_Finalize();
    return 0;
}