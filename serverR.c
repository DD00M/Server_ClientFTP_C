#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/sendfile.h>
#include <dirent.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#define PORT 8080
#define BUFSIZE 1024
#define num_files 1024
#define num_lines 256
#define nr_of_words 10

#define isLIST 1
#define isGET 2
#define isPUT 3
#define isDELETE 4
#define isUPDATE 5
#define isSEARCH 6

#define logFile "./log.txt"

int number_of_files = 0;
int number_of_readFiles = 0;

int control_operationValue = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_signal = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_signal = PTHREAD_COND_INITIALIZER;
int condition_spin = 0;
int signal_condition = 0;
int sigsegv_condition = 0;
int flag = 0;

time_t current_time;

char *files[num_files];
int size_files[num_files];

char *readFiles[num_files];

char buffer[BUFSIZE] = {0};

//return value for join
int val = -999;

//search word
char *toSearchWord;

// update vars
char *filename;
off_t offset_update;
int sizeFile_update;

//thread index:
pthread_t th_id_spin;
pthread_t tid;

int server_fd, new_socket;

int counter = 0;

bool take_only_ExtFile(char *strr)
{
    if (strstr(strr, ".") != NULL || strstr(strr, ".exe") != NULL || strstr(strr, ".o") != NULL || strstr(strr, ".i") != NULL)
    {
        return true;
    }
    return false;
}

int get_files()
{
    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    int i = 0;
    int j = 0;
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            if (dir->d_type == DT_REG)
            {
                files[i] = strdup(dir->d_name);

                struct stat st;
                stat(files[i], &st);
                size_files[i] = st.st_size;

                if (take_only_ExtFile(files[i]) == true)
                {
                    readFiles[j] = strdup(files[i]);
                    j++;
                }
                i++;
            }
        }
        closedir(d);
    }
    number_of_files = i;
    number_of_readFiles = j;
    return i;
}

char *get_result(char *my_str, int statusValue)
{
    char *myStr = (char *)malloc(BUFSIZE * sizeof(char));
    char *status = NULL;
    if (statusValue == 0)
    {
        status = strdup("Success");
    }
    else
        status = strdup("Failed");

    strcpy(myStr, status);
    strcat(myStr, "!");

    char result_integer[50];
    sprintf(result_integer, "%ld", sizeof(myStr));

    strcat(myStr, result_integer);
    strcat(myStr, "!");
    strcat(myStr, my_str);

    return myStr;
}

char **get_string_splitted(char *input, const char *chr)
{
    char **toRet = (char **)malloc(num_files * sizeof(char *));
    for (int i = 0; i < num_files; i++)
    {
        toRet[i] = NULL;
    }
    int k = 0;
    char *rest = strdup(input);
    char *token = strtok_r(input, chr, &rest);

    while (token != NULL)
    {
        toRet[k] = strdup(token);
        k++;
        token = strtok_r(NULL, chr, &rest);
    }
    return toRet;
}

void *pthread_GET(int new_socket)
{
    pthread_mutex_lock(&mutex);

    void *result;
    // printf("before split\n");
    char *myBuf = (char *)malloc(sizeof(buffer) * sizeof(char));
    strcpy(myBuf, buffer);
    char **split = get_string_splitted(myBuf, "!");
    // printf("after split\n");
    // printf("%s-%s-%s\n", split[0], split[1], split[2]);
    filename = NULL;
    filename = strdup(split[2]);
    control_operationValue = isGET;
    char *auxBuffer = (char *)malloc(BUFSIZE * sizeof(char));
    memset(auxBuffer, 0, sizeof(auxBuffer));
    if (access(filename, F_OK) == 0)
    {
        int in_fd = open(filename, O_RDONLY);

        struct stat st;
        stat(filename, &st);
        int sizeFile = st.st_size;
        char result_integer[50];
        sprintf(result_integer, "%d", sizeFile);
        strcpy(auxBuffer, "Success");
        strcat(auxBuffer, "!");
        strcat(auxBuffer, result_integer);
        // printf("buffer primul send: %s\n", auxBuffer);
        send(new_socket, auxBuffer, BUFSIZE, 0);
        ;
        sendfile(new_socket, in_fd, 0, sizeFile + 1);
        // send(new_socket, myStr, strlen(myStr), 0);

        close(in_fd);
        pthread_mutex_unlock(&mutex);
    }
    else
    {
        char result_integer[50];
        sprintf(result_integer, "%d", 0);
        strcpy(auxBuffer, "Failed");
        strcat(auxBuffer, "!");
        strcat(auxBuffer, result_integer);
        send(new_socket, auxBuffer, sizeof(auxBuffer), 0);
        printf("File not found\n");
        pthread_mutex_unlock(&mutex);
        return NULL;
    }
}

