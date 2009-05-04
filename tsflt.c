//=============================================================================
// Name: tsflt.c
// Purpose: analyse certain character with ts file
// To build: gcc -std=c99 -o tsflt tsflt.c
// Copyright (C) 2009 by ZHOU Cheng. All right reserved.
//=============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for strcmp, etc
#include <stdint.h> // for uint?_t, etc

//=============================================================================
// enum & struct definition:
//=============================================================================
enum
{
        NO_ERROR = 0,
        FOPEN_ERROR,
        MALLOC_ERROR,
        PARA_ERROR
};

//=============================================================================
// Variables definition:
//=============================================================================
FILE *fd_i;
FILE *fd_o;

char file_i[FILENAME_MAX] = "";
char file_o[FILENAME_MAX] = "";

int sizeofTS = 188; // Size of TS package
int with_empty = 0; // default: filter empty package

//=============================================================================
// Sub-function declare:
//=============================================================================
void deal_with_parameter(int argc, char *argv[]);
FILE *open_file(char *file, char *style, char *memo);
unsigned char *malloc_mem(int size);
void show_help();
void printb(uint32_t x, int head, int tail);

//=============================================================================
// The main function:
//=============================================================================
int main(int argc, char *argv[])
{
        uint32_t count;
        int nread; // number readed
        uint8_t *line;

        deal_with_parameter(argc, argv);
        line = malloc_mem(sizeofTS);
        fd_i = open_file( file_i, "rb", "read data" );
        fd_o = open_file( file_o, "wb", "write data" );

        count = 0;
        while(0 != (nread = fread(line, 1, sizeofTS, fd_i)))
        {
                uint16_t pid;

                if(0x47 != line[0x00])
                {
                        printf("Wrong package head!\n");
                        break;
                }

                pid = line[0x01];
                pid <<= 8;
                pid |= line[0x02];
                pid &= 0x1FFF;

                if(with_empty || (!with_empty && 0x1FFF != pid))
                {
                        fwrite(line, 1, sizeofTS, fd_o);
                        count += sizeofTS;
                }
        }
        fclose(fd_o);
        fclose(fd_i);
        free(line);
        printf("File %s created, %d-data.\n", file_o, count);
        exit(NO_ERROR);
}

//=============================================================================
// Subfunctions definition:
//=============================================================================
void deal_with_parameter(int argc, char *argv[])
{
        int i = 1;

        if(1 == argc)
        {
                // no parameter
                printf("No file to process...\n\n");
                show_help();
                exit(NO_ERROR);
        }
        while (i < argc)
        {
                if ('-' == argv[i][0])
                {
                        if (0 == strcmp(argv[i], "-o"))
                        {
                                strcpy(file_o, argv[++i]);
                        }
                        else if (0 == strcmp(argv[i], "-n"))
                        {
                                sizeofTS = atoi(argv[++i]);
                                if(188 != sizeofTS && 204 != sizeofTS)
                                {
                                        sizeofTS = 188;
                                }
                        }
                        else if (0 == strcmp(argv[i], "--with-empty"))
                        {
                                with_empty = 1;
                        }
                        else if (0 == strcmp(argv[i], "--help"))
                        {
                                show_help();
                                exit(NO_ERROR);
                        }
                        else
                        {
                                printf("Wrong parameter: %s\n", argv[i]);
                                exit(PARA_ERROR);
                        }
                }
                else
                {
                        strcpy(file_i, argv[i]);
                        i = argc;
                }
                i++;
        }
        // make output file name with input file name
        if('\0' == file_o[0])
        {
                // search last '.'
                i = strlen(file_i) - 1;
                while(i >= 0)
                {
                        if('.' == file_i[i]) break;
                        i--;
                }
                // fill file_o[]
                file_o[i--] = '\0';
                while(i >= 0)
                {
                        file_o[i] = file_i[i];
                        i--;
                }
                strcat(file_o, "2.ts");
        }
}

FILE *open_file( char *file, char *style, char *memo )
{
        FILE *fd;
        if ( NULL == ( fd = fopen( file, style ) ) )
        {
                printf( "Can not open file \"%s\" to %s!\n", file, memo );
                exit(FOPEN_ERROR);
        }
        fseek( fd, 0, SEEK_SET );
        return fd;
}

unsigned char *malloc_mem(int size)
{
        unsigned char *ptr = NULL;

        ptr = (unsigned char *)malloc(size);
        if(NULL == ptr)
        {
                printf("Can not malloc %d-byte memory!\n", size);
                exit(MALLOC_ERROR);
        }
        return ptr;
}

void show_help()
{
        printf("Usage: tsflt [-n *] [--with-empty] [-o *] TS_file\n");
        printf("Options:\n");
        printf("  -n <num>       Size of TS package, default: 188\n");
        printf("  --with-empty   With empty package, default: filter empty package\n");
        printf("  -o <file>      Output file name, default: *2.ts\n");
        printf("  --help         Display this information\n\n");
        printf("tsflt v1.00 by ZHOU Cheng, %s %s\n", __TIME__, __DATE__);
}

//=============================================================================
// THE END.
//=============================================================================
