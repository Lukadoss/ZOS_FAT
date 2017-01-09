#include "fat.h"

using namespace std;

bool DEBUG = false;

/**
 * Defaultni nastaveni FAT souboru
 * @param file nazev fatky
 */
fat::fat(char *file) {
    this->FAT_FILE = file;

    br.fat_type = 8;
    br.fat_copies = 2;
    br.cluster_size = 256;
    br.usable_cluster_count = 251;

    memset(br.volume_descriptor, '\0', sizeof(br.volume_descriptor));
    memset(br.signature, '\0', sizeof(br.signature));
    strcpy(br.volume_descriptor, "Super drsnej FAT system yolo, musim to udelat rychle protoze UPS");
    strcpy(br.signature, "lukad");

    if (DEBUG) reset();
    init();
}

/**
 * Inicializace promennych, nacteni FAT souboru, nacteni bootovaci a FAT tabulky
 */
void fat::init() {
    p_boot_record = (struct boot_record *) malloc(sizeof(struct boot_record));

    //otevru soubor a pro jistotu skocim na zacatek
    p_file = fopen(FAT_FILE, "r+");
    fseek(p_file, 0, SEEK_SET);

    //prectu boot a fat tabulku s vsemi copies
    fread(p_boot_record, sizeof(struct boot_record), 1, p_file);
    f = (int32_t*) malloc(sizeof(int32_t)*br.usable_cluster_count*br.fat_copies);

    fread(f, sizeof(int32_t)*br.usable_cluster_count*br.fat_copies, 1, p_file);

    if (DEBUG) {
        printf("-------------------------------------------------------- \n");
        printf("BOOT RECORD \n");
        printf("-------------------------------------------------------- \n");
        printf("volume_descriptor :%s\n", p_boot_record->volume_descriptor);
        printf("fat_type :%d\n", p_boot_record->fat_type);
        printf("fat_copies :%d\n", p_boot_record->fat_copies);
        printf("cluster_size :%d\n", p_boot_record->cluster_size);
        printf("signature :%s\n\n", p_boot_record->signature);

        for (int i = 0; i < br.usable_cluster_count; i++) {
            cout << i << "  :  " << f[i] << endl;
        }
    }

    fgetpos (p_file, &default_data_position);
    max_dir_num = br.cluster_size/sizeof(directory);
}

//
//  1. Ukol - Zkopirovani souboru do FAT
//

/**
 * Vlozi soubor a obsah do FAT
 * @param name jmeno souboru
 */
void fat::implementFile(char *name) {
    FILE *newFile = fopen(name, "r");
    if (newFile==0) {
        cout << "FILE NOT EXIST!" <<endl;
        return;
    }

    vector<string> split_content = vector<string>();

    fseek(newFile, 0, SEEK_END);
    long size = ftell(newFile);
    fseek(newFile, 0, SEEK_SET);
    string content((unsigned long) size, '\0');
    fread(&content[0], (size_t) size, 1, newFile);
    fclose(newFile);

    long subStr = content.length() / br.cluster_size;

    for (int i = 0; i < subStr; i++) {
        split_content.push_back(content.substr((unsigned long) (i * br.cluster_size), (unsigned long) br.cluster_size));
    }
    if (content.length() % br.cluster_size != 0) {
        split_content.push_back(content.substr((unsigned long) (br.cluster_size * subStr)));
    }

    vector<int> freeClusters = vector<int>();
    for (int i = 0; i < br.usable_cluster_count; i++) {
        if (f[i]==FAT_UNUSED) freeClusters.push_back(i);
        if(freeClusters.size()==split_content.size()) break;
    }
    if (freeClusters.size() < split_content.size()){
        cout << "NOT ENOUGH SPACE" << endl;
        return;
    }

    directory newDir;
    name[sizeof(newDir.name)] = '\0';

    memset(newDir.name, '\0', sizeof(newDir.name));
    newDir.is_file = 1;
    strcpy(newDir.name, name);
    newDir.size = (int32_t) size;
    newDir.start_cluster = freeClusters.at(0);

    fwrite(&newDir, sizeof(newDir), 1, p_file);

    for (int i = (int) (freeClusters.size() - 1); i >= 0; i--) {
        if(i==freeClusters.size()-1) f[freeClusters.at((unsigned long) i)] = FAT_FILE_END;
        else f[freeClusters.at((unsigned long) i)] = freeClusters.at((unsigned long) (i + 1));

        char cluster[br.cluster_size];
        memset(cluster, '\0', sizeof(cluster));
        strcpy(cluster, split_content.at((unsigned long) i).c_str());

        fsetpos(p_file, &default_data_position);
        fseek(p_file, br.cluster_size*f[freeClusters.at((unsigned long) i)], SEEK_CUR);
        fwrite(&cluster, sizeof(cluster), 1, p_file);
    }


    fseek(p_file, sizeof(boot_record), SEEK_SET);
    fwrite(f, sizeof(int32_t)*br.usable_cluster_count*br.fat_copies, 1, p_file);
    cout << "OK" << endl;

    fclose(p_file);
    init();
}

//
//  2. Ukol - Smazani souboru
//

/**
 * Nalezne cestu k souboru, osetri vstupy
 * @param path cesta
 */
void fat::findRemoveFile(char *path) {
    fsetpos(p_file, &default_data_position);

    char *split = strtok(path, "/");

    if (split==NULL){
        cout << "PATH NOT FOUND" << endl;
        return;
    }

    directory *children = (directory *) malloc(sizeof(struct directory));

    for (int i = 0; i < max_dir_num; i++) {
        fread(children, sizeof(struct directory), 1, p_file);
        if (children->start_cluster!=0 && strcmp(children->name, split)==0) {
            if (children->is_file){
                removeFile(children);
                free(children);
                return;
            }
            split = strtok(NULL, "/");
            if (split==NULL) break;
            fsetpos(p_file, &default_data_position);
            fseek(p_file, br.cluster_size*children->start_cluster, SEEK_CUR);
        }
    }
    free(children);
    cout << "PATH NOT FOUND" << endl;
}

/**
 * Smaze soubor a jeho obsah z FAT
 * @param children hlavicka souboru
 */
