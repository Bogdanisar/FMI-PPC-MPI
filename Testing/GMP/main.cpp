#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <gmpxx.h>
#include <mpi.h>

using namespace std;

int main() {
    mpz_class a, b("2"), c;

    a = 1234;
    b = "-5678";
    // b = string("-5678");
    c = a+b;
    cout << "sum is " << c << "\n";
    cout << "absolute value is " << abs(c) << "\n";
    cout << "absolute value is " << c.get_str() << "\n";

    return 0;
}