void *pthread_LS(int new_socket)
{
    pthread_mutex_lock(&mutex);

    int nr_of_files = get_files();
    int total_length = 0;

    printf("Numarul de fisiere este: %d\n", nr_of_files);

    // for (int i = 0; i < nr_of_files; i++)
    // {
    //    printf("%s\n", files[i]);
    //}

    char *return_list = (char *)malloc(BUFSIZE * sizeof(char));
    char *return_IntList = (char *)malloc(BUFSIZE * sizeof(char));
    strcpy(return_list, files[0]);
    strcat(return_list, "\\0");

    char result_integer[10];
    sprintf(result_integer, "%d", size_files[0]);
    //printf("%s-%d\n", result_integer, size_files[0]);
    strcpy(return_IntList, result_integer);
    strcat(return_IntList, "!");
    for (int i = 1; i < nr_of_files; i++)
    {
        memset(result_integer, 0 , 10);
        strcat(return_list, files[i]);
        strcat(return_list, "\\0");
        sprintf(result_integer, "%d", size_files[i]);
        //printf("%s-%d\n", result_integer, size_files[i]);
        strcat(return_IntList, result_integer);
        strcat(return_IntList, "!");
    }

    pthread_mutex_unlock(&mutex);

    char *myStr = get_result(return_list, 0);
    send(new_socket, myStr, 1024, 0);
    printf("ints: %s\n", return_IntList);
    send(new_socket, return_IntList, 1024, 0);
    memset(buffer, 0, sizeof(buffer));
}

void *pthread_DELETE(int new_socket)
{
    // printf("0\n");
    pthread_mutex_lock(&mutex);
    char status[32] = {0};
    if (access(filename, F_OK) == 0)
    {
        //printf("before remove\n");
        int x;
        x = remove(filename);
        //printf("after remove\n");
        if (x == 0)
        {
            //printf("succes\n");
            strcpy(status, "Success");
            send(new_socket, status, sizeof(status), 0);
            printf("a dat send\n");
        }
        else
        {
            strcpy(status, "Failed");
            send(new_socket, status, sizeof(status), 0);
            pthread_mutex_unlock(&mutex);
            return NULL;
        }
    }
    else
    {
        strcpy(status, "Failed");
        send(new_socket, status, sizeof(status), 0);
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    // printf("a terminat\n");
    pthread_mutex_unlock(&mutex);
    return NULL;
}

void *pthread_exec_func(void *arg)
{
    while (1)
    {
        int new_socket = *((int *)arg);
        memset(buffer, 0, sizeof(buffer));
        //printf("ZZZZZZZZZZZZZZZZZZZZZZZZZZ: %d\n", signal_condition);
        //printf("SALUT\n");
        int valread = recv(new_socket, buffer, BUFSIZE, 0);
        printf("Client %d sent: %s\n", counter, buffer);
        char *myBuf = (char *)malloc(sizeof(buffer) * sizeof(char));
        // printf("x\n");
        strcpy(myBuf, buffer);
        char **split = get_string_splitted(myBuf, "!");
        if (strcmp(split[0], "LS") == 0 || strcmp(split[0], "ls") == 0)
        {
            printf("a intrat pe list\n");
            // return
            pthread_LS(new_socket);
        }
        else if (strcmp(split[0], "GET") == 0 || strcmp(split[0], "get") == 0)
        {
            pthread_mutex_lock(&mutex);
            filename = NULL;
            filename = strdup(split[2]);
            // printf("a intrat pe get\n");
            pthread_mutex_unlock(&mutex);
            // return
            pthread_GET(new_socket);
        }
        else if (strcmp(split[0], "DELETE") == 0 || strcmp(split[0], "delete") == 0)
        {
            // printf("0x\n");
            pthread_mutex_lock(&mutex);
            filename = NULL;
            // printf("1x\n");
            filename = strdup(split[2]);
            // printf("2x\n");
            pthread_mutex_unlock(&mutex);
            // printf("a intrat in delete\n");
            // return
            pthread_DELETE(new_socket);
        }
        else if (strcmp(split[0], "BYE") == 0 || strcmp(split[0], "bye") == 0){
            char *buffy = (char*)malloc(32 * sizeof(char));
            strcpy(buffy, "Disconnect");

            send(new_socket, buffy, 32, 0);
            shutdown(new_socket, SHUT_RDWR);
            //shutdown(server_fd, SHUT_RDWR);

            pthread_exit(&tid);
            exit(EXIT_SUCCESS);

        }
        else
            return NULL;
    }
}

int main(int argc, char const *argv[])
{
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    for (int i = 0; i < num_files; i++)
    {
        files[i] = NULL;
    }

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    for (int i = 0; i < number_of_files; i++)
    {
        printf("%d-%s\n", i, files[i]);
    }

    // pthread_join(th_id_spin, NULL);
    //   Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    printf("Server On\n");
    while (1)
    {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        
        }
        counter++;
        
        printf("Client %d connected\n", counter);
        pthread_create(&tid, NULL, pthread_exec_func, (void *)&new_socket);
        printf("Thread %d created\n", counter);
        memset(buffer, 0, sizeof(buffer));
    }
    close(new_socket);
    shutdown(server_fd, SHUT_RDWR);
    return 0;
}