void fat::removeFile(directory *children) {
    fpos_t deletingDirPos;
    fgetpos(p_file, &deletingDirPos);
    char cluster_empty[br.cluster_size];
    memset(cluster_empty, '\0', sizeof(cluster_empty));

    int32_t x = children->start_cluster;
    int32_t temp;

    while(x!=FAT_FILE_END){
        fsetpos(p_file, &default_data_position);
        fseek(p_file, br.cluster_size*x, SEEK_CUR);
        fwrite(&cluster_empty, (size_t) br.cluster_size, 1, p_file);
        temp = x;
        x = f[x];
        f[temp] = FAT_UNUSED;
    }

    fsetpos(p_file, &deletingDirPos);
    fseek(p_file, -sizeof(directory), SEEK_CUR);
    char buffer[] = {'\0'};
    for (int16_t j = 0; j < sizeof(directory); j++)
        fwrite(buffer, sizeof(buffer), 1, p_file);

    fseek(p_file, sizeof(boot_record), SEEK_SET);
    fwrite(f, sizeof(int32_t)*br.usable_cluster_count*br.fat_copies, 1, p_file);
    cout << "OK" << endl;

    fclose(p_file);
    init();
}

//
//  3. Ukol - Nalezeni souboru a vypis clusteru
//

/**
 * Vypisuje vsechny clustery, na kterych se vyskytuje obsah souboru
 * @param string cesta k souboru
 */
void fat::getClusters(char *string) {
    fsetpos(p_file, &default_data_position);
    directory *dir = (directory *) malloc(sizeof(struct directory));

    char *split = strtok(string, "/");

    for (int j = 0; j < max_dir_num; j++) {
        fread(dir, sizeof(struct directory), 1, p_file);
        if (dir->start_cluster!=0 && strcmp(dir->name, split)==0) {
            if (dir->is_file){
                stringstream ss;
                int32_t x = f[dir->start_cluster];
                ss << dir->start_cluster;
                while(x!=FAT_FILE_END){
                    ss << ":" << x;
                    x = f[x];
                }

                cout << split << " " << ss.str() << endl;
                free(dir);
                return;
            }
            split = strtok(NULL, "/");
            if (split==NULL) break;
            fsetpos(p_file, &default_data_position);
            fseek(p_file, br.cluster_size*dir->start_cluster, SEEK_CUR);
            j=-1;
        }
    }
    free(dir);
    cout << "PATH NOT FOUND" << endl;
}


//
//  4. Ukol - Vytvoreni noveho adresare
//

/**
 * Vytvoreni noveho adresare, zapis do FAT,
 * @param name nazev
 * @param cluster cislo clusteru
 * @param dirPos volna pozice v clusteru
 */
void fat::implementDir(char *name) {
    directory newDir;
    name[sizeof(newDir.name)] = '\0';

    memset(newDir.name, '\0', sizeof(newDir.name));
    newDir.is_file = 0;
    strcpy(newDir.name, name);
    newDir.size = 0;

    for (int i = 0; i < br.usable_cluster_count; ++i) {
        if (f[i]==FAT_UNUSED) {
            newDir.start_cluster = i;
            f[i]=FAT_DIRECTORY;
            break;
        }
    }

    fwrite(&newDir, sizeof(newDir), 1, p_file);

    fseek(p_file, sizeof(boot_record), SEEK_SET);
    fwrite(f, sizeof(int32_t)*br.usable_cluster_count*br.fat_copies, 1, p_file);
    cout << "OK" << endl;

    fclose(p_file);
    init();
}

//
//  5. Ukol - Smazani adresare
//

/**
 * Nalezne cestu adresare a zkontroluje obsah
 * @param path cesta k adresari
 */
void fat::findRemoveDir(char *path) {
    fsetpos(p_file, &default_data_position);

    fpos_t deletingDirPos;
    char *split = strtok(path, "/");

    if (split==NULL){
        cout << "PATH NOT FOUND" << endl;
        return;
    }

    directory *dir = (directory *) malloc(sizeof(struct directory));

    for (int i = 0; i < max_dir_num; i++) {
        fread(dir, sizeof(struct directory), 1, p_file);
        if (dir->start_cluster!=0 && strcmp(dir->name, split)==0) {
            if (dir->is_file){
                cout << "PATH TO THE FILE IS NOT SUPPORTED" << endl;
                free(dir);
                return;
            }
            split = strtok(NULL, "/");
            fgetpos(p_file, &deletingDirPos);
            fsetpos(p_file, &default_data_position);
            fseek(p_file, br.cluster_size*dir->start_cluster, SEEK_CUR);

            if (split==NULL) {
                directory *headers = (directory *) malloc(sizeof(directory));
                for (int j = 0; j < max_dir_num; j++) {
                    fread(headers, sizeof(directory), 1, p_file);
                    if (headers->start_cluster != 0) {
                        cout << "PATH NOT EMPTY!" << endl;
                        break;
                    }
                    if (j==max_dir_num-1){
                        fsetpos(p_file, &deletingDirPos);
                        fseek(p_file, -sizeof(directory), SEEK_CUR);
                        removeDir(dir);
                        break;
                    }
                }
                free(headers);
                free(dir);
                return;
            }
            i=-1;
        }
    }
    free(dir);
    cout << "PATH NOT FOUND" << endl;
}

/**
 * Smaze adresar, updatuje fat tabulku
 * @param dir adresar
 */
void fat::removeDir(directory *dir) {
    f[dir->start_cluster] = FAT_UNUSED;

    char buffer[] = {'\0'};
    for (int16_t i = 0; i < sizeof(directory); i++)
        fwrite(buffer, sizeof(buffer), 1, p_file);

    fseek(p_file, sizeof(boot_record), SEEK_SET);
    fwrite(f, sizeof(int32_t)*br.usable_cluster_count*br.fat_copies, 1, p_file);
    cout << "OK" << endl;

    fclose(p_file);
    init();
}

//
//  6. Ukol - Nalezeni souboru a vypis jeho obsahu
//

/**
 *  Vypisuje obsah nalezeneho souboru na zadane ceste
 * @param string cesta k souboru
 */
