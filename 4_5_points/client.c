#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

enum REQUEST_CODE {
    GET_WORK = 0,
    SEND_TASK = 1,
    SEND_CHECK = 2
};

enum RESPONSE_CODE {
    UB = -1,
    NEW_TASK = 0,
    CHECK_TASK = 1,
    FIX_TASK = 2,
    FINISH = 3
};

enum STATUS {
    NEW = -1,
    EXECUTING = 0,
    EXECUTED = 1,
    CHECKING = 2,
    WRONG = 3,
    RIGHT = 4,
    FIX = 5
};

struct task {
    int id;
    int executor_id;
    int checker_id;
    int status;
};

struct request {
    int request_code;
    int programmer_id;
};

struct response {
    int response_code;
};

int sock;

void closeAll() {
    close(sock);
}

void handleSigInt(int sig) {
    if (sig != SIGINT) {
        return;
    }
    closeAll();
    exit(0);
}

int main(int argc, char *argv[]) {
    (void) signal(SIGINT, handleSigInt);

    struct sockaddr_in servaddr;
    char *ip;
    unsigned short port;
    int broadcastPermission;

    if (argc < 4) {
        fprintf(stderr, "Usage: %s <IP Address> <Port> <Programmer ID>\n", argv[0]);
        exit(1);
    }
    ip = argv[1];
    port = atoi(argv[2]);
    int programmer_id = atoi(argv[3]);

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == -1) {
        perror("socket() failed");
        exit(1);
    }

    broadcastPermission = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *) &broadcastPermission, sizeof(broadcastPermission)) < 0) {
        perror("setsockopt() failed");
        exit(1);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(ip);

    struct request request;
    request.request_code = GET_WORK;
    request.programmer_id = programmer_id;
    struct response response;

    for (;;) {
        if (sendto(sock, &request, sizeof(struct request), 0, (struct sockaddr *) &servaddr, sizeof(servaddr)) !=
            sizeof(request)) {
            perror("sendto()ЩЫ failed");
            exit(1);
        }
        printf("programmer#%d has sent request with code = %d\n", request.programmer_id, request.request_code);

        if (recvfrom(sock, &response, sizeof(struct response), 0, NULL, NULL) != sizeof(response)) {
            perror("recvfrom() failed");
            exit(1);
        }
        printf("response code: %d\n", response.response_code);

        if (response.response_code == FINISH) {
            break;
        }
        switch (response.response_code) {
            case UB:
                break;
            case NEW_TASK:
                sleep(3);
                request.request_code = SEND_TASK;
                break;
            case CHECK_TASK:
                sleep(3);
                request.request_code = SEND_CHECK;
                break;
            case FIX_TASK:
                sleep(3);
                request.request_code = SEND_TASK;
                break;
            case FINISH:
                close(sock);
                exit(0);
            default:
                request.request_code = GET_WORK;
                break;
        }
        sleep(5);
    }

    close(sock);
    exit(0);
}