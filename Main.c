#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <Windows.h>
#include <fltUser.h>
#include <stdio.h>
#include <winsock2.h>     // For socket(), bind(), etc.
#include <stdlib.h>
#define STATUS_SUCCESS ((NTSTATUS)0x00000000)
#pragma comment(lib, "Ws2_32.lib")

void HandleMessage(const BYTE* buffer, HANDLE hPort, ULONGLONG messageId);

struct sockaddr_in* cacheMangaer;

enum MsgType {
    PID,
    OPEN,
    WRITE1,
    WRITE2,
    CREATE,
    DEL
};

struct cacheMsg {
    int len;
    char data[1];
}typedef cacheMsg;

struct AfsRoutePortMessage {
    enum MsgType state;
    USHORT dataLength;
    WCHAR data[1];
}typedef AfsRoutePortMessage;

struct AfsRouteReplyMsg {
    FILTER_REPLY_HEADER ReplyHeader;
    INT data; // can be a process id or 0 if approved -1 if needs to be loaded , -2 if not approved
}typedef AfsRouteReplyMsg;

int main() {
    //main function
    HANDLE hPort;
    HRESULT hr = FilterConnectCommunicationPort(L"\\AfsRoute", 0, NULL, 0, NULL, &hPort); //setup connection with driver
    if (FAILED(hr)) {
        printf("Error connecting to port (HR=0x%08X)\n", hr);
        return 1;
    }

    printf("reciving msgs\n");
    BYTE buffer[1 << 12]; // 4 KB
    FILTER_MESSAGE_HEADER* message = (FILTER_MESSAGE_HEADER*)buffer; 
    for (;;) {
        hr = FilterGetMessage(hPort, message, sizeof(buffer), NULL); //recv msg from driver 
        if (FAILED(hr)) {
            printf("Error receiving message (0x%08X)\n", hr);
            break;
        }
        HandleMessage(buffer + sizeof(FILTER_MESSAGE_HEADER), hPort, message->MessageId); //handle msg from driver 
    }
    CloseHandle(hPort);
    return 0;
}


cacheMsg* createCacheMsg(char* msg, int len) { 
    //creates msg to send to the cache manager
    int total_len = sizeof(cacheMsg) + len;
    cacheMsg* builtMsg = (cacheMsg*)malloc(sizeof(cacheMsg) + len);
    memchr(msg, 0, total_len);
    builtMsg->len = len;
    memcpy(builtMsg->data, msg, len);
    return builtMsg;
}

int talkToCache(enum MsgType state, USHORT len, PWCHAR data) {
    //connects to the cache manager and exchange messages
    printf("\n talk to cache : data : %d len : %hu data: %S ", state, len, data);
    WSADATA wsaData; // structure to hold the Winsock data
    int result;
    result = WSAStartup(MAKEWORD(2, 2), &wsaData); //start winsock2 
    if (result != 0) {
        printf("WSAStartup failed: %d\n", result);
        return 1;
    }
    printf("started\n");
    struct sockaddr_in cacheM = { .sin_family = 2, .sin_port = htons(9998) }; // set addr
    memset(cacheM.sin_zero, 0, 8);
    cacheM.sin_addr.s_addr = inet_addr("127.0.0.1");
    SOCKET connectedSocket = socket(2, SOCK_STREAM, 0); // AF_LOCAL = 2 for the 127.0.0.1 
    int status = connect(connectedSocket, (struct sockaddr*)&cacheM, sizeof(struct sockaddr_in)); //connect 
    if (status == SOCKET_ERROR) {
        printf("socket error was encounterd %d", WSAGetLastError());
    }

    char* buff;
    int i;

    int lengthBuffer;
    
    switch (state) {
        case PID:
            buff = (char*)malloc(4);
            lengthBuffer = 4;
            memchr(buff, 0, lengthBuffer);
            strcpy_s(buff, lengthBuffer, "PID");
            break;
        case OPEN:    
            lengthBuffer = len + 6;
            buff = (char*)malloc(lengthBuffer);
            memchr(buff, 0, lengthBuffer);
            strcpy_s(buff, 6, "open ");
            i = 5;
            for (; i < lengthBuffer; i++) {
                buff[i] = (char)data[i-5];
            }
            //buff[i] = '\0';
            memchr(buff, 0, lengthBuffer);
            break;
        case WRITE1:
            printf("write1");
            lengthBuffer = len + 8;
            buff = (char*)malloc(lengthBuffer);
            memchr(buff, 0, lengthBuffer);
            strcpy_s(buff, 8, "write1 ");
            i = 7;
            for (; i < lengthBuffer; i++) {
                buff[i] = (char)data[i - 7];
            }
            //buff[i] = '\0';
            //printf("buffer %s", buff);
            break;
        case WRITE2:
            lengthBuffer = len + 7;
            buff = (char*)malloc(lengthBuffer);
            memchr(buff, 0, lengthBuffer);
            strcpy_s(buff, 7, "write ");
            i = 6;
            for (; i < lengthBuffer; i++) {
                buff[i] = (char)data[i - 6];
            }
            //buff[i] = '\0';
            //printf("buffer %s", buff);
            break;
        default:
            //buff = (char*)malloc(11);
            lengthBuffer = 11;
            buff = "open /dir2";
            break;
    }
    printf("buffer1 %s", buff);

    char* msg_to_send = (char*)createCacheMsg(buff, lengthBuffer); //create msg for cache manager
    printf("buffer2 %s", buff);
    status = send(connectedSocket, msg_to_send, lengthBuffer + sizeof(cacheMsg), 0);
    char buffer[4];
    status = recv(connectedSocket, buffer, 4, 0);
    printf("\n %d ", *(INT*)buffer); 
    int rdata = *(INT*)buffer;
    closesocket(connectedSocket);
    free(msg_to_send);
    WSACleanup();
    free(buff);
    return rdata;  //concerts char[] into int
}


void HandleMessage(const BYTE* buffer, HANDLE hPort, ULONGLONG messageId) { //, 
    AfsRoutePortMessage* my_msg = (AfsRoutePortMessage*)buffer;
    printf("\nmsg type: %hu , msg len: %hu msg data: %S ", my_msg->state, my_msg->dataLength, my_msg->data);
    //needs to talk to cache manager
    int data = talkToCache(my_msg->state, my_msg->dataLength, my_msg->data); // talk to cache manager
    printf("\n  12345 data to send in the reply : %d", data);
    //add msgs idk what i think about it 
    
    AfsRouteReplyMsg reply;
    reply.ReplyHeader.Status = STATUS_SUCCESS;
    reply.ReplyHeader.MessageId = messageId;
    reply.data = data; // set  custom data as needed
    printf("\n data to send in the reply : %d", reply.data);
    FilterReplyMessage(hPort, (PFILTER_REPLY_HEADER)&reply, sizeof(FILTER_REPLY_HEADER) + sizeof(INT)); // reply to driver msg
}

