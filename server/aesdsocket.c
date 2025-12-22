#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <syslog.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define MAX_PACKET_SIZE 1024

static volatile sig_atomic_t keepRunning = 1;

struct clientInfo {
    int clientFD;
    struct sockaddr_in clientAddr;
};

void signal_handler(int signo)
{
    (void)signo;
    keepRunning = 0;
}

void *processClientData(void *arg)
{
    struct clientInfo *client = (struct clientInfo *)arg;
    char buffer[MAX_PACKET_SIZE];
    ssize_t bytes;

    int fd = open(DATA_FILE, O_CREAT | O_RDWR | O_APPEND, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "File open failed");
        close(client->clientFD);
        free(client);
        return NULL;
    }

    while ((bytes = recv(client->clientFD, buffer, sizeof(buffer), 0)) > 0) {

        write(fd, buffer, bytes);

        if (memchr(buffer, '\n', bytes)) {
            lseek(fd, 0, SEEK_SET);

            char sendbuf[MAX_PACKET_SIZE];
            ssize_t read_bytes;
            while ((read_bytes = read(fd, sendbuf, sizeof(sendbuf))) > 0) {
                send(client->clientFD, sendbuf, read_bytes, 0);
            }
        }
    }

    syslog(LOG_INFO, "Closed connection from %s",
           inet_ntoa(client->clientAddr.sin_addr));

    close(fd);
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

    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    int sockFD = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFD < 0) {
        syslog(LOG_ERR, "Socket creation failed");
        return -1;
    }

    int reuse = 1;
    setsockopt(sockFD, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockFD, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        syslog(LOG_ERR, "Bind failed");
        close(sockFD);
        return -1;
    }

    if (listen(sockFD, 10) < 0) {
        syslog(LOG_ERR, "Listen failed");
        close(sockFD);
        return -1;
    }

    if (daemonize) {
        pid_t pid = fork();
        if (pid < 0) exit(EXIT_FAILURE);
        if (pid > 0) exit(EXIT_SUCCESS);

        setsid();
        chdir("/");
        umask(0);

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    remove(DATA_FILE);

    while (keepRunning) {
        struct clientInfo *client = malloc(sizeof(struct clientInfo));
        socklen_t addrlen = sizeof(client->clientAddr);

        client->clientFD = accept(sockFD,
                                  (struct sockaddr *)&client->clientAddr,
                                  &addrlen);

        if (client->clientFD < 0) {
            free(client);
            if (errno == EINTR) break;
            continue;
        }

        syslog(LOG_INFO, "Accepted connection from %s",
               inet_ntoa(client->clientAddr.sin_addr));

        pthread_t tid;
        pthread_create(&tid, NULL, processClientData, client);
        pthread_detach(tid);
    }

    close(sockFD);
    remove(DATA_FILE);
    closelog();
    return 0;
}

