#include <ncurses.h>
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

Task tasks[MAX_TASK_COUNT];
int task_count = 0;

int compare_task_by_priority(const void *a, const void *b)
{
    Task *ta = (Task *)a;
    Task *tb = (Task *)b;
    return ta->priority - tb->priority;
}

void draw_task_list()
{
    clear();
    int padding = 2;
    int todo_y = 1, in_progress_y = 1, done_y = 1;
    int todo_width = COLS / 3;
    int in_progress_width = COLS / 3;
    int done_width = COLS / 3;
    mvvline(0, todo_width, ACS_VLINE, LINES);
    mvvline(0, todo_width + in_progress_width, ACS_VLINE, LINES);
    mvprintw(0, padding, "To Do");
    mvprintw(0, todo_width + padding, "In Progress");
    mvprintw(0, todo_width + in_progress_width + padding, "Done");
    qsort(tasks, task_count, sizeof(Task), compare_task_by_priority);
    for (int i = 0; i < task_count; i++)
    {
        Task task = tasks[i];
        if (task.status == 0)
        {
            mvprintw(todo_y++, padding, "[ ] %d: %s (priority: %d)", i, task.name, task.priority);
        }
        else if (task.status == 1)
        {
            mvprintw(in_progress_y++, todo_width + padding, "[x] %d: %s (priority: %d)", i, task.name, task.priority);
        }
        else
        {
            mvprintw(done_y++, todo_width + in_progress_width + 3, "[v] %d: %s (priority: %d)", i, task.name, task.priority);
        }
    }

    mvprintw(LINES - 1, 0, "Commands: [a]dd [u]pdate [d]elete [q]uit");
    mvprintw(LINES - 1, COLS / 2, "Enter command: ");
    refresh();
}

void DieWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}

// ソケットを宣言しておく
int sock;
// サーバーのアドレス
struct sockaddr_in serverAddr;

void SIGIOHandler(int signalType)
{
    struct sockaddr_in serverAddr;
    unsigned int serverLen;
    int recvMsgSize;

    do
    {
        serverLen = sizeof(serverAddr);
        Responce *res = malloc(sizeof(Responce));

        if ((recvMsgSize = recvfrom(sock, res, sizeof(*res), 0,
                                    (struct sockaddr *)&serverAddr, &serverLen)) < 0)
        {
            if (errno != EWOULDBLOCK)
                DieWithError("recvfrom() failed");
            continue;
        }
        memcpy(tasks, res->tasks, sizeof(res->tasks));
        task_count = res->task_count;
        draw_task_list();
    } while (recvMsgSize >= 0);
}

void UseIdleTime();

void listen_server()
{
    Request req = {LISTEN};
    int reqLen = sizeof(req);

    if (sendto(sock, (struct Request *)&req, reqLen, 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) != reqLen)
        DieWithError("sendto() sent a different number of bytes than expected");
}

void dispose_server()
{
    Request req = {DISPOSE};
    int reqLen = sizeof(req);

    if (sendto(sock, (struct Request *)&req, reqLen, 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) != reqLen)
        DieWithError("sendto() sent a different number of bytes than expected");
}

void add_task(char *name, int priority)
{
    Task task = {};
    strcpy(task.name, name);
    task.priority = priority;
    task.status = 0;

    Request req = {ADD_TASK, 0, task};
    int reqLen = sizeof(req);

    if (sendto(sock, (struct Request *)&req, reqLen, 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) != reqLen)
        DieWithError("sendto() sent a different number of bytes than expected");
}

void update_task(int index, int status)
{
    tasks[index].status = status;
    Request req = {UPDATE_TASK, index, tasks[index]};
    int reqLen = sizeof(req);

    if (sendto(sock, (struct Request *)&req, reqLen, 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) != reqLen)
        DieWithError("sendto() sent a different number of bytes than expected");
}

void delete_task(int index)
{
    Request req = {DELETE_TASK, index};
    int reqLen = sizeof(req);

    if (sendto(sock, (struct Request *)&req, reqLen, 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) != reqLen)
        DieWithError("sendto() sent a different number of bytes than expected");
}

int main(int argc, char *argv[])
{

    // 引数をバリデーション
    if ((argc != 3))
    {
        fprintf(stderr, "Usage: %s <CLIENT PORT> <SERVER PORT>\n", argv[0]);
        exit(1);
    }

    /// サーバーの情報を設定
    unsigned short serverPort;
    char *servIP = "127.0.0.1";

    serverPort = atoi(argv[2]);

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(servIP);
    serverAddr.sin_port = htons(serverPort);

    /// ソケットの作成
    struct sockaddr_in clientAddr;
    unsigned short clientPort;
    struct sigaction handler;

    clientPort = atoi(argv[1]);

    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");

    memset(&clientAddr, 0, sizeof(clientAddr));
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    clientAddr.sin_port = htons(clientPort);

    if (bind(sock, (struct sockaddr *)&clientAddr, sizeof(clientAddr)) < 0)
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
        DieWithError("Unable to put client sock into non-blocking/async mode");

    UseIdleTime();
    return 0;
}

void UseIdleTime()
{
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    listen_server();

    while (1)
    {
        noecho();
        move(LINES - 1, 0);
        printw("Commands: [a]dd [u]pdate [d]elete [q]uit");

        move(LINES - 1, COLS / 2 + 15);
        int c = getch();

        if (c == 'a')
        {
            move(LINES - 1, 0);
            clrtoeol();
            printw("Enter task name:");
            move(LINES - 1, COLS / 2 + 15);
            echo();
            char task_name[100];
            getstr(task_name);
            clrtoeol();
            printw("Enter priority:");
            char priority_str[5];
            move(LINES - 1, COLS / 2 + 15);
            getstr(priority_str);
            int priority = atoi(priority_str);
            add_task(task_name, priority);
        }
        else if (c == 'u')
        {
            move(LINES - 1, 0);
            clrtoeol();
            printw("Enter task index:");
            move(LINES - 1, COLS / 2 + 15);
            echo();
            char task_index_str[5];
            getstr(task_index_str);
            int task_index = atoi(task_index_str);

            move(LINES - 1, 0);
            clrtoeol();
            printw("Enter new status(0: todo, 1: in progress, 2: done):");
            move(LINES - 1, COLS / 2 + 15);
            char new_status_str[100];
            getstr(new_status_str);
            int new_status = atoi(new_status_str);

            update_task(task_index, new_status);
            // noecho();
        }
        else if (c == 'd')
        {
            move(LINES - 1, 0);
            clrtoeol();
            printw("Enter task index:");
            move(LINES - 1, COLS / 2 + 15);
            echo();
            char task_index_str[5];
            getstr(task_index_str);
            int task_index = atoi(task_index_str);

            delete_task(task_index);
            // noecho();
        }
        else if (c == 'q')
        {
            break;
        }
        move(LINES - 1, 0);
        clrtoeol();
    }
    endwin();
    dispose_server();
}