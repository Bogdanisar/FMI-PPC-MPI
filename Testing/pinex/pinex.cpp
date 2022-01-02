#include <iostream>
#include <fstream>
#include <unordered_map>
#include <cmath>
 
#define ll long long
using namespace std;
ifstream in("pinex.in");
ofstream out("pinex.out");
 
const int NMax = 1e6 + 5;
 
ll M,A,B,nrDivs,nrPrimes;
int primes[NMax],divs[NMax];
bool notPrime[NMax];
// primes retine in numerele prime pana la 1e6, generate prin ciur
// notPrime[i] = true daca numarul i nu este prim
// divs este un vector care contine la un moment dat divizorii primi ai lui B
// nrDivs - numarul de divizori primi ai lui B
// nrPrimes - numarul de numere prime pana la 1e6
 
int main(int argc, char *argv[]) {
    int (argc != 2) {
        printf("Usage: %s limit coprime_number\n");
        return -1;
    }
 
    // se construieste primes prin ciurul lui Eratostene
    primes[++nrPrimes] = 2;
    for (int i=3;i < NMax;i += 2) {
        if (notPrime[i]) {
            continue;
        }
 
        primes[++nrPrimes] = i;
        for (int j=3*i;j < NMax;j += 2*i) {
            notPrime[j] = true;
        }
    }
 
    in>>M;
    while (M--) {
        in>>A>>B;
        nrDivs = 0;
 
        // se stabliesc divizorii primi ai lui B
        ll p;
        for (int i=1;(p = primes[i]) != 0 && p*p <= B;++i) {
            if (B % p != 0) {
                continue;
            }
 
            while (B % p == 0) {
                B /= p;
            }
            divs[++nrDivs] = p;
        }
        if (B != 1) {
            divs[++nrDivs] = B;
        }
 
        // notPrime reprezinta numarul de numere mai mici ca A care nu sunt coprime cu B
        // numarul cerut va fi astfel A - notPrime
        ll notPrime = 0;
        ll lim = 1<<nrDivs;
        for (ll i=1;i < lim;++i) { // se parcurg submultimile de divizori ai lui B;
 
            // nr reprezinta numarul de elemente ale submultimii
            // astfel stim daca cardinalul intersectiei se ia cu + sau -
            // conform principiului includerii-excluderii;
            // prod - produsul divizorilor din submultimea curenta;
            ll nr = 0, prod = 1;
            for (ll j=1;j <= nrDivs;++j) {
                if ( (1<<(j-1)) & i ) {
                    ++nr;
                    prod *= divs[j];
                }
            }
 
            // card reprezinta cardinalul intersectiei;
            ll card = A / prod;
            if (nr % 2 == 0) {
                card = -card;
            }
 
            notPrime += card;
        }
 
        out<<A-notPrime<<'\n';
    }
 
    in.close();out.close();
    return 0;
}


