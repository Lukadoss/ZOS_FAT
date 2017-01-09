//
// Created by Lukado on 3. 1. 2017.
//

#ifndef SEMESTRALKA_FAT_H
#define SEMESTRALKA_FAT_H

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string.h>
#include <cstdint>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <vector>
#include <stdlib.h>
#include <sstream>
#include <map>

class fat {
    struct boot_record {
        char volume_descriptor[250];    //popis vygenerovaného FS
        int8_t fat_type;                //typ FAT (FAT12, FAT16...) 2 na fat_type - 1 clusterů
        int8_t fat_copies;              //počet kopií FAT tabulek
        int16_t cluster_size;           //velikost clusteru
        int32_t usable_cluster_count;   //max počet clusterů, který lze použít pro data (-konstanty)
        char signature[9];              //login autora FS
    };// 272B

    struct directory{
        char name[13];                  //jméno souboru, nebo adresáře ve tvaru 8.3'/0' 12 + 1
        bool is_file;                    //identifikace zda je soubor (TRUE), nebo adresář (FALSE)
        int32_t size;                   //velikost položky, u adresáře 0
        int32_t start_cluster;          //počáteční cluster položky
    };// 24B

    //pocitame s FAT32 MAX - tedy horni 3 hodnoty
    const static int32_t FAT_UNUSED = INT32_MAX - 1;
    const static int32_t FAT_FILE_END = INT32_MAX - 2;
    const static int32_t FAT_DIRECTORY = INT32_MAX - 3;
    int max_dir_num;
    char *FAT_FILE;

    struct boot_record *p_boot_record;
    FILE *p_file;
    fpos_t default_data_position;

    boot_record br;
    int32_t *f;
    directory root_a, root_b, root_c, root_d, root_e, root_f;

    char *escape_tabs;
    std::string ss = "";
    std::map<int, struct directory> fileMap;
    std::map<int, std::vector<struct directory>> directoryClustersMap;
    std::map<std::string, std::vector<std::string>> fileContentMap;

    void init();

    void reset();

    void implementDir(char *name);

    void removeDir(directory *dir);

    void implementFile(char *name);

    void removeFile(directory *dir);

    char *appendCharToCharArray(char* array, char a);

    char *removeCharToCharArray(char *array);

    char *nameToUpper(char *name);

    void writeDir();

    char * getDirName(int pos);

    char *getFileName(int pos);

    void getFileMap();

    void getDirectoryMap(int x);

    void writeFatStatus();


public:

    fat(char *file);

    void list();

    void getClusters(char *string);

    void fileContent(char *string);

    void findPath(std::string str, char *string, char *string1);

    void findRemoveDir(char *path);

    void findRemoveFile(char *path);

    void defragment();
};


#endif //SEMESTRALKA_FAT_H
