#include <iostream>
#include <fstream>

int main(int argc, char *argv[]) {
    int n = 1;
    if (argc > 1) n = std::atoi(argv[1]);

    std::ofstream myfile;
    myfile.open ("test_input");
    for (int i = 0; i < 1024 * n; i++) {
        char randomletter = 'A' + (random() % 26);
        myfile << randomletter;
    }
    myfile.close();
}