void fat::fileContent(char *string) {
    fsetpos(p_file, &default_data_position);
    directory *dir = (directory *) malloc(sizeof(struct directory));

    char *split = strtok(string, "/");

    for (int j = 0; j < max_dir_num; j++) {
        fread(dir, sizeof(struct directory), 1, p_file);
        if (dir->start_cluster!=0 && strcmp(dir->name, split)==0) {
            if (dir->is_file){
                char *text = (char *) malloc(sizeof(char) * br.cluster_size);
                int32_t x = dir->start_cluster;
                while(x!=FAT_FILE_END){
                    fsetpos(p_file, &default_data_position);
                    fseek(p_file, br.cluster_size*x, SEEK_CUR);
                    fread(text, (size_t) br.cluster_size, 1, p_file);
                    ss.append(text);
                    x = f[x];
                }

                cout << split << ": " << ss << endl;
                free(dir);
                free(text);
                return;
            }
            split = strtok(NULL, "/");
            if (split==NULL) break;
            fsetpos(p_file, &default_data_position);
            fseek(p_file, br.cluster_size*dir->start_cluster, SEEK_CUR);
            j=-1;
        }
    }
    free(dir);
    cout << "PATH NOT FOUND" << endl;
}

//
//   7. Ukol - Funkce pro vypsani adresarove struktury.
//

/**
 *  Zacatek vypisu z rootu definujici zakladni adresar fatky
 */
void fat::list() {
    fsetpos(p_file, &default_data_position);

    escape_tabs = (char *) "\t";
    directory *dir = (directory *) malloc(sizeof(struct directory));
    bool empty=true;

    for (int j = 0; j < max_dir_num; j++) {
        fread(dir, sizeof(struct directory), 1, p_file);
        if (dir->start_cluster!=0 && dir->is_file) {
            if (empty) cout << "+ROOT" << endl;
            empty = false;
            printf("%s-%s %d %d\n ", escape_tabs, dir->name, dir->start_cluster, (int) ceil((double)dir->size / br.cluster_size));
        }else if(dir->start_cluster!=0){
            if (empty) cout << "+ROOT" << endl;
            empty = false;
            printf("%s+%s\n", escape_tabs, nameToUpper(dir->name));
            escape_tabs = appendCharToCharArray(escape_tabs, '\t');
            fpos_t position;
            fgetpos (p_file, &position);
            fsetpos(p_file, &default_data_position);
            fseek(p_file, br.cluster_size*dir->start_cluster, SEEK_CUR);
            writeDir();
            fsetpos (p_file, &position);
        }
    }
    if (empty) {
        cout << "EMPTY" << endl;
        free(dir);
        return;
    }
    escape_tabs = removeCharToCharArray(escape_tabs);
    printf("%s--\n", escape_tabs);
//    fseek(p_file, br.cluster_size-sizeof(directory)*max_dir_num, SEEK_CUR);
    free(dir);
}

/**
 *  Rekurzivni vypis adresaru se vsemi daty v nich
 */
void fat::writeDir(){
    directory *dir = (directory *) malloc(sizeof(directory));
    for (int j = 0; j < max_dir_num; j++) {
        fread(dir, sizeof(directory), 1, p_file);

        if (dir->start_cluster!=0 && dir->is_file) {
            printf("%s-%s %d %d\n ", escape_tabs, dir->name, dir->start_cluster, (int) ceil((double)dir->size / br.cluster_size));
        }else if(dir->start_cluster!=0){
            printf("%s+%s\n", escape_tabs, nameToUpper(dir->name));
            escape_tabs = appendCharToCharArray(escape_tabs, '\t');
            fpos_t position;
            fgetpos (p_file, &position);
            fsetpos(p_file, &default_data_position);
            fseek(p_file, br.cluster_size*dir->start_cluster, SEEK_CUR);
            writeDir();
            fsetpos (p_file, &position);
        }
    }
    escape_tabs = removeCharToCharArray(escape_tabs);
    printf("%s--\n", escape_tabs);
    free(dir);
}

//
// Pomocne funkce
//

/**
 *  Automaticky reset fatky do defaultniho nastaveni. Pouzivane pro debugovani a rychle upravy.
 */
