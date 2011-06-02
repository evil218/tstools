/* vim: set tabstop=8 shiftwidth=8: */
//=============================================================================
// Name: catbin.c
// Purpose: generate text data file with bin data file
// To build: gcc -std=c99 -o catbin catbin.c
// Copyright (C) 2008 by ZHOU Cheng. All right reserved.
//=============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for strcmp, etc
#include <stdint.h> // for uintN_t, etc

#include "error.h"
#include "if.h"

enum
{
        FILE_MTS,
        FILE_TS,
        FILE_BIN
};

//=============================================================================
// Variables definition:
//=============================================================================
static FILE *fd_i = NULL;
static char file_i[FILENAME_MAX] = "";
static int npline = 188; // data number per line
static char white_space = ' ';
static int file_type = FILE_BIN;
static int show_address = 0;
static int dec_address = 0; // default: hex address
//static uint64_t file_size = 0;
static int aim_start = 0; // first byte
static int aim_stop = 0; // last byte

//=============================================================================
// Sub-function declare:
//=============================================================================
static int deal_with_parameter(int argc, char *argv[]);
static void show_help();
static void show_version();
static int ts_sync();

//=============================================================================
// The main function:
//=============================================================================
int main(int argc, char *argv[])
{
        unsigned char bbuf[ LINE_LENGTH_MAX / 3 + 10]; // bin data buffer
        char tbuf[LINE_LENGTH_MAX + 10]; // txt data buffer
        uint64_t addr = 0;
        char *addr_fmt;

        if(0 != deal_with_parameter(argc, argv))
        {
                return -1;
        }
        addr_fmt = (dec_address) ? "%llu%c" : "%llX%c";

        fd_i = fopen(file_i, "rb");
        if(NULL == fd_i)
        {
                fprintf(stderr, "%s ", file_i);
                DBG(ERR_FOPEN_FAILED, "\n");
                return -ERR_FOPEN_FAILED;
        }

        switch(file_type)
        {
                case FILE_TS:
                        if(0 != ts_sync())
                        {
                                fprintf(stderr, "EOF, but TS sync failed!\n");
                                return -1;
                        }
                        break;
                case FILE_MTS:
                        fseek(fd_i, +4, SEEK_CUR);
                        npline = 192;
                        break;
                default: // FILE_BIN
                        break;
        }

        while(1 == fread(bbuf, npline, 1, fd_i))
        {
                b2t(tbuf, bbuf, npline, white_space);
                if(show_address)
                {
                        fprintf(stdout, addr_fmt, addr, white_space);
                }
                puts(tbuf);
                addr += npline;
        }

        fclose(fd_i);

        return 0;
}

//=============================================================================
// Subfunctions definition:
//=============================================================================
static int deal_with_parameter(int argc, char *argv[])
{
        int i;
        int dat;

        if(1 == argc)
        {
                // no parameter
                fprintf(stderr, "No binary file to process...\n\n");
                show_help();
                return -1;
        }

        for(i = 1; i < argc; i++)
        {
                if('-' == argv[i][0])
                {
                        if(     0 == strcmp(argv[i], "-w") ||
                                0 == strcmp(argv[i], "--width")
                        )
                        {
                                sscanf(argv[++i], "%i" , &dat);
                                if(0 < dat && dat <= (LINE_LENGTH_MAX / 3))
                                {
                                        npline = dat;
                                }
                                else
                                {
                                        fprintf(stderr,
                                                "bad variable for '-n': %d, use %d instead!\n",
                                                dat, npline);
                                }
                        }
                        else if(0 == strcmp(argv[i], "-bin"))
                        {
                                file_type = FILE_BIN;
                        }
                        else if(0 == strcmp(argv[i], "-ts"))
                        {
                                file_type = FILE_TS;
                        }
                        else if(0 == strcmp(argv[i], "-mts"))
                        {
                                file_type = FILE_MTS;
                        }
                        else if(0 == strcmp(argv[i], "-a") ||
                                0 == strcmp(argv[i], "--addr")
                        )
                        {
                                show_address = 1;
                        }
                        else if(0 == strcmp(argv[i], "-d") ||
                                0 == strcmp(argv[i], "--decaddr")
                        )
                        {
                                dec_address = 1;
                        }
                        else if(0 == strcmp(argv[i], "-start"))
                        {
                                int start;

                                i++;
                                if(i >= argc)
                                {
                                        fprintf(stderr, "no parameter for '-start'!\n");
                                        exit(EXIT_FAILURE);
                                }
                                sscanf(argv[i], "%i" , &start);
                                aim_start = start;
                        }
                        else if(0 == strcmp(argv[i], "-stop"))
                        {
                                int stop;

                                i++;
                                if(i >= argc)
                                {
                                        fprintf(stderr, "no parameter for '-stop'!\n");
                                        exit(EXIT_FAILURE);
                                }
                                sscanf(argv[i], "%i" , &stop);
                                aim_stop = stop;
                        }
                        else if(0 == strcmp(argv[i], "-s") ||
                                0 == strcmp(argv[i], "--space")
                        )
                        {
                                sscanf(argv[++i], "%c" , &white_space);
                        }
                        else if(0 == strcmp(argv[i], "-h") ||
                                0 == strcmp(argv[i], "--help")
                        )
                        {
                                show_help();
                                return -1;
                        }
                        else if(0 == strcmp(argv[i], "-v") ||
                                0 == strcmp(argv[i], "--version")
                        )
                        {
                                show_version();
                                return -1;
                        }
                        else
                        {
                                fprintf(stderr, "Wrong parameter: %s\n", argv[i]);
                                DBG(ERR_BAD_ARG, "\n");
                                return -ERR_BAD_ARG;
                        }
                }
                else
                {
                        strcpy(file_i, argv[i]);
                }
        }

        return 0;
}

