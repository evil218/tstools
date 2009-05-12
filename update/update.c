
#include "winsock.h"
#include "stdio.h"

#define TRUE 1
#define INT8U unsigned char
#define INT8S signed char
#define INT16U unsigned short
#define INT16S unsigned short
#define INT32U unsigned long

const unsigned char ServiceIP[] = {192,168,  1,  252};

int main(void)
{
        int sock,length;
        unsigned long opt;
        struct sockaddr_in server;
        int err;
        WORD wVersionRequested;
        WSADATA wsaData;
        int rdata;
        unsigned char TBuf[1100];
        unsigned char RBuf[100];
        FILE *fp;
        INT32U DataLen;
        INT16U DataCheckSum;
        INT32U DataPointer;
        INT16U ch,i;

        //build socket
        wVersionRequested = MAKEWORD(1,1);
        err = WSAStartup(wVersionRequested, &wsaData);
        if(err != 0){
                return 1;
        }
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        err = GetLastError();
        if(sock < 0){
                perror("opening stream socket error!");
                return 1;
        }
        /* ·Ç×èÈûÄ£Ê½ */
        opt = 1;
        ioctlsocket(sock, FIONBIO, &opt);
        //name the socket
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = *((INT32U *)&ServiceIP[0]);
        server.sin_port = htons(22);
        rdata = connect(sock, (struct sockaddr *)&server, sizeof(server));
        err = GetLastError();
        if(rdata < 0){
                perror("binding stream socket error!");
                return 1;
        }
        //find the port and print it
        length = sizeof(server);
        rdata = getsockname(sock, (struct sockaddr *)&server, &length);
        err = GetLastError();
        if(rdata < 0){
                perror("getting socket name error!");
                return 1;
        }

        if((fp = fopen("rom.bin", "rb")) == NULL){
                printf("cannot open file: rom.bin !\n");
                return 0;
        }
        DataLen = 0;
        DataCheckSum = 0;
        while(1){
                if(fread(&ch, 2, 1, fp)){
                        DataLen += 2;
                        DataCheckSum += ch;
                } else {
                        fclose(fp);
                        break;
                }
        }
        fp = fopen("rom.bin", "rb");
        //begin transmit data
        TBuf[0] = 'u'; TBuf[1] = 'p'; TBuf[2] = 'd'; TBuf[3] = 'a'; TBuf[4] = 't'; TBuf[5] = 'w';
        TBuf[6] = (INT8U)(DataLen>>24); TBuf[7] = (INT8U)(DataLen>>16); TBuf[8] = (INT8U)(DataLen>>8); TBuf[9] = (INT8U)DataLen;
        TBuf[10] = (INT8U)(DataCheckSum>>8); TBuf[11] = (INT8U)(DataCheckSum);
        send(sock, TBuf, 12, 0);
        Sleep(50);
        length = recv(sock, RBuf, 100, 0);
        if((length > 0)&&(RBuf[0] == 'u')&&(RBuf[1] == 'p')&&(RBuf[2] == 'd')&&(RBuf[3] == 'a')&&(RBuf[4] == 't')&&(RBuf[5] == 'w')){
                DataPointer = 0;
                printf("connect OK!\n");
                printf("Transmit data......\n");
                while(1){
                        if(fread(&TBuf[4], 1, 1024, fp)){
                                TBuf[0] = (INT8U)(DataPointer>>24);
                                TBuf[1] = (INT8U)(DataPointer>>16);
                                TBuf[2] = (INT8U)(DataPointer>>8);
                                TBuf[3] = (INT8U)(DataPointer);
                                while(1){
                                        send(sock, TBuf, 1028, 0);
                                        Sleep(20);
                                        length = recv(sock, RBuf, 100, 0);
                                        if((length > 0)&&(TBuf[0] == RBuf[0])&&(TBuf[1] == RBuf[1])&&(TBuf[2] == RBuf[2])&&(TBuf[3] == RBuf[3])){
                                                break;
                                        }
                                }
                                DataPointer += 1024;
                                printf(".");
                        } else {
                                fclose(fp);
                                printf(".\n");
                                break;
                        }
                }
                TBuf[0] = 'r'; TBuf[1] = 'e'; TBuf[2] = 's'; TBuf[3] = 'e'; TBuf[4] = 't';
                send(sock, TBuf, 5, 0);
                for(i = 0; i < 20; i++){
                        length = recv(sock, RBuf, 100, 0);
                        if(length > 0)
                                break;
                        Sleep(1000);
                }
                if((length > 0)&&(RBuf[9] == 'O')&&(RBuf[10] == 'K')){
                        printf("CheckSum OK!\n");
                } else {
                        printf("CheckSum Error!\n");
                }
        } else {
                printf("connect error!\n");
        }
        closesocket(sock);

        WSACleanup();
        while(1){
                Sleep(1000);
        }
        return 0;
}