void fat::reset() {
    //directory - vytvoreni polozek
    memset(root_a.name, '\0', sizeof(root_a.name));
    root_a.is_file = 1;
    strcpy(root_a.name,"cisla.txt");
    root_a.size = 135;
    root_a.start_cluster = 2;

    memset(root_b.name, '\0', sizeof(root_b.name));
    root_b.is_file = 1;
    strcpy(root_b.name,"pohadka.txt");
    root_b.size = 5975;
    root_b.start_cluster = 1;

    memset(root_c.name, '\0', sizeof(root_c.name));
    root_c.is_file = 1;
    strcpy(root_c.name,"msg.txt");
    root_c.size = 396;
    root_c.start_cluster = 7;

    memset(root_d.name, '\0', sizeof(root_d.name));
    root_d.is_file = 0;
    strcpy(root_d.name,"adr1");
    root_d.size = 0;
    root_d.start_cluster = 29;

    memset(root_e.name, '\0', sizeof(root_e.name));
    root_e.is_file = 0;
    strcpy(root_e.name,"adr2");
    root_e.size = 0;
    root_e.start_cluster = 34;

    memset(root_f.name, '\0', sizeof(root_f.name));
    root_f.is_file = 0;
    strcpy(root_f.name,"adr3");
    root_f.size = 0;
    root_f.start_cluster = 27;

    // pro zapis budu potrebovat i prazdny cluster
    char cluster_empty[br.cluster_size];
    ////////////////////////////////////////////// SOUBORY A ADRESARE POKUSNE
    char cluster_dir1[br.cluster_size];
    char cluster_dir2[br.cluster_size];
    char cluster_dir3[br.cluster_size];
    char cluster_a[br.cluster_size];
    char cluster_b1[br.cluster_size];
    char cluster_b2[br.cluster_size];
    char cluster_b3[br.cluster_size];
    char cluster_b4[br.cluster_size];
    char cluster_b5[br.cluster_size];
    char cluster_b6[br.cluster_size];
    char cluster_b7[br.cluster_size];
    char cluster_b8[br.cluster_size];
    char cluster_b9[br.cluster_size];
    char cluster_b10[br.cluster_size];
    char cluster_b11[br.cluster_size];
    char cluster_b12[br.cluster_size];
    char cluster_b13[br.cluster_size];
    char cluster_b14[br.cluster_size];
    char cluster_b15[br.cluster_size];
    char cluster_b16[br.cluster_size];
    char cluster_b17[br.cluster_size];
    char cluster_b18[br.cluster_size];
    char cluster_b19[br.cluster_size];
    char cluster_b20[br.cluster_size];
    char cluster_b21[br.cluster_size];
    char cluster_b22[br.cluster_size];
    char cluster_b23[br.cluster_size];
    char cluster_b24[br.cluster_size];
    char cluster_c1[br.cluster_size];
    char cluster_c2[br.cluster_size];

    //pripravim si obsah - delka stringu musi byt stejna jako velikost clusteru
    memset(cluster_empty, '\0', sizeof(cluster_empty));
    memset(cluster_a, '\0', sizeof(cluster_a));
    memset(cluster_b1, '\0', sizeof(cluster_b1));
    memset(cluster_b2, '\0', sizeof(cluster_b2));
    memset(cluster_b3, '\0', sizeof(cluster_b3));
    memset(cluster_b4, '\0', sizeof(cluster_b4));
    memset(cluster_b5, '\0', sizeof(cluster_b5));
    memset(cluster_b6, '\0', sizeof(cluster_b6));
    memset(cluster_b7, '\0', sizeof(cluster_b7));
    memset(cluster_b8, '\0', sizeof(cluster_b8));
    memset(cluster_b9, '\0', sizeof(cluster_b9));
    memset(cluster_b10, '\0', sizeof(cluster_b10));
    memset(cluster_b11, '\0', sizeof(cluster_b11));
    memset(cluster_b12, '\0', sizeof(cluster_b12));
    memset(cluster_b13, '\0', sizeof(cluster_b13));
    memset(cluster_b14, '\0', sizeof(cluster_b14));
    memset(cluster_b15, '\0', sizeof(cluster_b15));
    memset(cluster_b16, '\0', sizeof(cluster_b16));
    memset(cluster_b17, '\0', sizeof(cluster_b17));
    memset(cluster_b18, '\0', sizeof(cluster_b18));
    memset(cluster_b19, '\0', sizeof(cluster_b19));
    memset(cluster_b20, '\0', sizeof(cluster_b20));
    memset(cluster_b21, '\0', sizeof(cluster_b21));
    memset(cluster_b22, '\0', sizeof(cluster_b22));
    memset(cluster_b23, '\0', sizeof(cluster_b23));
    memset(cluster_b24, '\0', sizeof(cluster_b24));
    memset(cluster_c1, '\0', sizeof(cluster_c1));
    memset(cluster_c2, '\0', sizeof(cluster_c2));
    memset(cluster_dir1, '\0', sizeof(cluster_dir1));
    memset(cluster_dir2, '\0', sizeof(cluster_dir2));
    memset(cluster_dir3, '\0', sizeof(cluster_dir3));

    strcpy(cluster_a,
           "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789 - tohle je malicky soubor pro test");
    strcpy(cluster_b1,
           "Byla jednou jedna sladka divenka, kterou musel milovat kazdy, jen ji uvidel, ale nejvice ji milovala jeji babicka, ktera by ji snesla i modre z nebe. Jednou ji darovala cepecek karkulku z cerveneho sametu a ten se vnucce tak libil, ze nic jineho nechtela ");
    strcpy(cluster_b2,
           "nosit, a tak ji zacali rikat Cervena Karkulka. Jednou matka Cervene Karkulce rekla: ?Podivej, Karkulko, tady mas kousek kolace a lahev vina, zanes to babicce, je nemocna a zeslabla, timhle se posilni. Vydej se na cestu drive nez bude horko, jdi hezky spor");
    strcpy(cluster_b3,
           "adane a neodbihej z cesty, kdyz upadnes, lahev rozbijes a babicka nebude mit nic. A jak vejdes do svetnice, nezapome?babicce poprat dobreho dne a ne abys smejdila po vsech koutech.? ?Ano, maminko, udelam, jak si prejete.? rekla Cerveni Karkulka, na stvrz");
    strcpy(cluster_b4,
           "eni toho slibu podala matce ruku a vydala se na cestu. Babicka bydlela v lese; celou p?lhodinu cesty od vesnice. Kdyz sla Cervena Karkulka lesem, potkala vlka. Tenkrat jeste nevedela, co je to za zaludne zvire a ani trochu se ho nebala. ?Dobry den, Cerven");
    strcpy(cluster_b5,
           "a Karkulko!? rekl vlk. ?Dekuji za prani, vlku.? ?Kampak tak casne, Cervena Karkulko?? ?K babicce!? ?A copak to neses v zasterce?? ?Kolac a vino; vcera jsme pekli, nemocne a zeslable babicce na posilnenou.? ?Kdepak bydli babicka, Cervena Karkulko?? ?Inu, j");
    strcpy(cluster_b6,
           "este tak ctvrthodiny cesty v lese, jeji chaloupka stoji mezi tremi velkymi duby, kolem je liskove oresi, urcite to tam musis znat.? odvetila Cervena Karkulka. Vlk si pomyslil: ?Tohle mla?ucke, jem?ucke masicko bude jiste chutnat lepe nez ta starena, mus");
    strcpy(cluster_b7,
           "im to navleci lstive, abych schlamstnul obe.? Chvili sel vedle Cervene Karkulky a pak pravil: ?Cervena Karkulko, koukej na ty krasne kvetiny, ktere tu rostou vsude kolem, procpak se trochu nerozhlednes? Myslim, ze jsi jeste neslysela ptacky, kteri by zpiv");
    strcpy(cluster_b8,
           "ali tak libezne. Ty jsi tu vykracujes, jako kdybys sla do skoly a pritom je tu v lese tak krasne!? Cervena Karkulka otevrela oci dokoran a kdyz videla, jak slunecni paprsky tancuji skrze stromy sem a tam a vsude roste tolik krasnych kvetin, pomyslila si: ");
    strcpy(cluster_b9,
           "?Kdyz prinesu babicce kytici cerstvych kvetin, bude mit jiste radost, casu mam dost, prijdu akorat.? A sebehla z cesty do lesa a trhala kvetiny. A kdyz jednu utrhla, zjistila, ze o kus dal roste jeste krasnejsi, bezela k ni, a tak se dostavala stale hloub");
    strcpy(cluster_b10,
           "eji do lesa. Ale vlk bezel rovnou k babiccine chaloupce a zaklepal na dvere. ?Kdo je tam?? ?Cervena Karkulka, co nese kolac a vino, otevri!? ?Jen zmackni kliku,? zavolala babicka: ?jsem prilis slaba a nemohu vstat.? Vlk vzal za kliku, otevrel dvere a beze");
    strcpy(cluster_b11,
           "slova sel rovnou k babicce a spolknul ji. Pak si obleknul jeji saty a nasadil jeji cepec, polozil se do postele a zatahnul zaves. Zatim Cervena Karkulka behala mezi kvetinami, a kdyz jich mela naruc tak plnou, ze jich vic nemohla pobrat, tu ji prisla na  ");
    strcpy(cluster_b12,
           "mysl babicka, a tak se vydala na cestu za ni. Podivila se, ze jsou dvere otevrene, a kdyz vesla do svetnice, prislo ji vse takove podivne, ze si pomyslila: ?Dobrotivy Boze, je mi dneska nejak ?zko a jindy jsem u babicky tak rada.? Zvolala: ?Dobre jitro!? ");
    strcpy(cluster_b13,
           "Ale nedostala zadnou odpove? ?la tedy k posteli a odtahla zaves; lezela tam babicka a mela cepec narazeny hluboko do obliceje a vypadala nejak podivne. Ach, babicko, proc mas tak velke usi?? ?Abych te lepe slysela.? ?Ach, babicko, proc mas tak velke oci ");
    strcpy(cluster_b14,
           "?? ?Abych te lepe videla.? ?Ach, babicko, proc mas tak velke ruce?? ?Abych te lepe objala.? ?Ach, babicko, proc mas tak straslivou tlamu?? ?Abych te lepe sezrala!!? Sotva vlk ta slova vyrknul, vyskocil z postele a ubohou Cervenou Karkulku spolknul. Kdyz t");
    strcpy(cluster_b15,
           "e?uhasil svoji zadostivost, polozil se zpatky do postele a usnul a z toho spanku se jal mocne chrapat. Zrovna sel kolem chaloupky lovec a pomyslil si: ?Ta starenka ale chrape, musim se na ni podivat, zda neco nepotrebuje.? Vesel do svetnice, a kdyz prist");
    strcpy(cluster_b16,
           "oupil k posteli, uvidel, ze v ni lezi vlk. ?Tak prece jsem te nasel, ty stary hrisniku!? zvolal lovec: ?Uz mam na tebe dlouho policeno!? Strhnul z ramene pusku, ale pak mu prislo na mysl, ze vlk mohl sezrat babicku a mohl by ji jeste zachranit. Nestrelil ");
    strcpy(cluster_b17,
           "tedy, nybrz vzal n?zky a zacal spicimu vlkovi parat bricho. Sotva ucinil par rez?, uvidel se cervenat karkulku a po par dalsich rezech vyskocila divenka ven a volala: ?Ach, ja jsem se tolik bala, ve vlkovi je cernocerna tma.? A potom vylez la ven i ziva b");
    strcpy(cluster_b18,
           "abicka; sotva dechu popadala. Cervena Karkulka pak nanosila obrovske kameny, kterymi vlkovo bricho naplnili, a kdyz se ten probudil a chtel uteci, kameny ho tak desive tizily, ze klesnul k zemi nadobro mrtvy. Ti tri byli spokojeni. Lovec stahnul vlkovi ko");
    strcpy(cluster_b19,
           "zesinu a odnesl si ji dom?, babicka snedla kolac a vypila vino, ktere Cervena Karkulka prinesla, a opet se zotavila. A Cervena Karkulka? Ta si svatosvate prisahala: ?Uz nikdy v zivote nesejdu z cesty do lesa, kdyz mi to maminka zakaze!? O Cervene  Karkulc");
    strcpy(cluster_b20,
           "e se jeste vypravi, ze kdyz sla jednou zase k babicce s babovkou, potkala jineho vlka a ten se ji taky vemlouval a snazil se ji svest z cesty. Ale  ona se toho vystrihala a kracela rovnou k babicce, kde hned vypovedela, ze potkala vlka, ktery ji sice popr");
    strcpy(cluster_b21,
           "al dobry den, ale z oci mu koukala nekalota. ?Kdyby to nebylo na verejne ceste, jiste by mne sezral!? ?Poj??  rekla babicka: ?zavreme dobre dvere, aby nemohl dovnitr.? Brzy nato zaklepal vlk a zavolal: ?Otevri, babicko, ja jsem Cervena Karkulka a nesu ti");
    strcpy(cluster_b22,
           "pecivo!? Ty dve vsak z?staly jako peny a neotevrely. Tak se ten sedivak plizil kolem domu a naslouchal, pak vylezl na strechu, aby tam pockal, az Cervena Karkulka p?jde vecer dom?, pak ji v temnote popadne a sezere. Ale babicka zle vlkovy ?mysly odhalila ");
    strcpy(cluster_b23,
           ". Pred domem staly obrovske kamenne necky, tak Cervene  Karkulce rekla: ?Vezmi vedro, devenko, vcera jsem varila klobasy, tak tu vodu nanosime venku do necek.? Kdyz byly necky plne, stoupala v?ne klobas nahoru az k vlkovu cenichu. Zavetril a natahoval krk");
    strcpy(cluster_b24,
           "tak daleko, ze se na strese vice neudrzel a zacal klouzat dol?, kde spadnul primo do necek a bidne se utopil.");
    strcpy(cluster_c1,
           "Prodej aktiv SABMilleru v Ceske republice, Polsku, Ma?rsku, Rumunsku a na Slovensku je soucasti podminek pro prevzeti podniku ze strany americkeho pivovaru Anheuser-Busch InBev, ktere bylo dokonceno v rijnu. Krome Plze?keho Prazdroje zahrnuji prodavana ");
    strcpy(cluster_c2,
           "aktiva polske znacky Tyskie a Lech, slovensky Topvar, ma?rsky Dreher a rumunsky Ursus. - Tento soubor je sice kratky, ale neni fragmentovany");

/////////// ZACATEK VYTVARENI FAT TABULKY

    int32_t fat[br.usable_cluster_count];

    //ve fatce na 0 clusteru bude root directory
    fat[0] = FAT_DIRECTORY;
    //pak bude soubor "cisla.txt" - ten je jen jednoclusterovy
    fat[1] = 3;
    //pak bude dlouhy soubor "pohadka.txt", ktery je cely za sebou
    fat[2] = FAT_FILE_END;
    fat[3] = 4;
    fat[4] = 5;
    fat[5] = 6;
    fat[6] = 8;
    fat[7] = 9;
    fat[8] = 10;
    fat[9] = FAT_FILE_END;
    fat[10] = 11;
    fat[11] = 12;
    fat[12] = 13;
    fat[13] = 14;
    fat[14] = 15;
    fat[15] = 16;
    fat[16] = 17;
    fat[17] = 18;
    fat[18] = 19;
    fat[19] = 20;
    fat[20] = 21;
    fat[21] = 22;
    fat[22] = 23;
    fat[23] = 24;
    fat[24] = 25;
    fat[25] = 30;
    //ted bude nejake volne misto a adr
    fat[26] = FAT_UNUSED;
    fat[27] = FAT_DIRECTORY;
    fat[28] = FAT_UNUSED;
    //pak adresar "adr1"
    fat[29] = FAT_DIRECTORY;
    //pak soubor "pohadka.txt"
    fat[30] = 33;
    fat[31] = FAT_UNUSED;
    fat[32] = FAT_UNUSED;
    //a tady je konec "pohadky.txt"
    fat[33] = FAT_FILE_END;
    fat[34] = FAT_DIRECTORY;
    //zbytek bude prazdny
    for (int32_t i = 35; i < br.usable_cluster_count; i++)
    {
        fat[i] = FAT_UNUSED;
    }

/////////// KONEC VYTVARENI FAT TABULKY

    FILE *fp = fopen(FAT_FILE, "w");
    //boot record
    fwrite(&br, sizeof(br), 1, fp);
    // 2x FAT
    fwrite(&fat, sizeof(fat), 1, fp);
    fwrite(&fat, sizeof(fat), 1, fp);
    //dir - bacha, tady je potreba zapsat CELY CLUSTER, ne jen prvnich n-BYTU plozek - tedy doplnit nulami poradi zaznamu v directory NEMUSI odpovidat poradi ve FATce a na disku
    int16_t cl_size = br.cluster_size;
    int16_t ac_size = 0;
    fwrite(&root_d, sizeof(root_d), 1, fp);
    ac_size += sizeof(root_d);

    char buffer[] = {'\0'};
    for (int16_t i = 0; i < (cl_size - ac_size); i++)
        fwrite(buffer, sizeof(buffer), 1, fp);
    ac_size = 0;
    //data - soubor pohadka.txt
    fwrite(&cluster_b1, sizeof(cluster_b1), 1, fp);
    //data - soubor cisla.txt
    fwrite(&cluster_a, sizeof(cluster_a), 1, fp);
    fwrite(&cluster_b2, sizeof(cluster_b2), 1, fp);
    fwrite(&cluster_b3, sizeof(cluster_b3), 1, fp);
    fwrite(&cluster_b4, sizeof(cluster_b4), 1, fp);
    fwrite(&cluster_b5, sizeof(cluster_b5), 1, fp);

    fwrite(&cluster_c1, sizeof(cluster_c1), 1, fp);

    fwrite(&cluster_b6, sizeof(cluster_b6), 1, fp);

    fwrite(&cluster_c2, sizeof(cluster_c2), 1, fp);

    fwrite(&cluster_b7, sizeof(cluster_b7), 1, fp);
    fwrite(&cluster_b8, sizeof(cluster_b8), 1, fp);
    fwrite(&cluster_b9, sizeof(cluster_b9), 1, fp);
    fwrite(&cluster_b10, sizeof(cluster_b10), 1, fp);
    fwrite(&cluster_b11, sizeof(cluster_b11), 1, fp);
    fwrite(&cluster_b12, sizeof(cluster_b12), 1, fp);
    fwrite(&cluster_b13, sizeof(cluster_b13), 1, fp);
    fwrite(&cluster_b14, sizeof(cluster_b14), 1, fp);
    fwrite(&cluster_b15, sizeof(cluster_b15), 1, fp);
    fwrite(&cluster_b16, sizeof(cluster_b16), 1, fp);
    fwrite(&cluster_b17, sizeof(cluster_b17), 1, fp);
    fwrite(&cluster_b18, sizeof(cluster_b18), 1, fp);
    fwrite(&cluster_b19, sizeof(cluster_b19), 1, fp);
    fwrite(&cluster_b20, sizeof(cluster_b20), 1, fp);
    fwrite(&cluster_b21, sizeof(cluster_b21), 1, fp);
    fwrite(&cluster_b22, sizeof(cluster_b22), 1, fp);

    //2x volne misto a adresar 3
    fwrite(&cluster_empty, sizeof(cluster_empty), 1, fp);
    fwrite(&root_a, sizeof(root_a), 1, fp);
    ac_size += sizeof(root_a);
    for (int16_t i = 0; i < (cl_size - ac_size); i++)
        fwrite(buffer, sizeof(buffer), 1, fp);
    ac_size =0;
    fwrite(&cluster_empty, sizeof(cluster_empty), 1, fp);
    //neprazdny adresar
    fwrite(&root_c, sizeof(root_c), 1, fp);
    ac_size += sizeof(root_c);
    fwrite(&root_e, sizeof(root_e), 1, fp);
    ac_size += sizeof(root_e);
    for (int16_t i = 0; i < (cl_size - ac_size); i++)
        fwrite(buffer, sizeof(buffer), 1, fp);
    ac_size =0;
    //prvni cast msg.txt
    fwrite(&cluster_b23, sizeof(cluster_b23), 1, fp);
    //sem je jedno co zapisu, sou to vadne sektory - tedy realne byto meli byt stringy FFFFFFcosi cosi cosiFFFFFF
    fwrite(&cluster_empty, sizeof(cluster_empty), 1, fp);
    fwrite(&cluster_empty, sizeof(cluster_empty), 1, fp);
    //druha cast msg.txt
    fwrite(&cluster_b24, sizeof(cluster_b24), 1, fp);
    //dalsi neprazdny adresar
    fwrite(&root_b, sizeof(root_b), 1, fp);
    ac_size += sizeof(root_b);
    fwrite(&root_f, sizeof(root_f), 1, fp);
    ac_size += sizeof(root_f);
    for (int16_t i = 0; i < (cl_size - ac_size); i++)
        fwrite(buffer, sizeof(buffer), 1, fp);
    //zbytek disku budou 0
    for (long i=35; i < br.usable_cluster_count; i++)
    {
        fwrite(&cluster_empty, sizeof(cluster_empty), 1, fp);
    }
    fclose(fp);
}

