#include <stdio.h>
#include "filesystem.h"
#include "softwaredisk.h"

int main(int argc, char *argv[]){
    init_software_disk();
    printf("Checking data structure sizes and alignments...\n");
    if(! check_structure_alignment()){
        printf("Check Failed. Filesystem not initilized and should not be used.\n");
    } else {
        prinf("Check succeeded.\n");

        printf("Initilizing filesystem...\n");
        //Goldens FS only simply requires a completely zeroed software disk.
        init_software_disk();
        printf("Done.\n");
    }
    return 0;
}