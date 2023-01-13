#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <signal.h>
#include <errno.h>
#include "schema.h"

#define MAX_CLIENT_NUM 10

int sock;
Task tasks[MAX_TASK_COUNT];
struct sockaddr_in clientAddrs[MAX_CLIENT_NUM];
int client_count = 0;
int task_count = 0;

void DieWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}

void UseIdleTime();

void print_tasks()
{
    for (int i = 0; i < task_count; i++)
    {
        printf("%d: %s, %d, %d\n", i, tasks[i].name, tasks[i].priority, tasks[i].status);
    }
}

void brodcast()
{
    for (int i = 0; i < MAX_CLIENT_NUM; i++)
    {
        if (clientAddrs[i].sin_addr.s_addr == 0)
        {
            break;
        }
        printf("Brodcasting!\n");
        print_tasks();
        Responce res = {};
        memcpy(res.tasks, tasks, sizeof(tasks));
        res.task_count = task_count;
        int resLen = sizeof(res);

        if (sendto(sock, (struct Responce *)&res, resLen, 0, (struct sockaddr *)&clientAddrs[i], sizeof(clientAddrs[i])) != resLen)
            DieWithError("sendto() sent a different number of bytes than expected");
    }
    fflush(stdout);
}

void SIGIOHandler(int signalType)
{
    struct sockaddr_in clientAddr;
    unsigned int clientLen;
    int recvMsgSize;

    do
    {
        clientLen = sizeof(clientAddr);
        Request *req = malloc(sizeof(Request));

        if ((recvMsgSize = recvfrom(sock, req, sizeof(*req), 0,
                                    (struct sockaddr *)&clientAddr, &clientLen)) < 0)
        {
            if (errno != EWOULDBLOCK)
                DieWithError("recvfrom() failed");
            continue;
        }

        printf("Received request: %d\n", req->type);

        switch (req->type)
        {
        case LISTEN:
            clientAddrs[client_count] = clientAddr;
            client_count++;
            sleep(1);
            brodcast();
            break;
        case DISPOSE:
            // ポートとアドレスが一致するクライアントを削除する
            for (int i = 0; i < MAX_CLIENT_NUM; i++)
            {
                if (clientAddrs[i].sin_addr.s_addr == clientAddr.sin_addr.s_addr && clientAddrs[i].sin_port == clientAddr.sin_port)
                {
                    for (int j = i; j < MAX_CLIENT_NUM - 1; j++)
                    {
                        clientAddrs[j] = clientAddrs[j + 1];
                    }
                    client_count--;
                    break;
                }
            }
            break;
        case ADD_TASK:
            tasks[task_count] = req->task;
            task_count++;
            brodcast();
            break;
        case UPDATE_TASK:
            // インデックスが問題ないか確認する
            if (req->index < 0 || req->index > task_count)
                break;
            tasks[req->index].status = req->task.status;
            brodcast();
            break;
        case DELETE_TASK:
            // インデックスが問題ないか確認する
            if (req->index < 0 || req->index > task_count)
                break;
            for (int i = req->index; i < task_count - 1; i++)
            {
                tasks[i] = tasks[i + 1];
            }
            task_count--;
            brodcast();
            break;
        default:
            break;
        }
    } while (recvMsgSize >= 0);
}

int main(int argc, char *argv[])
{
    struct sockaddr_in serverAddr;
    unsigned short serverPort;
    struct sigaction handler;

    if (argc != 2)
    {
        fprintf(stderr, "Usage:  %s <SERVER PORT>\n", argv[0]);
        exit(1);
    }

    serverPort = atoi(argv[1]);

    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(serverPort);

    if (bind(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
        DieWithError("bind() failed");

    handler.sa_handler = SIGIOHandler;

    if (sigfillset(&handler.sa_mask) < 0)
        DieWithError("sigfillset() failed");

    handler.sa_flags = 0;

    if (sigaction(SIGIO, &handler, 0) < 0)
        DieWithError("sigaction() failed for SIGIO");

    if (fcntl(sock, F_SETOWN, getpid()) < 0)
        DieWithError("Unable to set process owner to us");

    if (fcntl(sock, F_SETFL, O_NONBLOCK | FASYNC) < 0)
        DieWithError("Unable to put server sock into non-blocking/async mode");

    for (;;)
        UseIdleTime();
}

void UseIdleTime()
{
}