#include "server.h"
#include "seminsql.h"
int main()
{
    /* Init MySQL */
    mysql_init(&mysql);
    mysqlConnect("localhost", "root", "1111", "wordDB", 3306);
    /* About Socket */
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t addr_size;

    /* About Epoll */
    struct epoll_event event;
    int event_cnt;

    /* About Thread and Mutex */
    pthread_t t_id;
    pthread_mutex_init(&mutex1, NULL); // mutex1 : When access Room_list
    pthread_mutex_init(&mutex2, NULL); // mutex2 : When access Room(property)

    /* About Room_list(Linked List) */
    room_list = (Room_List*)malloc(sizeof(Room_List));
    room_list->head = NULL;
    room_list->tail = NULL;

    /* Server Socket Regist */
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(PORT);

    if(bind(serv_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1)
        error_handling("[ERROR] bind() error");
    if(listen(serv_sock, MAX_CLIENT) == -1)
        error_handling("[ERROR] listen() error");

    /* Epoll Create */
    ep_fd = epoll_create(MAX_CLIENT);
    ep_events = malloc(sizeof(struct epoll_event)*MAX_CLIENT);

    /* Add Server Socket to Epoll */
    setnonblockingmode(serv_sock); // Non-blocking Socket for Edge Trigger
    event.events = EPOLLIN;
    event.data.fd = serv_sock;
    epoll_ctl(ep_fd, EPOLL_CTL_ADD, serv_sock, &event);


    /* Waitng for Event */
    while(1){
        event_cnt = epoll_wait(ep_fd, ep_events, MAX_CLIENT, -1);
        if(event_cnt == -1){
            puts("[ERROR] epll_wait() error");
            break;
        }

        for(int i=0; i<event_cnt; i++){
            /* Client Accept */
            if(ep_events[i].data.fd == serv_sock){
                addr_size = sizeof(clnt_addr);
                clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &addr_size);
                setnonblockingmode(clnt_sock); // Non-blocking Socket for Edge Trigger
                event.events = EPOLLIN | EPOLLET;
                event.data.fd = clnt_sock;
                epoll_ctl(ep_fd, EPOLL_CTL_ADD, clnt_sock, &event);

                printf("Connected Client : %s\n", inet_ntoa(clnt_addr.sin_addr));
                send_room_list(clnt_sock); // Send Room list to Client
            }
            else{
                /* Receive Message from Client */
                 if(pthread_create(&t_id, NULL, thread_main, (void*)&ep_events[i].data.fd) != 0)
                    error_handling("[ERROR] pthread_create() error");

                 system("clear");
                 printf("<< Room Status >>\n");
                 Room* p = room_list->head;
                 while(p != NULL){
                     printf("[ Room No. %d ] (%d/4)\n", p->num, p->user_cnt);
                     for(int i=0; i<4; i++){
                         if(p->user[i] == -1)
                             printf(" - ");
                         else{
                             if(i == p->leader)
                                 printf(" # ");
                             else
                                 printf(" @ ");
                         }
                     }
                     printf("\t< # : Leader >\n");

                     char keyword[19];
                     strncpy(keyword, p->keyword, 18);
                     keyword[18] = '\0';
                     if(p->round == 0)
                         printf("<< Wait For Starting..... >>");
                     else if(p->round <= 6){
                         printf("[ Round %d ] ! %s !\n", p->round, keyword);
                         Word* pp = p->used_word->head;
                         while(pp != NULL){
                             printf(" %s ", pp->word);
                             pp = pp->next;
                         }
                     }
                     else
                         printf("<< End of Game >>");
                     printf("\n");
                     p = p->next;
                 }
            }
        }
    }
    pthread_mutex_destroy(&mutex1);
    pthread_mutex_destroy(&mutex2);
    free(room_list); free(ep_events);
    close(serv_sock); close(ep_fd);
    mysqlQuit();
    return 0;
}
void setnonblockingmode(int fd){
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag|O_NONBLOCK);
}
void error_handling(char* msg){
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}

void* thread_main(void* args){
    char buf[20];
    memset(buf, 0, sizeof(buf));
    int clnt_sock = *((int*)args);
    int msg_len;
    while(1){
        msg_len = read(clnt_sock, buf, 20);
        if(msg_len == 0){
            /* Check remain in Room */
            Room* p = room_list->head;
            while(p != NULL){
                for(int i=0; i<4; i++){
                    if(p->user[i] == clnt_sock){
                        char msg[2];
                        msg[1] = p->num + '0';
                        exit_room(clnt_sock, msg);
                    }
                }
                p = p->next;
            }
            printf("Closed socket : %d\n", clnt_sock);
            epoll_ctl(ep_fd, EPOLL_CTL_DEL, clnt_sock, NULL);
            close(clnt_sock);
            return NULL;
        }
        else if(msg_len < 0){
            if(errno == EAGAIN) // Read All Message
                break;
        }
    }
    printf("%d - msg : %s\n", clnt_sock, buf);
    message_handling(clnt_sock, buf, msg_len);
}

