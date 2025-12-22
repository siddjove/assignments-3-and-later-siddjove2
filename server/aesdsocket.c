#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <syslog.h>

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"
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

static int server_init(void)
{
    int sockfd;
    struct sockaddr_in addr;
    int opt = 1;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        syslog(LOG_ERR, "socket failed");
        return -1;
    }

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        syslog(LOG_ERR, "bind failed");
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, 10) < 0) {
        syslog(LOG_ERR, "listen failed");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

static void *processClientData(void *arg)
{
    struct clientInfo *client = arg;
    char recvbuf[BUF_SIZE];
    char accum[BUF_SIZE * 10];
    size_t accum_len = 0;

    while (keepRunning) {
        ssize_t r = recv(client->clientFD, recvbuf, sizeof(recvbuf), 0);
        if (r <= 0) break;

        memcpy(accum + accum_len, recvbuf, r);
        accum_len += r;

        char *newline;
        while ((newline = memchr(accum, '\n', accum_len)) != NULL) {
            size_t line_len = newline - accum + 1;

            pthread_mutex_lock(&file_mutex);

            int fd = open(DATA_FILE, O_CREAT | O_WRONLY | O_APPEND, 0644);
            write(fd, accum, line_len);
            close(fd);

            fd = open(DATA_FILE, O_RDONLY);
            char sendbuf[BUF_SIZE];
            ssize_t s;
            while ((s = read(fd, sendbuf, sizeof(sendbuf))) > 0) {
                send(client->clientFD, sendbuf, s, 0);
            }
            close(fd);

            pthread_mutex_unlock(&file_mutex);

            memmove(accum, accum + line_len, accum_len - line_len);
            accum_len -= line_len;
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

    struct sigaction sa = {0};
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    openlog("aesdsocket", LOG_PID, LOG_USER);

    int sockfd = server_init();
    if (sockfd < 0) exit(EXIT_FAILURE);

    if (daemonize) {
        if (fork() > 0) exit(0);
        setsid();
        chdir("/");
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    remove(DATA_FILE);

    while (keepRunning) {
        struct clientInfo *client = malloc(sizeof(*client));
        socklen_t len = sizeof(client->clientAddr);

        client->clientFD = accept(sockfd,
            (struct sockaddr *)&client->clientAddr, &len);

        if (client->clientFD < 0) {
            free(client);
            continue;
        }

        syslog(LOG_INFO, "Accepted connection from %s",
               inet_ntoa(client->clientAddr.sin_addr));

        pthread_t tid;
        pthread_create(&tid, NULL, processClientData, client);
        pthread_detach(tid);
    }

    close(sockfd);
    remove(DATA_FILE);
    closelog();
    return 0;
}

