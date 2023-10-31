#include <stdlib.h>
#include <stdio.h>
#include <papi.h>
#include "sha256.h"

void handle_error(int retval)
{
    printf("PAPI error %d: %s\n", retval, PAPI_strerror(retval));
    exit(1);
}

int main(int argc, char const *argv[])
{
    int retval;
    FILE *file;
    FILE *fileForPrinting;
    char fileName[] = "input.txt";
    int argumentCount = argc;

    // Open a file, specifying which file using command line arguments

    file = fopen(fileName, "r");

    // First check to make sure the file could be found
    if (file == NULL)
    {
        printf("\n Could not open file %s\n", fileName);
        exit(1);
    }

    // Function calls
    printf("\n File ok, executing functions.. \n");
    endianCheckPrint();
    // fileForPrinting = fopen(fileName, "r");
    // printFileContents(fileForPrinting);

    retval = PAPI_hl_region_begin("computation");
    if (retval != PAPI_OK)
        handle_error(1);

    // Open a file, specifying which file using command line arguments
    calculateHash(file);

    retval = PAPI_hl_region_end("computation");
    if (retval != PAPI_OK)
        handle_error(1);

    fclose(file);

    return 0;
}