/* Select Function According to Message Type */
void message_handling(int clnt_sock, char *buf, int size){
    TYPE type = buf[0] - '0';
    switch(type){
        case MAKE_ROOM : make_room(clnt_sock, buf); break;
        case JOIN_ROOM : join_room(clnt_sock, buf); break;
        case EXIT_ROOM : exit_room(clnt_sock, buf); break;
        case REFRESH : send_room_list(clnt_sock); break;
        case GAME_START : start_game(buf); break;
        case WORD : input_word(buf); break;
        case HINT : send_hint(clnt_sock, buf); break;
        case JUMP : jump_order(buf); break;
        case BACK : back_order(buf); break;
        case TIME_OUT : end_round(buf); break;
    }
}

/* Send Room List to Client */
void send_room_list(int clnt_sock){
    pthread_mutex_lock(&mutex1);
    int cnt = room_cnt(); 
    char* list = (char*)malloc(cnt*2+1);
    memset(list, 0, sizeof(list));
    list[0] = cnt + '0';
    Room* room = room_list->head;
    for(int i=1; i<cnt+1; i++){
        list[i] = room->num + '0';
        list[i+cnt] = room->user_cnt + '0';
        room = room->next;
    }
    pthread_mutex_unlock(&mutex1);
    write(clnt_sock, list, cnt*2+1); // list : Room_count(1Byte) + Each Room_number(Room_count Bytes)
    printf("%d - send_room_list : %s\n", clnt_sock, list);
    free(list);
}

void make_room(int clnt_sock, char *buf){
    char response; // Fail(0) or Success(1) : 1Byte
    int room_num = buf[1] - '0';

    pthread_mutex_lock(&mutex1);

    /* Check for the same room number */
    if(search_room(room_num) != NULL){
        response = '0';
        write(clnt_sock, &response, 1);
        pthread_mutex_unlock(&mutex1);
        return;
    }

    /* Init New Room */
    Room *newRoom = (Room*)malloc(sizeof(Room));
    newRoom->num = room_num;
    memset(newRoom->user, -1, sizeof(newRoom->user));
    newRoom->user[0] = clnt_sock;
    newRoom->user_cnt = 1;
    newRoom->direction = 1;
    newRoom->leader = 0;
    newRoom->round = 0;
    memset(newRoom->keyword, 0, sizeof(newRoom->keyword));
    memset(newRoom->last_letter, 0, sizeof(newRoom->last_letter));
    newRoom->used_word = (Word_List*)malloc(sizeof(Word_List));
    newRoom->used_word->head = NULL;
    newRoom->used_word->tail = NULL;
    newRoom->next = NULL;
    
    if(room_list->head == NULL) // First Room
        room_list->head = room_list->tail = newRoom;
    else{
        room_list->tail->next = newRoom;
        room_list->tail = newRoom;
    }
    pthread_mutex_unlock(&mutex1);
    response = '1';
    write(clnt_sock, &response, 1);
    printf("%d - send make_room() : %d\n", clnt_sock, room_num);

    response = '0';
    write(clnt_sock, &response, 1);
}

Room* search_room(int room_num){
    Room *p = room_list->head;
    while(p != NULL){
        if(p->num == room_num)
            return p;
        p = p->next;
    }
    return NULL;
}

void delete_room(int room_num){
    Room *pre = room_list->head;
    Room *cur = room_list->head;

    pthread_mutex_lock(&mutex1);
    while(cur != NULL){
        if(cur->num == room_num){
            if(pre == cur){ // First
                room_list->head = cur->next;
                cur->used_word = NULL;
                cur = NULL;
                free(cur->used_word);
                free(cur);
            }
            else if(cur->next == NULL){ // Last
                room_list->tail = pre;
                room_list->tail->next = NULL;
                cur->used_word = NULL;
                cur = NULL;
                free(cur->used_word);
                free(cur);
            }
            else{ // Middle
                pre->next = cur->next;
                cur->used_word = NULL;
                cur = NULL;
                free(cur->used_word);
                free(cur);
            }
        }
        pre = cur;
        cur = cur->next;
    }
    pthread_mutex_unlock(&mutex1);
    printf("delete_room : %d\n", room_num);
}

int room_cnt(){
    Room *p = room_list->head;
    int num = 0;
    while(p != NULL){
        p = p->next;
        num++;
    }
    return num;
}

