/* vim: set tabstop=8 shiftwidth=8: */
//=============================================================================
// Name: b2t.c
// Purpose: generate text data file with bin data file
// To build: gcc -std=c99 -o b2t b2t.c
// Copyright (C) 2008 by ZHOU Cheng. All right reserved.
//=============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for strcmp, etc
#include <stdint.h> // for uint?_t, etc
#include <time.h>

#define MAX_STRING_LENGTH 256
//=============================================================================
// enum definition:
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

int npline = 16; // data number per line
char fmt[MAX_STRING_LENGTH] = "%3d"; // data format
char sep[MAX_STRING_LENGTH] = ","; // list separator

//=============================================================================
// Sub-function declare:
//=============================================================================
void deal_with_parameter(int argc, char *argv[]);
FILE *open_file(char *file, char *style, char *memo);
unsigned char *malloc_mem(int size);
void show_help();

//=============================================================================
// The main function:
//=============================================================================
int main(int argc, char *argv[])
{
        clock_t start, finish;
        double duration;
        int i;
        int count;
        int nread; // number readed
        unsigned char *line;

        deal_with_parameter(argc, argv);
        line = malloc_mem(npline);
        fd_i = open_file( file_i, "rb", "read data" );
        fd_o = open_file( file_o, "w" , "write data" );
        count = 0;
        start = clock();
        while(0 != (nread = fread(line, 1, npline, fd_i)))
        {
                if(0 != count) fprintf(fd_o, "%s\n", sep);
                i = 0;
                while(i < nread)
                {
                        fprintf(fd_o, fmt, (unsigned int)line[i]);
                        count++;
                        i++;
                        if(i != nread) fprintf(fd_o, sep);
                }
        }
        finish = clock();
        duration = (double)(finish - start) / CLOCKS_PER_SEC;
        fclose(fd_o);
        fclose(fd_i);
        free(line);
        printf("File %s created, %d-data, %.3f-second used.\n", file_o, count, duration);
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
                printf("No binary file to process...\n\n");
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
                        else if (0 == strcmp(argv[i], "-f"))
                        {
                                strcpy(fmt, argv[++i]);
                        }
                        else if (0 == strcmp(argv[i], "-s"))
                        {
                                strcpy(sep, argv[++i]);
                        }
                        else if (0 == strcmp(argv[i], "-n"))
                        {
                                npline = atoi(argv[++i]);
                                if(npline < 1 || 1024 < npline)
                                {
                                        npline = 16;
                                }
                        }
                        else if (0 == strcmp(argv[i], "-ts"))
                        {
                                strcpy(fmt, "%02X");
                                strcpy(sep, " ");
                                npline = 188;
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
                strcat(file_o, ".txt");
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
        printf("Usage: b2t [options] bin_file\n");
        printf("Options:\n");
        printf("  -o <file>      Output file name, default: *.txt\n");
        printf("  -f <format>    Data format string in C style, default: \"%c3d\"\n", '%');
        printf("  -s <sep>       List separator between data, default: \",\"\n");
        printf("  -n <num>       Data count per line, default: 16\n");
        printf("  -ts            Auto set: -f \"%c02X\" -s \" \" -n 188 for TS file\n", '%');
        printf("  --help         Display this information\n\n");
        printf("b2t v1.00 by ZHOU Cheng, %s %s\n", __TIME__, __DATE__);
}

//=============================================================================
// THE END.
//=============================================================================