/**
 * Vyhleda cestu do adresare. Zkontroluje zda-li je v adresari volne misto, nastavi ukazatel na toto volne misto.
 *
 * @param str volana funkce
 * @param name nazev souboru/adresare
 * @param path cesta
 */
void fat::findPath(std::string str, char *name, char *path) {
    fsetpos(p_file, &default_data_position);

    directory *dir = (directory *) malloc(sizeof(struct directory));

    char *split = strtok(path, "/");
    if (split==NULL){
        directory *headers = (directory *) malloc(sizeof(directory));
        for (int j = 0; j < max_dir_num; j++) {
            fread(headers, sizeof(directory), 1, p_file);
            if (headers->start_cluster == 0) {
                fseek(p_file, (long) (-sizeof(directory)), SEEK_CUR);

                if (str == "-a") implementFile(name);
                else if (str == "-m") implementDir(name);

                break;
            }
            if (j==max_dir_num-1){
                cout << "NOT ENOUGH SPACE IN THIS DIRECTORY!" << endl;
                break;
            }
        }
        free(headers);
        free(dir);
        return;
    }

    for (int i = 0; i < max_dir_num; i++) {
        fread(dir, sizeof(struct directory), 1, p_file);
        if (dir->start_cluster!=0 && strcmp(dir->name, split)==0) {
            if (dir->is_file){
                cout << "PATH TO THE FILE IS NOT SUPPORTED" << endl;
                free(dir);
                return;
            }
            split = strtok(NULL, "/");
            fsetpos(p_file, &default_data_position);
            fseek(p_file, br.cluster_size*dir->start_cluster, SEEK_CUR);

            if (split==NULL) {
                directory *headers = (directory *) malloc(sizeof(directory));
                for (int j = 0; j < max_dir_num; j++) {
                    fread(headers, sizeof(directory), 1, p_file);
                    if (headers->start_cluster == 0) {
                        fseek(p_file, (long) (-sizeof(directory)), SEEK_CUR);

                        if (str == "-a") implementFile(name);
                        else if (str == "-m") implementDir(name);

                        break;
                    }
                    if (j==max_dir_num-1){
                        cout << "NOT ENOUGH SPACE IN THIS DIRECTORY!" << endl;
                        break;
                    }
                }
                free(headers);
                free(dir);
                return;
            }
            i=-1;
        }
    }
    free(dir);
    cout << "PATH NOT FOUND" << endl;
}