void join_room(int clnt_sock, char *buf){
    char response[2]; // Fail or Success(0 or 1) + fixOrder(1Byte) : 2Bytes
    int room_num = buf[1] - '0';

    memset(response, 0, 2);
    pthread_mutex_lock(&mutex2);
    Room* room = search_room(room_num);

    /* Nonexistent Room or Room is Full */
    if(room == NULL || room->user_cnt == 4){
        response[0] = '0';
        write(clnt_sock, response, 2);
        pthread_mutex_unlock(&mutex2);
        return;
    }
    
    int i, j;
    /* Existed_user seet check */
    char* existed_user = (char*)malloc(room->user_cnt+1);
    existed_user[0] = room->user_cnt + '0';

    j = 1;
    for(i=0; i<4; i++){
        if(room->user[i] != -1){
            existed_user[j] = i + '0';
            j++;
        }
    }

    /* Searching for empty order */
    for(i=0; i<4; i++){
        if(room->user[i] == -1)
            break;
    }
    room->user[i] = clnt_sock;
    room->user_cnt++;
    pthread_mutex_unlock(&mutex2);

    response[0] = '1';
    response[1] = i + '0';
    write(clnt_sock, response, 2);
    write(clnt_sock, existed_user, room->user_cnt);
    
    char update[4];
    update[0] = '1';
    update[1] = i + '0';
    update[2] = room->user_cnt + '0';
    update[3] = room->leader + '0';

    for(j=0; j<4; j++){
        if(room->user[j] != -1)
            write(room->user[j], update, 4);
    }
    
    printf("join update : %s\n", update);
}

void exit_room(int clnt_sock, char *buf){
    int room_num = buf[1] - '0';
    int i;

    pthread_mutex_lock(&mutex2);
    Room *room = search_room(room_num);
    for(i=0; i<4; i++){
        if(room->user[i] == clnt_sock){
            room->user[i] = -1;
            break;
        }
    }
    room->user_cnt--;
    pthread_mutex_unlock(&mutex2);

    if(room->user_cnt == 0) // Number of User in Room is 0
        delete_room(room_num);
    else{
        if(room->leader == i){ // Room Leader change
            for(int j=0; j<4; j++){
                if(room->user[j] != -1)
                    room->leader = j;
            }
        }
    }

    if(room != NULL){
        char update[4];
        update[0] = '0';
        update[1] = i + '0';
        update[2] = room->user_cnt + '0';
        update[3] = room->leader + '0';
        
        for(int j=0; j<4; j++){
            if(room->user[j] != -1)
                write(room->user[j], update, 4);
        }

        printf("exit update : %s\n", update);
    }

    char dummy[4];
    memset(dummy, '9', 4);
    write(clnt_sock, dummy, 4);
    send_room_list(clnt_sock);
}

