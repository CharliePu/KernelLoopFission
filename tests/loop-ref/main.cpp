#include <iostream>
#include <vector>
#include <array>

// void scop(std::array<int,10> a, std::array<int,10> b, std::array<int,10> c) {
//     for (int i = 2; i < 10; i++) {
//         c[i] = a[i-1] + b[i-2];
//     }
// }

// void parallel(std::array<int,10> a, std::array<int,10> b, std::array<int,10> c) {
//     for (int i = 0; i < 10; i++) {
//         c[i] = 1;
//     }
// }

// void non_parallel(std::array<int,10> a, std::array<int,10> b, std::array<int,10> c) {
//     for (int i = 0; i < c[i]; i++) {
//         c[i] = a[i] + b[i];
//     }
// }

int main() {
    std::array<int,10> a{};
    std::array<int,10> b{};
    std::array<int,10> c{};

    // scop(a, b, c);
    // parallel(a, b, c);
    // non_parallel(a, b, c);

    for (int i = 0; i < 1000; i++) {
        c[i] = i;
    }

    for (int i = 0; i < 1000; i++) {
        std::cout << c[i] << " ";
    }

    return 0;
}