static void show_help()
{
        puts("'catbin' read binary file, translate 0xXX to 'XY ' format, then send to stdout.");
        puts("");
        puts("Usage: catbin [OPTION] file [OPTION]");
        puts("");
        puts("Options:");
        puts("");
        puts(" -bin                     treat as common binary file, default");
        puts(" -ts                      treat as TS file, sync and guess packet length at first");
        puts(" -mts                     treat as MTS file, sync and guess packet length at first");
        puts(" -a, --addr               show data address at line head, default: do NOT show it");
        puts(" -d, --decaddr            dec address format, default: hex");
#if 0
        puts(" -start <a>               cat from a%(file length), default: 0, first byte");
        puts(" -stop <b>                cat to b%(file length), default: 0, last byte");
#endif
        puts(" -s, --seperate <,>       white space, any char except [0-9A-Fa-f], default: ' '");
        puts(" -w, --width <w>          w-byte per line, [1,10922], default: 188");
        puts(" -h, --help               display this information");
        puts(" -v, --version            display my version");
        puts("");
        puts("Examples:");
        puts("  catbin xxx.ts");
        puts("");
        puts("Report bugs to <zhoucheng@tsinghua.org.cn>.");
        return;
}

static void show_version()
{
        puts("catbin 1.0.0");
        puts("");
        puts("Copyright (C) 2009,2010,2011 ZHOU Cheng.");
        puts("This is free software; contact author for additional information.");
        puts("There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR");
        puts("A PARTICULAR PURPOSE.");
        puts("");
        puts("Written by ZHOU Cheng.");
        return;
}

enum
{
        STATE_SYNC2_204,
        STATE_SYNC2_188,
        STATE_SYNC1_204,
        STATE_SYNC1_188,
        STATE_SYNC0,
        STATE_EXIT
};

// for TS data: use "state machine" to determine sync position and packet size
// continuous 3-sync means TS sync
static int ts_sync()
{
        uint8_t dat;
        int state = STATE_SYNC0;

        while(STATE_EXIT != state)
        {
                switch(state)
                {
                        case STATE_SYNC0:
                                if(1 != fread(&dat, 1, 1, fd_i))
                                {
                                        return -1;
                                }
                                if(0x47 == dat)
                                {
                                        fseek(fd_i, +187, SEEK_CUR);
                                        state = STATE_SYNC1_188;
                                }
                                else
                                {
                                        // STATE_SYNC0
                                }
                                break;
                        case STATE_SYNC1_188:
                                if(1 != fread(&dat, 1, 1, fd_i))
                                {
                                        return -1;
                                }
                                if(0x47 == dat)
                                {
                                        fseek(fd_i, +187, SEEK_CUR);
                                        state = STATE_SYNC2_188;
                                }
                                else
                                {
                                        fseek(fd_i, +15, SEEK_CUR);
                                        state = STATE_SYNC1_204;
                                }
                                break;
                        case STATE_SYNC1_204:
                                if(1 != fread(&dat, 1, 1, fd_i))
                                {
                                        return -1;
                                }
                                if(0x47 == dat)
                                {
                                        fseek(fd_i, +203, SEEK_CUR);
                                        state = STATE_SYNC2_204;
                                }
                                else
                                {
                                        fseek(fd_i, -204, SEEK_CUR);
                                        state = STATE_SYNC0;
                                }
                                break;
                        case STATE_SYNC2_188:
                                if(1 != fread(&dat, 1, 1, fd_i))
                                {
                                        return -1;
                                }
                                if(0x47 == dat)
                                {
                                        //fprintf(stderr, "sync with 188\n");
                                        fseek(fd_i, -377, SEEK_CUR);
                                        npline = 188;
                                        state = STATE_EXIT;
                                }
                                else
                                {
                                        fseek(fd_i, -376, SEEK_CUR);
                                        state = STATE_SYNC0;
                                }
                                break;
                        case STATE_SYNC2_204:
                                if(1 != fread(&dat, 1, 1, fd_i))
                                {
                                        return -1;
                                }
                                if(0x47 == dat)
                                {
                                        //fprintf(stderr, "sync with 204\n");
                                        fseek(fd_i, -409, SEEK_CUR);
                                        npline = 204;
                                        state = STATE_EXIT;
                                }
                                else
                                {
                                        fseek(fd_i, -408, SEEK_CUR);
                                        state = STATE_SYNC0;
                                }
                                break;
                        default:
                                fprintf(stderr, "bad state!\n");
                                return -1;
                }
        }
        return 0;
}

//=============================================================================
// THE END.
//=============================================================================