void input_word(char *buf){
    char response[11]; // Fail or Success(0 or 1) + next_order(1Byte) + Word(9Bytes) = 11Bytes
    int room_num = buf[1] - '0';
    int user_order = buf[2] - '0';
    char input_word[9];
    strncpy(input_word, buf+3, 9);
    strcpy(response+2, input_word);

    Room* room = search_room(room_num);
    
    /* Check Used Word Last Letter = Input First Letter */
    char first_letter[3];
    strncpy(first_letter, input_word, 3);
    printf("Last_letter : %s\n", room->last_letter);
    printf("First_letter : %s\n", first_letter);

    if(strcmp(first_letter, room->last_letter)){
        response[0] = '0';
        response[1] = user_order + '0';
        printf("L != F : %s\n", response);
        for(int i=0; i<4; i++){
            if(room->user[i] != -1)
                write(room->user[i], response, 11);
        }
        return;
    }
    /* Check Used word */
    Word* p = room->used_word->head;
    while(p != NULL){
        if(!strcmp(p->word, input_word)){
            printf("Used word : %s\n", response);
            response[0] = '0';
            response[1] = user_order + '0';
            for(int i=0; i<4; i++){
                if(room->user[i] != -1)
                    write(room->user[i], response, 11);
            }
            return;
        }
        p = p->next;
    }
    
    /* Check Database */
    if(!wordCompare(input_word)){
        response[0] = '0';
        response[1] = user_order + '0';
        for(int i=0; i<4; i++){
            if(room->user[i] != -1)
                write(room->user[i], response, 11);
        }
        return;
    }
    
    /* Correct Word */
    strncpy(room->last_letter, input_word+6, 3); // Change Last Letter
    add_used_word(room->used_word, input_word);
    
    response[0] = '1';
    int next_order = user_order + room->direction;
    while(1){
        if(next_order == -1)
            next_order = 3;
        else if(next_order == 4)
            next_order = 0;

        if(room->user[next_order] != -1)
            break;
        next_order += room->direction;
    }
    response[1] = next_order +'0';
    printf("Correct Word : %s\n", response);
    for(int i=0; i<4; i++){
        if(room->user[i] != -1)
            write(room->user[i], response, 11);
    }
}
void add_used_word(Word_List* used_word, char* input_word){
    Word* new = (Word*)malloc(sizeof(Word));
    strcpy(new->word, input_word);
    new->next = NULL;

    if(used_word->head == NULL) // First word
        used_word->head = used_word->tail = new;
    else{
        used_word->tail->next = new;
        used_word->tail = new;
    }
}
void start_game(char* buf){
    int room_num = buf[1] - '0';
    char* keyword = roundWord();

    Room* room = search_room(room_num);

    room->round = 1;

    strncpy(room->keyword, keyword, 18);
    strncpy(room->last_letter, room->keyword, 3);

    char dummy[4]; // For End Thread 
    for(int i=0; i<4; i++){
        if(room->user[i] != -1){
            dummy[0] = '3';
            dummy[1] = i + '0';
            dummy[2] = room->user_cnt + '0';
            dummy[3] = room->leader + '0';
            write(room->user[i], dummy, 4);
        }
    }

    for(int i=0; i<4; i++){
        if(room->user[i] != -1)
            write(room->user[i], room->keyword, 18);
    }

    printf("Start Game!!\n");
}
void end_round(char* buf){
    int room_num = buf[1] - '0';
    int order = buf[2] - '0';
    char response[11];
    memset(response, ' ', 11);

    Room* room = search_room(room_num);
    room->round++;
    if(room->round <= 6){
        strncpy(room->last_letter, room->keyword + (room->round-1)*3, 3); // Change Start letter
        response[0] = '2'; // End of Round
    }
    else{
        response[0] = '5'; // End of Game
        memset(room->keyword, 0, 18);
        memset(room->last_letter, 0, 3);
    }
    /* Reset used_word */
    Word* pre = room->used_word->head;
    Word* cur = room->used_word->head;
    while(cur != NULL){
        pre = cur;
        cur = cur->next;
        pre = NULL;
        free(pre);
    }
    room->used_word->head = room->used_word->tail = NULL;
    response[1] = order + '0';
    printf("end_round response : %s\n", response);
    for(int i=0; i<4; i++){
        if(room->user[i] != -1)
            write(room->user[i], response, 11);
    }

    printf("End of Round : %d\n", room->round-1);
}

void send_hint(int clnt_sock, char* buf){
    int room_num = buf[1] - '0';
    char hint[9];
    char start_letter[4];
    memset(start_letter, 0, 4);

    Room* room = search_room(room_num);
    strcpy(start_letter, room->last_letter);

    char* tmp = wordHint(start_letter);
    if(tmp != NULL)
        strncpy(hint, tmp, 9);
    else
        memset(hint, ' ', 9);
    
    write(clnt_sock, hint, 9);
    printf("HINT : %s\n", hint);
}

void jump_order(char* buf){
    int room_num = buf[1] - '0';
    int order = buf[2] - '0';
    char response[11];
    memset(response, ' ', 11);

    Room* room = search_room(room_num);
    response[0] = '3'; // JUMP
    int next_order = order + 2 * room->direction;

    while(1){
        if(room->direction == 1)
            next_order %= 4;
        else{
            if(next_order == -1)
                next_order = 3;
            else if(next_order == -2)
                next_order = 2;
        }

        if(room->user[next_order] != -1)
            break;
        next_order+=room->direction;
    }
    response[1] = next_order + '0';

    if(room->used_word->tail != NULL)
        strncpy(response+2, room->used_word->tail->word, 9);
    
    for(int i=0; i<4; i++){
        if(room->user[i] != -1)
            write(room->user[i], response, 11);
    }
}

void back_order(char* buf){
    int room_num = buf[1] - '0';
    int order = buf[2] - '0';
    char response[11];
    memset(response, ' ', 11);

    Room* room = search_room(room_num);
    response[0] = '4'; // BACK
    room->direction *= -1;
    int next_order = order + room->direction;
    while(1){
        if(next_order == -1)
            next_order = 3;
        else if(next_order == 4)
            next_order = 0;

        if(room->user[next_order] != -1)
           break;
        next_order += room->direction;
    }
    response[1] = next_order + '0';

    if(room->used_word->tail != NULL)
       strncpy(response+2, room->used_word->tail->word, 9);

    for(int i=0; i<4; i++){
        if(room->user[i] != -1)
            write(room->user[i], response, 11);
    }
}
