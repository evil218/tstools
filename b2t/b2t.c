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
#include <time.h>

#include "error.h"

#define MAX_STRING_LENGTH 256

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
static int deal_with_parameter(int argc, char *argv[]);
static void show_help();

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

        if(0 != deal_with_parameter(argc, argv))
        {
                return -1;
        }

        line = (unsigned char *)malloc(npline);
        if(NULL == line)
        {
                DBG(ERR_MALLOC_FAILED);
                return -ERR_MALLOC_FAILED;
        }

        fd_i = fopen(file_i, "rb");
        fd_o = fopen(file_o, "w");
        if(NULL == fd_i || NULL == fd_o)
        {
                DBG(ERR_FOPEN_FAILED);
                return -ERR_FOPEN_FAILED;
        }

        count = 0;
        start = clock();
        while(0 != (nread = fread(line, 1, npline, fd_i)))
        {
                if(0 != count) fprintf(fd_o, "%s\n", sep);
                for(i = 0; i < nread; i++, count++)
                {
                        fprintf(fd_o, fmt, (unsigned int)line[i]);
                        if(i != nread - 1) fprintf(fd_o, sep);
                }
        }
        finish = clock();
        duration = (double)(finish - start) / CLOCKS_PER_SEC;
        fclose(fd_o);
        fclose(fd_i);
        free(line);
        fprintf(stdout, "File %s created, %d-data, %.3f-second used.\n",
                file_o, count, duration);

        return 0;
}

//=============================================================================
// Subfunctions definition:
//=============================================================================
static int deal_with_parameter(int argc, char *argv[])
{
        int i;

        if(1 == argc)
        {
                // no parameter
                fprintf(stderr, "No binary file to process...\n\n");
                show_help();
                return -1;
        }

        for(i = 1; i < argc; i++)
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
                                return -1;
                        }
                        else
                        {
                                fprintf(stderr, "Wrong parameter: %s\n", argv[i]);
                                DBG(ERR_BAD_ARG);
                                return -ERR_BAD_ARG;
                        }
                }
                else
                {
                        strcpy(file_i, argv[i]);
                }
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

        return 0;
}

static void show_help()
{
        puts("Usage: b2t [options] bin_file [options]");
        puts("Options:");
        puts("  -o <file>      Output file name, default: *.txt");
        fprintf(stdout, "  -f <format>    Data format string in C style, default: \"%c3d\"\n", '%');
        puts("  -s <sep>       List separator between data, default: \",\"");
        puts("  -n <num>       Data count per line, default: 16");
        fprintf(stdout, "  -ts            Auto set: -f \"%c02X\" -s \" \" -n 188 for TS file\n", '%');
        puts("  --help         Display this information");
        puts("");
        fprintf(stdout, "b2t v1.00 by ZHOU Cheng, %s %s\n", __TIME__, __DATE__);
        return;
}

//=============================================================================
// THE END.
//=============================================================================
