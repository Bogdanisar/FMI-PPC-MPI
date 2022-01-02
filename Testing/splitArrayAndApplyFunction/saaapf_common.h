
namespace saaapf {

    int factorial(int n) {
        int prod = 1;
        for (int i = 2; i <= n; ++i) {
            prod *= i;
        }
        return prod;
    }

    inline int getTotalSize() {
        return factorial(10) * 10;
    }

    inline int computeValue(int num) {
        int result = 1;
        for (int i = 0; i < 20; ++i) {
            result = ((long long)result * num) % (num / 3 + 1);
        }

        return result;
    }

    void processArray(int *arr, int dim) {
        for (int i = 0; i < dim; ++i) {
            arr[i] = computeValue(arr[i]);
        }
    }

}