/**
 *  Pridava na konec pole novy znak. Uzivane pro odradkovavani pomoci /t
 * @param array pole znaku
 * @param a pridavany znak
 * @return nove pole
 */
char* fat::appendCharToCharArray(char* array, char a) {
    size_t len = strlen(array);

    char* ret = new char[len+1];

    strcpy(ret, array);
    ret[len] = a;
    ret[len+1] = '\0';

    return ret;
}

/**
 * Odstranuje z pole koncovy znak.
 * @param array pole
 * @return nove pole
 */
char* fat::removeCharToCharArray(char* array) {
    size_t len = strlen(array);
    array[len-1] = '\0';
    return array;
}

/**
 * Prevadi jmena na velka pismena
 * @param name jmeno
 * @return uppercase jmeno
 */
char* fat::nameToUpper(char *name){
    char *ret = new char[strlen(name)];
    for (int i=0; i<strlen(name); ++i) ret[i] = (char) toupper(name[i]);
    ret[strlen(name)] = '\0';
    return ret;
}

/**
 * Funkce provadejici defragmentaci celeho FAT souboru
 */
void fat::defragment(){
    writeFatStatus();

    fsetpos(p_file, &default_data_position);
    getFileMap();
    fsetpos(p_file, &default_data_position);
    getDirectoryMap(0);
    fsetpos(p_file, &default_data_position);

    int x = (int) directoryClustersMap.size();

    for (std::map<int, struct directory>::iterator it=fileMap.begin(); it!=fileMap.end(); ++it) {
//        cout<<it->first<< " => " << it->second.name << endl;
        int iter = it->second.size/br.cluster_size;
        it->second.start_cluster = x;
        for (int i = 0; i <= iter; ++i) {
            if (i!=iter){
                f[x] = x+1;
            }else f[x] = FAT_FILE_END;
            char cluster[br.cluster_size];
            memset(cluster, '\0', sizeof(cluster));
            strcpy(cluster, fileContentMap.at(it->second.name).at((unsigned long) i).c_str());

            fsetpos(p_file, &default_data_position);
            fseek(p_file, br.cluster_size*x, SEEK_CUR);
            fwrite(&cluster, sizeof(cluster), 1, p_file);
            x++;
        }
    }
    x=1;
    fsetpos(p_file, &default_data_position);

    for (std::map<int, vector<directory>>::iterator it=directoryClustersMap.begin(); it!=directoryClustersMap.end(); ++it) {
        int16_t ac_size = 0;
        int16_t cl_size = br.cluster_size;
        char buffer[] = {'\0'};

        for (unsigned int i = 0; i < it->second.size(); ++i) {
            if (it->second.at(i).start_cluster!=0 && !it->second.at(i).is_file){
//                cout<<it->first<< " => " << it->second.at(i).name << endl;
                f[x] = FAT_DIRECTORY;
                it->second.at(i).start_cluster = x;
                x++;
            } else if(it->second.at(i).start_cluster!=0)
                it->second.at(i).start_cluster = fileMap.at(it->second.at(i).start_cluster).start_cluster;
            fwrite(&it->second.at(i), sizeof(it->second.at(i)), 1, p_file);
            ac_size += sizeof(directory);
        }
        for (int16_t i = 0; i < (cl_size - ac_size); i++)
            fwrite(buffer, sizeof(buffer), 1, p_file);
    }

    fseek(p_file, sizeof(boot_record), SEEK_SET);
    fwrite(f, sizeof(int32_t)*br.usable_cluster_count*br.fat_copies, 1, p_file);
    cout << "DEFRAGMENTATION SUCCESSFUL!" << endl << endl;

    writeFatStatus();

}

