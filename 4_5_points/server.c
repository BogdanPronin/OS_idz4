#include <semaphore.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define MAX_TASK_COUNT 10

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


struct task tasks[MAX_TASK_COUNT];
struct task for_execute[MAX_TASK_COUNT];
struct task for_check[MAX_TASK_COUNT];
int tasks_count, complete_count = 0;

void initPulls() {
    for (int i = 0; i < tasks_count; ++i) {
        struct task task = {.id = i, .executor_id = -1, .checker_id = -1, .status = NEW};
        tasks[i] = task;
        for_execute[i] = task;
        for_check[i] = task;
    }
}

void printTasksInfo() {
    for (int j = 0; j < tasks_count; ++j) {
        printf("Задача №%d и статусом %d, выполнена программистом №%d, проверена программистом №%d\n", tasks[j].id,
               tasks[j].status, tasks[j].executor_id, tasks[j].checker_id);
    }
}

void getWork(struct response *response, int programmer_id) {
    for (int i = 0; i < tasks_count; ++i) {
        if (tasks[i].status == NEW) {
            printf("Программист №%d нашел задачу №%d для выполнения\n", programmer_id, tasks[i].id);
            response->response_code = NEW_TASK;
            tasks[i].executor_id = programmer_id;
            tasks[i].status = EXECUTING;
            for_execute[tasks[i].id] = tasks[i];
            return;
        } else if (tasks[i].status == FIX && tasks[i].executor_id == programmer_id) {
            printf("Программист №%d исправляет задачу №%d\n", programmer_id, tasks[i].id);
            response->response_code = NEW_TASK;
            tasks[i].status = EXECUTING;
            for_execute[tasks[i].id] = tasks[i];
            return;
        } else if (tasks[i].status == EXECUTED && tasks[i].executor_id != programmer_id) {
            printf("Программист №%d нашел задачу №%d на проверку\n", programmer_id, tasks[i].id);
            response->response_code = CHECK_TASK;
            tasks[i].checker_id = programmer_id;
            tasks[i].status = CHECKING;
            for_check[tasks[i].id] = tasks[i];
            return;
        } else if (tasks[i].executor_id == programmer_id && tasks[i].status == WRONG) {
            printf("Программист №%d должен исправить задачу №%d\n", programmer_id, tasks[i].id);
            response->response_code = FIX_TASK;
            tasks[i].status = FIX;
            for_execute[tasks[i].id] = tasks[i];
            return;
        }
    }
    if (complete_count == tasks_count) {
        response->response_code = FINISH;
    }
}

void sendTask(struct response *response, int programmer_id) {
    struct task curExecute = {-1, -1, -1, -1};
    for (int i = 0; i < tasks_count; ++i) {
        if (for_execute[i].executor_id == programmer_id && for_execute[i].status == EXECUTING) {
            curExecute = for_execute[i];
            break;
        }
    }
    if (curExecute.status != EXECUTING) {
        printf("Задача сейчас не выполняется...\n");
        printf("Статус задачи = %d\n", curExecute.status);
        response->response_code = 10;
        return;
    }
    curExecute.status = EXECUTED;
    tasks[curExecute.id] = curExecute;
}

void sendCheckResult(struct response *response, int programmer_id) {
    struct task curCheck = {-1, -1, -1, -1};
    for (int i = 0; i < tasks_count; ++i) {
        if (for_check[i].checker_id == programmer_id && for_check[i].status == CHECKING) {
            curCheck = for_check[i];
            if (curCheck.status != CHECKING) {
                printf("Задача сейчас не проверяется...\n");
                printf("Статус задачи = %d\n", curCheck.status);
                response->response_code = 10;
                return;
            }
            int8_t result = rand() % 2;
            printf("Результат проверки задачи №%d - %d\n", curCheck.id, result);
            curCheck.status = result == 0 ? WRONG : RIGHT;
            tasks[curCheck.id] = curCheck;
            if (curCheck.status == RIGHT) {
                ++complete_count;
                printf("\n\nКол-во выполненных = %d\n\n", complete_count);
            }
        }
    }
}


int main(int argc, char *argv[]) {
    (void) signal(SIGINT, handleSigInt);

    struct sockaddr_in servaddr, cliaddr;
    unsigned short port;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <Broadcast Port> [Tasks count]\n", argv[0]);
        exit(1);
    }
    port = atoi(argv[1]);
    tasks_count = MAX_TASK_COUNT;
    if (argc > 2) {
        tasks_count = atoi(argv[2]);
        tasks_count = (tasks_count > MAX_TASK_COUNT || tasks_count < 2) ? MAX_TASK_COUNT : tasks_count;
    }

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == -1) {
        perror("socket() failed");
        exit(1);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) {
        perror("bind() failed");
        exit(1);
    }

    struct request request;
    struct response response;

    initPulls();
    printf("pulls have been initialized\n");

    while (complete_count < tasks_count) {
        printf("Server listening...\n");

        socklen_t len = sizeof(cliaddr);
        recvfrom(sock, &request, sizeof(struct request), 0, (struct sockaddr *) &cliaddr, &len);

        printf("Request code: %d\n", request.request_code);
        printf("Programmer ID: %d\n", request.programmer_id);
        printf("\n");

        int programmer_id = request.programmer_id;

        switch (request.request_code) {
            case 0:
                printf("gone to getwork\n");
                getWork(&response, programmer_id);
                break;
            case 1:
                printf("gone to sendtask\n");
                sendTask(&response, programmer_id);
                getWork(&response, programmer_id);
                break;
            case 2:
                printf("gone to sendcheck\n");
                sendCheckResult(&response, programmer_id);
                getWork(&response, programmer_id);
                break;
            default:
                printf("ub request code\n");
                break;
        }
        printf("\n");

        sendto(sock, &response, sizeof(struct response), 0, (struct sockaddr *) &cliaddr, len);
    }

    closeAll();
    return 0;
}