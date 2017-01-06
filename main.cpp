#include <iostream>
#include "fat.h"

using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cout << "Not enough arguments. Program will now end" << endl;
        return 1;
    }
    fat fatka(argv[1]);
    int i = 2;
    while(i<argc) {
        string arg = argv[i];

        if (arg == "-a") {

        } else if (arg == "-f") {
            cout << arg << endl;
        } else if (arg == "-c") {
            fatka.getClusters(argv[i+1]);
            i+=2;
        } else if (arg == "-m") {
            fatka.createDirectory(argv[i+1], argv[i+2]);
            i+=3;
        } else if (arg == "-r") {
            cout << arg << endl;
        } else if (arg == "-l") {
            fatka.fileContent(argv[i+1]);
            i+=2;
        } else if (arg == "-p") {
            fatka.list();
            i++;
        } else {
            cout << "Bad argument: " << arg << endl;
            i++;
        }
    }
    return 0;
}