char *fat::getDirName(int pos){
    directory *dir = (directory *) malloc(sizeof(directory));
    for (int j = 0; j < max_dir_num; j++) {
        fread(dir, sizeof(directory), 1, p_file);

        if (dir->start_cluster!=0 && !dir->is_file) {
            if (dir->start_cluster==pos) return dir->name;
            else {
                fpos_t position;
                fgetpos(p_file, &position);
                fsetpos(p_file, &default_data_position);
                fseek(p_file, br.cluster_size * dir->start_cluster, SEEK_CUR);
                char *result = getDirName(pos);
                if (strcmp(result, "DIRECTORY_NOT_FOUND")==0) fsetpos(p_file, &position);
                else return result;
            }
        }
    }
    free(dir);
    return (char *) "DIRECTORY_NOT_FOUND";
}

char *fat::getFileName(int pos){
    directory *dir = (directory *) malloc(sizeof(directory));
    for (int j = 0; j < max_dir_num; j++) {
        fread(dir, sizeof(directory), 1, p_file);

        if (dir->start_cluster!=0 && dir->is_file==1) {
            int32_t x = dir->start_cluster;
            while(x!=FAT_FILE_END){
                if (x==pos) return dir->name;
                x = f[x];
            }

        }else if(dir->start_cluster!=0){
            fpos_t position;
            fgetpos(p_file, &position);
            fsetpos(p_file, &default_data_position);
            fseek(p_file, br.cluster_size * dir->start_cluster, SEEK_CUR);
            char *result = getFileName(pos);
            if (strcmp(result, "FILE_NOT_FOUND")==0) fsetpos(p_file, &position);
            else return result;
        }
    }
    free(dir);
    return (char *) "FILE_NOT_FOUND";
}

