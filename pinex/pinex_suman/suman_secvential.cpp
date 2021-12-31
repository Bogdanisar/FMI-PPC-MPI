	
#include <iostream>
#include <fstream>
#include <vector>
 
using namespace std;
 
#define KMAX 20 + 5
int num_divs, divs[KMAX];
 
 
int cmmdc(int a, int b) {
    if (b == 0) {
        return a;
    }
 
    return cmmdc(b, a % b);
}
 
// cel mai mare multiplu comun a 2 numere
long long cmmmc(int a, int b) {
    return (long long)a * b / cmmdc(a, b);
}
 
int main() {
    ifstream in("suman.in");
    ofstream out("suman.out");
 
    int N, K;
    in >> N >> K;
 
    for (int i = 0; i < K; ++i) {
        int d;
        in >> d;
        divs[num_divs++] = d;
    }
 
    int limit_mask = (1<<K);
    long long total_suma = 0;
    for (int mask = 1; mask < limit_mask; ++mask) { // itereaza peste submultimi
 
        int nr_elemente = 0;
        long long multiplu_comun = 1;
        bool too_big = false;

        for (int b = 0; b < K; ++b) { // itereaza peste posibilele elemente din submultimea curenta
            if (mask & (1 << b)) { // verifica ca elementul curent e in submultime
                nr_elemente += 1;
                multiplu_comun = cmmmc(multiplu_comun, divs[b]);
                if (multiplu_comun > N) {
                    too_big = true;
                    break;
                }
            }
        }

        if (too_big) {
            continue;
        }
 
        long long cardinal_submultime = N / multiplu_comun;
        long long suma_submultime = multiplu_comun * (cardinal_submultime * (cardinal_submultime + 1) / 2);
        
        if (nr_elemente & 1) {
            total_suma += suma_submultime;
        }
        else {
            total_suma -= suma_submultime;
        }
 
        // printf("mask = %i\n", mask);
        // printf("submultime:\n");
        // for (int i = 0; i < num_submultime; ++i) {
        //     printf("%i ", submultime[i]);
        // }
        // printf("\n");
        // printf("cardinal_submultime = %llii\n", cardinal_submultime);
        // printf("suma_submultime = %llii\n", suma_submultime);
        // printf("total_suma = %lli\n\n", total_suma);
    }
 
    out << total_suma << '\n';
 
    return 0;
}
 
 