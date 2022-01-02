#include <iostream>
#include <fstream>
#include <vector>
#include <unistd.h>

#include <gmpxx.h>
#include <mpi.h>

using namespace std;


#define MPIPrintf(format, ...) printf("[%i]: " format, rank, ##__VA_ARGS__); fflush(stdout)

void __MPIAssert(bool condition, const char * const cond_str, const char * const func, int line) {
    if (!condition) {
        printf("Assert condition [ %s ] failed at (%s):%i. Aborting...\n", cond_str, func, line);
        fflush(stdout);
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
}

#define MPIAssert(condition) __MPIAssert((condition), #condition, __FUNCTION__, __LINE__)
#define MPIPv(var) cout << "[" << rank << "]: " << #var << " = " << var << std::flush
#define MPIPn cout << endl

#define MASTER_RANK 0
#define MAX_DIGITS 100
#define MAX_BIGINT_SIZE (MAX_DIGITS + 1)
const char * const INPUT_FILE = "suman.in";
const char * const OUTPUT_FILE = "suman.out";


// euclid
mpz_class cmmdc(mpz_class a, mpz_class b) {
    if (b == 0) {
        return a;
    }

    return cmmdc(b, a % b);
}

// lowest common multiple
mpz_class cmmmc(mpz_class a, mpz_class b) {
    return (a / cmmdc(a, b)) * b;
}


void broadcastIntegerMPZ(int rank, mpz_class& number) {
    unsigned bufferSize;
    if (rank == MASTER_RANK) {
        bufferSize = number.get_str().size() + 1;
    }

    MPI_Bcast(&bufferSize, 1, MPI_UNSIGNED, MASTER_RANK, MPI_COMM_WORLD);

    char *buffer = (char*)malloc(sizeof(char) * bufferSize);
    MPIAssert(buffer != NULL);

    if (rank == MASTER_RANK) {
        // we need a separate variable here so that it doesn't get deallocated immediately
        string stringRepresentation = number.get_str();

        const char * numberCString = stringRepresentation.c_str();
        strcpy(buffer, numberCString); // populate the buffer
    }

    MPI_Bcast(buffer, bufferSize, MPI_CHAR, MASTER_RANK, MPI_COMM_WORLD);

    if (rank != MASTER_RANK) {
        number = mpz_class(buffer);
    }

    free(buffer);
}

struct InputInformation {
    mpz_class N;
    int numDivisors;
    vector<mpz_class> divisors;
};

InputInformation getInput(int rank) {
    mpz_class N;
    int numDivisors;
    vector<mpz_class> divisors;

    if (rank == MASTER_RANK) {
        ifstream in(INPUT_FILE);

        string N_str;
        in >> N_str;
        N = mpz_class(N_str);

        in >> numDivisors;
        for (int i = 0; i < numDivisors; ++i) {
            string div_str;
            in >> div_str;
            divisors.emplace_back(div_str);
        }

        in.close();
    }

    broadcastIntegerMPZ(rank, N);
    MPI_Bcast(&numDivisors, 1, MPI_INT, MASTER_RANK, MPI_COMM_WORLD);

    if (rank != MASTER_RANK) {
        divisors.resize(numDivisors);
    }

    for (mpz_class& d : divisors) {
        broadcastIntegerMPZ(rank, d);
    }

    InputInformation input = {.N = N, .numDivisors = numDivisors, .divisors = divisors};
    return input;
}


mpz_class computeSumForRange(int rank, int chunkStart, int chunkSize, InputInformation input, int debug) {
    const int chunkEnd = chunkStart + chunkSize;
    mpz_class localSum = 0;

    for (int mask = max(chunkStart, 1); mask < chunkEnd; ++mask) { // iterate over subsets
        int elementCount = 0;
        mpz_class commonMultiple = 1;
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

        mpz_class subsetCardinality = input.N / commonMultiple;
        mpz_class currSubsetSum = commonMultiple * (subsetCardinality * (subsetCardinality + 1) / 2);

        // if (debug >= 2) {
        //     MPIPv(elementCount); MPIPn;
        //     MPIPv(commonMultiple); MPIPn;
        //     MPIPv(subsetCardinality); MPIPn;
        //     MPIPv(currSubsetSum); MPIPn;
        //     MPIPn;
        // }

        if (elementCount & 1) { // odd
            localSum += currSubsetSum;
        }
        else { // even
            localSum -= currSubsetSum;
        }
    }

    return localSum;
}


struct BigIntAsDigitStruct {
    char digits[MAX_BIGINT_SIZE];
};

BigIntAsDigitStruct mpzToStruct(const mpz_class& bigInt) {
     // we need this variable or the C-string gets prematurely deallocated
    string stringRepresentation = bigInt.get_str();
    MPIAssert(stringRepresentation.size() <  MAX_BIGINT_SIZE);

    BigIntAsDigitStruct ret;
    strcpy(ret.digits, stringRepresentation.c_str());

    return ret;
}

void reduceBigInt(void *invec_void, void *inoutvec_void, int *len, MPI_Datatype *datatype) {
    BigIntAsDigitStruct *invec = (BigIntAsDigitStruct *)invec_void;
    BigIntAsDigitStruct *inoutvec = (BigIntAsDigitStruct *)inoutvec_void;

    for (int i = 0; i < *len; ++i) {
        mpz_class n1(invec[i].digits);
        mpz_class n2(inoutvec[i].digits);

        mpz_class result = n1 + n2;
        inoutvec[i] = mpzToStruct(result);
    }
}


int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int rank, proc_num;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &proc_num);

    MPIAssert(argc == 2);
    int debug = atoi(argv[1]);

    MPIPrintf("I am rank %i out of %i running on pid %i\n", rank, proc_num, (int)getpid());

    InputInformation input = getInput(rank);

    int limit = 1<<input.numDivisors;
    MPIAssert(limit % proc_num == 0);
    int chunkSize = limit / proc_num;
    int procStart = rank * chunkSize;
    int procEnd = procStart + chunkSize;

    if (debug > 0) {
        MPIPv(limit); MPIPn;
        MPIPv(chunkSize); MPIPn;
        MPIPv(procStart); MPIPn;
        MPIPv(procEnd); MPIPn;
        MPIPn;
    }

    if (rank == MASTER_RANK && debug > 1) {
        MPIPv(input.N); MPIPn;
        MPIPv(input.numDivisors); MPIPn;
        for (int i = 0; i < input.numDivisors; ++i) {
            MPIPv(input.divisors[i]); MPIPn;
        }
    }


    // compute
    mpz_class localSum = computeSumForRange(rank, procStart, chunkSize, input, debug);
    BigIntAsDigitStruct localSumStruct = mpzToStruct(localSum);
    if (debug) { MPIPv(localSum); MPIPn; }

    MPI_Datatype bigNumberStructType;
    MPI_Type_contiguous(MAX_BIGINT_SIZE, MPI_CHAR, &bigNumberStructType);
    MPI_Type_commit(&bigNumberStructType);
    MPI_Op myReduceOperation;
    MPI_Op_create(reduceBigInt, 1, &myReduceOperation);

    BigIntAsDigitStruct totalSumStruct;
    MPI_Reduce(&localSumStruct, &totalSumStruct, 1, bigNumberStructType, myReduceOperation, MASTER_RANK, MPI_COMM_WORLD);


    // output
    if (rank == MASTER_RANK) {
        mpz_class totalSum(totalSumStruct.digits);
        MPIPv(totalSum); MPIPn;

        ofstream out(OUTPUT_FILE);
        out << totalSum << '\n';
        out.close();
    }


    // cleanup
    MPI_Type_free(&bigNumberStructType);
    MPI_Op_free(&myReduceOperation);
    MPI_Finalize();

    return 0;
}


