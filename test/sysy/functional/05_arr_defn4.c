#include "sylib.h"
int main(){
    const int a[4][2] = {{1, 2}, {3, 4}, {}, 7};
    const int N = 3;
    int b[4][2] = {};
    int c[4][2] = {1, 2, 3, 4, 5, 6, 7, 8};
    int d[N + 1][2] = {1, 2, {3}, {5}, a[3][0], 8};
    int e[4][2][1] = {{d[2][1], {c[2][1]}}, {3, 4}, {5, 6}, {7, 8}};
    return e[3][1][0] + e[0][0][0] + e[0][1][0] + d[3][0];
}
