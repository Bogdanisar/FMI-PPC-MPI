
#include <iostream>
#include <fstream>
#include <vector>

#include <gmpxx.h>

using namespace std;


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

int main() {
    ifstream in("suman.in");
    ofstream out("suman.out");

    string N_str;
    in >> N_str;
    mpz_class N(N_str);

    int numDivisors;
    in >> numDivisors;

    vector<mpz_class> divisors;
    for (int i = 0; i < numDivisors; ++i) {
        string divisor_str;
        in >> divisor_str;
        divisors.emplace_back(divisor_str);
    }

    int limit_mask = (1<<numDivisors);
    mpz_class total_suma = 0;
    for (int mask = 1; mask < limit_mask; ++mask) { // iterate over subsets

        int nr_elemente = 0;
        mpz_class multiplu_comun = 1;
        bool too_big = false;

        for (int b = 0; b < numDivisors; ++b) { // iterate over the possible elements of the current subset
            if (mask & (1 << b)) { // element is in the subset
                nr_elemente += 1;
                multiplu_comun = cmmmc(multiplu_comun, divisors[b]);
                if (multiplu_comun > N) {
                    too_big = true;
                    break;
                }
            }
        }

        if (too_big) {
            continue;
        }

        mpz_class cardinal_submultime = N / multiplu_comun;
        mpz_class suma_submultime = multiplu_comun * (cardinal_submultime * (cardinal_submultime + 1) / 2);

        if (nr_elemente & 1) {
            total_suma += suma_submultime;
        }
        else {
            total_suma -= suma_submultime;
        }
    }

    out << total_suma << '\n';
    cout << total_suma << '\n';

    return 0;
}