void fat::getDirectoryMap(int x){
    directory *dir = (directory *) malloc(sizeof(directory));
    vector<directory> dirCont;

    for (int j = 0; j < max_dir_num; j++) {
        fread(dir, sizeof(directory), 1, p_file);
        if (dir->start_cluster!=0) dirCont.push_back(*dir);
        if(dir->start_cluster!=0 && !dir->is_file){
            f[dir->start_cluster] = FAT_UNUSED;
            fpos_t position;
            fgetpos(p_file, &position);
            fsetpos(p_file, &default_data_position);
            fseek(p_file, br.cluster_size * dir->start_cluster, SEEK_CUR);
            getDirectoryMap(x+1);
            fsetpos(p_file, &position);
        }
    }

    char cluster_empty[br.cluster_size];
    memset(cluster_empty, '\0', sizeof(cluster_empty));
    fseek(p_file, -max_dir_num*sizeof(directory), SEEK_CUR);
    fwrite(&cluster_empty, (size_t) br.cluster_size, 1, p_file);

    directoryClustersMap.insert(pair<int, vector<struct directory>>(x ,dirCont));
    free(dir);
}

void fat::getFileMap(){
    directory *dir = (directory *) malloc(sizeof(directory));

    for (int j = 0; j < max_dir_num; j++) {
        fread(dir, sizeof(directory), 1, p_file);
        if (dir->start_cluster!=0 && dir->is_file) {
            int32_t x = dir->start_cluster;
            char cluster_empty[br.cluster_size];
            memset(cluster_empty, '\0', sizeof(cluster_empty));
            fpos_t tempPos;
            vector<string> fileContents;

            fileMap.insert(pair<int, struct directory>(dir->start_cluster, *dir));
            fgetpos(p_file, &tempPos);

            while(x!=FAT_FILE_END){
                string content((unsigned long) br.cluster_size, '\0');

                fsetpos(p_file, &default_data_position);
                fseek(p_file, br.cluster_size*x, SEEK_CUR);
                fread(&content[0], (size_t) br.cluster_size, 1, p_file);
                fseek(p_file, -br.cluster_size, SEEK_CUR);
                fwrite(&cluster_empty, (size_t) br.cluster_size, 1, p_file);

                fileContents.push_back(content);
                int32_t temp = x;
                x = f[x];
                f[temp] = FAT_UNUSED;
            }
            fileContentMap.insert(pair<string, vector<string>>(dir->name, fileContents));
            fsetpos(p_file, &tempPos);
        }else if(dir->start_cluster!=0){
            fpos_t position;
            fgetpos(p_file, &position);
            fsetpos(p_file, &default_data_position);
            fseek(p_file, br.cluster_size * dir->start_cluster, SEEK_CUR);
            getFileMap();
            fsetpos(p_file, &position);
        }
    }
    free(dir);
}

void fat::writeFatStatus() {
    cout << "FAT FILE STATUS" << endl;
    cout << "----------------------" << endl;
    for (int i = 0; i < br.usable_cluster_count; i++) {
        fsetpos(p_file, &default_data_position);
        switch (f[i]) {
            case FAT_DIRECTORY:
                if (i==0) cout << i << "  :  Adresář (ROOT)" << endl;
                else {
                    cout << i << "  :  Adresář (" << getDirName(i) << ")" << endl;
                }
                break;
            case FAT_UNUSED:
                cout << i << "  :  Prázdné" << endl;
                break;
            default:
                cout << i << "  :  Soubor (" << getFileName(i) << ")" << endl;
        }
    }
}
