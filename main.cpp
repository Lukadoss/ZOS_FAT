#include <iostream>
#include "fat.h"

using namespace std;
bool create = false;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cout<<"------   Spusťte s parametry   ------"<<endl;
        cout<<"./program NAZEV.fat -create"<<endl;
        cout<<"\tVytvoří novy FAT soubor"<<endl;
        cout<<"./program NAZEV.fat <parametry>"<<endl<<endl;
        cout<<"\t\"-a Nazev_souboru cesta_ve_fat\" - Zkopiruje soubor do FAT"<<endl;
        cout<<"\t\t\"-a text.txt ADR1/\""<<endl<<endl;
        cout<<"\t\"-f Cesta_k_souboru_ve_fat\" - Smaze soubor z FAT"<<endl;
        cout<<"\t\t\"-f ADR1/text.txt\""<<endl<<endl;
        cout<<"\t\"-c Cesta_k_souboru_ve_fat\" - Vypise na jakych clusterech se soubor nachazi"<<endl;
        cout<<"\t\t\"-c ADR1/text.txt\""<<endl<<endl;
        cout<<"\t\"-l Cesta_k_souboru_ve_fat\" - Vypise obsah souboru"<<endl;
        cout<<"\t\t\"-l ADR1/text.txt\""<<endl<<endl;
        cout<<"\t\"-m Nazev_adresare cesta_ve_fat\" - Vytvori novy adresar"<<endl;
        cout<<"\t\t\"-m ADRESAR ADR1/\""<<endl<<endl;
        cout<<"\t\"-r Cesta_k_adresari_ve_fat\" - Smaze prazdny adresar"<<endl;
        cout<<"\t\t\"-r ADR1/ADRESAR\""<<endl<<endl;
        cout<<"\t\"-p\" - Vypise strukturu FAT"<<endl;
        cout<<"\t\"-d\" - Defragmentuje FAT"<<endl;
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
        } else if(arg == "-h" || arg == "-help"){
            cout<<"------   Spusťte s parametry   ------"<<endl;
            cout<<"./program NAZEV.fat -create"<<endl;
            cout<<"\tVytvoří novy FAT soubor"<<endl;
            cout<<"./program NAZEV.fat <parametry>"<<endl<<endl;
            cout<<"\t\"-a Nazev_souboru cesta_ve_fat\" - Zkopiruje soubor do FAT"<<endl;
            cout<<"\t\t\"-a text.txt ADR1/\""<<endl<<endl;
            cout<<"\t\"-f Cesta_k_souboru_ve_fat\" - Smaze soubor z FAT"<<endl;
            cout<<"\t\t\"-f ADR1/text.txt\""<<endl<<endl;
            cout<<"\t\"-c Cesta_k_souboru_ve_fat\" - Vypise na jakych clusterech se soubor nachazi"<<endl;
            cout<<"\t\t\"-c ADR1/text.txt\""<<endl<<endl;
            cout<<"\t\"-l Cesta_k_souboru_ve_fat\" - Vypise obsah souboru"<<endl;
            cout<<"\t\t\"-l ADR1/text.txt\""<<endl<<endl;
            cout<<"\t\"-m Nazev_adresare cesta_ve_fat\" - Vytvori novy adresar"<<endl;
            cout<<"\t\t\"-m ADRESAR ADR1/\""<<endl<<endl;
            cout<<"\t\"-r Cesta_k_adresari_ve_fat\" - Smaze prazdny adresar"<<endl;
            cout<<"\t\t\"-r ADR1/ADRESAR\""<<endl<<endl;
            cout<<"\t\"-p\" - Vypise strukturu FAT"<<endl;
            cout<<"\t\"-d\" - Defragmentuje FAT"<<endl;
            i++;
        } else {
            cout << "Bad argument: " << arg << endl;
            i++;
        }
    }
    return 0;
}