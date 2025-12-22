#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <syslog.h>

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BACKLOG 10
#define BUF_SIZE 1024

static volatile sig_atomic_t keepRunning = 1;
static pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

struct clientInfo {
    int clientFD;
    struct sockaddr_in clientAddr;
};

static void signal_handler(int signo)
{
    (void)signo;
    keepRunning = 0;
}

void *processClientData(void *arg)
{
    struct clientInfo *client = (struct clientInfo *)arg;
    char buffer[BUF_SIZE];
    char packet[BUF_SIZE];
    int packet_len = 0;

    syslog(LOG_INFO, "Accepted connection from %s",
           inet_ntoa(client->clientAddr.sin_addr));

    while (keepRunning) {
        ssize_t bytes = recv(client->clientFD, buffer, sizeof(buffer), 0);
        if (bytes <= 0) {
            break;
        }

        for (ssize_t i = 0; i < bytes; i++) {
            packet[packet_len++] = buffer[i];

            if (buffer[i] == '\n') {
                pthread_mutex_lock(&file_mutex);

                int fd = open(DATA_FILE, O_CREAT | O_RDWR | O_APPEND, 0644);
                write(fd, packet, packet_len);
                lseek(fd, 0, SEEK_SET);

                char sendbuf[BUF_SIZE];
                ssize_t r;
                while ((r = read(fd, sendbuf, sizeof(sendbuf))) > 0) {
                    send(client->clientFD, sendbuf, r, 0);
                }

                close(fd);
                pthread_mutex_unlock(&file_mutex);

                packet_len = 0;
            }
        }
    }

    close(client->clientFD);
    free(client);
    return NULL;
}

int main(int argc, char *argv[])
{
    int daemonize = 0;
    int opt;

    while ((opt = getopt(argc, argv, "d")) != -1) {
        if (opt == 'd') daemonize = 1;
    }

    openlog("aesdsocket", LOG_PID, LOG_USER);

    struct sigaction sa = {0};
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    int sockFD = socket(AF_INET, SOCK_STREAM, 0);
    int optval = 1;
    setsockopt(sockFD, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    bind(sockFD, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    listen(sockFD, BACKLOG);

    if (daemonize) {
        pid_t pid = fork();
        if (pid > 0) exit(0);
        setsid();
        chdir("/");
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        open("/dev/null", O_RDWR);
        dup(0);
        dup(0);
    }

    remove(DATA_FILE);

    while (keepRunning) {
        struct clientInfo *client = malloc(sizeof(*client));
        socklen_t len = sizeof(client->clientAddr);

        client->clientFD = accept(sockFD,
                                  (struct sockaddr *)&client->clientAddr,
                                  &len);
        if (client->clientFD < 0) {
            free(client);
            continue;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, processClientData, client);
        pthread_detach(tid);
    }

    close(sockFD);
    remove(DATA_FILE);
    closelog();
    return 0;
}

