#include <iostream>
#include "fat.h"

using namespace std;
bool create = false;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cout << "Not enough arguments. Program will now end" << endl;
        return 1;
    }

    if (strcmp(argv[2], "-create")==0) create = true;

    fat fatka(argv[1], create);

    if(create){
        cout<<"FAT FILE CREATED"<<endl;
        return 0;
    }

    int i = 2;
    while(i<argc) {
        string arg = argv[i];

        if (arg == "-a") {
            fatka.findPath(arg, argv[i+1], argv[i+2]);
            i+=3;
        } else if (arg == "-f") {
            fatka.findRemoveFile(argv[i+1]);
            i+=2;
        } else if (arg == "-c") {
            fatka.getClusters(argv[i+1]);
            i+=2;
        } else if (arg == "-m") {
            fatka.findPath(arg, argv[i+1], argv[i+2]);
            i+=3;
        } else if (arg == "-r") {
            fatka.findRemoveDir(argv[i+1]);
            i+=2;
        } else if (arg == "-l") {
            fatka.fileContent(argv[i+1]);
            i+=2;
        } else if (arg == "-p") {
            fatka.list();
            i++;
        } else if (arg == "-d"){
            fatka.defragment();
            i++;
        } else {
            cout << "Bad argument: " << arg << endl;
            i++;
        }
    }
    return 0;
}