#ifndef _SERVER_H_
#define _SERVER_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#define MAX_CLIENT 1000
#define PORT 9090
#define MAX_EVENTS 1000

struct epoll_event *ep_events;

typedef struct _Word{
    char word[9];
    struct _Word *next;
}Word;
typedef struct _Word_List{
    Word *head;
    Word *tail;
}Word_List;

typedef struct _Room{
    int num;
    int user[4];
    int user_cnt;
    int direction;
    int leader;

    int round;
    char keyword[18];
    char last_letter[3];
    Word_List *used_word;

    struct _Room *next;
}Room;
typedef struct _Room_List{
    Room *head;
    Room *tail;
}Room_List;

typedef enum{WORD=0, HINT, MAKE_ROOM, JOIN_ROOM, EXIT_ROOM, JUMP, BACK, GAME_START, REFRESH, TIME_OUT}TYPE;

int ep_fd;
Room_List *room_list;

pthread_mutex_t mutex1, mutex2;

void setnonblockingmode(int fd);
void* thread_main(void* args);
void error_handling(char* msg);
void message_handling(int clnt_sock, char* buf, int size);
void send_room_list(int clnt_sock);
void make_room(int clnt_sock, char *buf);
Room* search_room(int room_num);
void delete_room(int room_num);
int room_cnt();
void join_room(int clnt_sock, char *buf);
void exit_room(int clnt_sock, char *buf);
void input_word(char *buf);
void add_used_word(Word_List* used_word, char* input_word);
void start_game(char* buf);
void end_round(char* buf);
void send_hint(int clnt_sock, char* buf);
void jump_order(char* buf);
void back_order(char* buf);
#endif
