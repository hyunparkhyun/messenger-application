#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")


#define PORT 8080
#define MAX_CLIENTS 100
#define INPUT_LINE 20  // 입력줄 위치, 콘솔 하단쪽

// 메시지 노드 구조체 정의
typedef struct MessageNode {
    int id;
    char message[2000];
    char client_name[50];
    char timestamp[20];
    int is_deleted;
    struct MessageNode* next;
} MessageNode;

// 클라이언트 노드 구조체 정의
typedef struct ClientNode {
    SOCKET socket;
    int id;
    char name[50];
    MessageNode* message_head;
    struct ClientNode* next;
} ClientNode;

//전역 변수
ClientNode* head = NULL;    //클라이언트 리스트의 헤드
CRITICAL_SECTION client_list_cs;    //임계 구역
int client_id_counter = 1;  //클라이언트 ID카운터
int message_id_counter = 1; //메시지 ID 카운터
int console_line = 0; // 콘솔에서 출력할 라인 번호

//콘솔 커서를 지정한 위치로 이동시키는 함수
void gotoxy(int x, int y) {
    COORD coord;
    coord.X = x;
    coord.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

//새로운 클라이언트를 추가하는 함수
void add_client(SOCKET client_socket, char* name) {
    EnterCriticalSection(&client_list_cs);

    ClientNode* new_node = (ClientNode*)malloc(sizeof(ClientNode));
    new_node->socket = client_socket;
    new_node->id = client_id_counter++;
    strncpy(new_node->name, name, sizeof(new_node->name) - 1);
    new_node->name[sizeof(new_node->name) - 1] = '\0';
    new_node->message_head = NULL;
    new_node->next = head;
    head = new_node;

    LeaveCriticalSection(&client_list_cs);
}

//클라이언트를 리스트에서 제거하는 함수
void remove_client(SOCKET client_socket) {
    EnterCriticalSection(&client_list_cs);

    ClientNode* current = head;
    ClientNode* prev = NULL;

    while (current != NULL && current->socket != client_socket) {
        prev = current;
        current = current->next;
    }

    if (current == NULL) {
        LeaveCriticalSection(&client_list_cs);
        return;
    }

    if (prev == NULL) {
        head = current->next;
    }
    else {
        prev->next = current->next;
    }

    MessageNode* message_current = current->message_head;
    while (message_current != NULL) {
        MessageNode* temp = message_current;
        message_current = message_current->next;
        free(temp);
    }

    free(current);
    LeaveCriticalSection(&client_list_cs);
}

//특정 클라이언트에 메시지를 추가하는 함수
void add_message_to_client(int client_id, char* client_name, char* message) {
    EnterCriticalSection(&client_list_cs);

    ClientNode* current = head;
    while (current != NULL) {
        if (current->id == client_id) {
            MessageNode* new_message = (MessageNode*)malloc(sizeof(MessageNode));
            new_message->id = message_id_counter++;
            strcpy_s(new_message->message, sizeof(new_message->message), message);
            strcpy_s(new_message->client_name, sizeof(new_message->client_name), client_name);
            new_message->is_deleted = 0;

            time_t now = time(NULL);
            struct tm* t = localtime(&now);
            strftime(new_message->timestamp, sizeof(new_message->timestamp), "%Y-%m-%d %H:%M:%S", t);

            new_message->next = current->message_head;
            current->message_head = new_message;
            break;
        }
        current = current->next;
    }

    LeaveCriticalSection(&client_list_cs);
}

//메시지를 수정하는 함수
void modify_message(int message_id, char* new_message) {
    EnterCriticalSection(&client_list_cs);

    ClientNode* client_current = head;
    while (client_current != NULL) {
        MessageNode* message_current = client_current->message_head;
        while (message_current != NULL) {
            if (message_current->id == message_id && !message_current->is_deleted) {
                strcpy_s(message_current->message, sizeof(message_current->message), new_message);
                break;
            }
            message_current = message_current->next;
        }
        client_current = client_current->next;
    }

    LeaveCriticalSection(&client_list_cs);
}

//메시지를 삭제하는 함수
void delete_message(int message_id) {
    EnterCriticalSection(&client_list_cs);

    ClientNode* client_current = head;
    while (client_current != NULL) {
        MessageNode* message_current = client_current->message_head;
        while (message_current != NULL) {
            if (message_current->id == message_id) {
                message_current->is_deleted = 1;
                break;
            }
            message_current = message_current->next;
        }
        client_current = client_current->next;
    }

    LeaveCriticalSection(&client_list_cs);
}

//메시지를 복원하는 함수
void restore_message(int message_id) {
    EnterCriticalSection(&client_list_cs);

    ClientNode* client_current = head;
    while (client_current != NULL) {
        MessageNode* message_current = client_current->message_head;
        while (message_current != NULL) {
            if (message_current->id == message_id) {
                message_current->is_deleted = 0;
                break;
            }
            message_current = message_current->next;
        }
        client_current = client_current->next;
    }

    LeaveCriticalSection(&client_list_cs);
}

//특정 클라이언트를 제외한 모든 클라이언트에게 메시지를 브로드캐스트하는 함수
void broadcast_message(char* message, SOCKET exclude_socket) {
    EnterCriticalSection(&client_list_cs);

    ClientNode* current = head;
    while (current != NULL) {
        if (current->socket != exclude_socket) {
            send(current->socket, message, strlen(message), 0);
        }
        current = current->next;
    }

    LeaveCriticalSection(&client_list_cs);
}

//특정 클라이언트의 모든 메시지를 출력하는 함수
void print_all_messages_for_client(char* client_name) {
    EnterCriticalSection(&client_list_cs);

    ClientNode* current = head;
    while (current != NULL) {
        if (strcmp(current->name, client_name) == 0) {
            MessageNode* message_current = current->message_head;
            gotoxy(0, console_line++);  // 현재 라인에 출력 후 라인 증가
            printf(" 클라이언트 %s (%d) 가 보낸 메시지:\n", client_name, current->id);
            while (message_current != NULL) {
                if (!message_current->is_deleted) {
                    gotoxy(0, console_line++);  // 현재 라인에 출력 후 라인 증가
                    printf("[%d][%s] %s\n", message_current->id, message_current->timestamp, message_current->message);
                }
                message_current = message_current->next;
            }
            break;
        }
        current = current->next;
    }

    LeaveCriticalSection(&client_list_cs);
}

//특정 클라이언트의 메시지 중 특정 문자열을 포함한 메시지를 검색하는 함수
void search_message(int client_id, const char* message_part) {
    EnterCriticalSection(&client_list_cs);

    ClientNode* client_current = head;
    while (client_current != NULL) {
        if (client_current->id == client_id) {
            MessageNode* message_current = client_current->message_head;
            while (message_current != NULL) {
                if (strstr(message_current->message, message_part) != NULL && !message_current->is_deleted) {
                    gotoxy(0, console_line++);  // 현재 라인에 출력 후 라인 증가
                    printf("메시지 number: %d, 시간: %s, 클라이언트: %s, 메시지: %s\n",
                        message_current->id, message_current->timestamp, message_current->client_name, message_current->message);
                }
                message_current = message_current->next;
            }
            break;
        }
        client_current = client_current->next;
    }

    LeaveCriticalSection(&client_list_cs);
}

//클라이언트 이름으로 클라이언트 ID를 얻는 함수
int get_client_id_by_name(const char* client_name) {
    EnterCriticalSection(&client_list_cs);

    ClientNode* current = head;
    while (current != NULL) {
        if (strcmp(current->name, client_name) == 0) {
            int id = current->id;
            LeaveCriticalSection(&client_list_cs);
            return id;
        }
        current = current->next;
    }

    LeaveCriticalSection(&client_list_cs);
    return -1;
}
//모든 클라이언트의 모든 메시지를 출력하는 함수
void print_all_messages() {
    EnterCriticalSection(&client_list_cs);

    ClientNode* current_client = head;
    while (current_client != NULL) {
        MessageNode* current_message = current_client->message_head;
        while (current_message != NULL) {
            if (!current_message->is_deleted) {
                printf("[%d][%s] %s\n", current_message->id, current_message->timestamp, current_message->message);
            }
            current_message = current_message->next;
        }
        current_client = current_client->next;
    }

    LeaveCriticalSection(&client_list_cs);
}
//클라이언트 핸들러 함수
DWORD WINAPI handle_client(LPVOID socket_desc) {
    SOCKET sock = *(SOCKET*)socket_desc;
    char message[2000]; //메시지를 저장한 변수
    int read_size;      //읽은 메시지의 크기를 저장할 변수
    char client_name[50];//클라이언트 이름을 저장할 변수

    //클라이언트 이름을 받음
    if ((read_size = recv(sock, client_name, sizeof(client_name), 0)) == SOCKET_ERROR) {
        gotoxy(0, console_line++);  // 현재 라인에 출력 후 라인 증가
        printf("에러 발생: %d\n", WSAGetLastError());
        return 1;
    }
    client_name[read_size] = '\0';

    //리스트에 클라이언트를 추가함
    add_client(sock, client_name);
    gotoxy(0, console_line++);  // 현재 라인에 출력 후 라인 증가
    printf("클라이언트 %s 님이 연결되었습니다.\n", client_name);

    //클라이언트의 ID를 가져옴
    int client_id = get_client_id_by_name(client_name);
    if (client_id == -1) {
        gotoxy(0, console_line++);  // 현재 라인에 출력 후 라인 증가
        printf("클라이언트 ID를 찾을 수 없습니다 이름: %s\n", client_name);
        return 1;
    }

    //클라이언트로부터 메시지를 반복적으로 가져옴
    while ((read_size = recv(sock, message, sizeof(message), 0)) > 0) {
        message[read_size] = '\0';

        //메시지 수정
        if (strncmp(message, "modify ", 7) == 0) {
            int message_id;
            char new_message[2000];
            sscanf(message, "modify %d %[^\n]", &message_id, new_message);
            modify_message(message_id, new_message);
            gotoxy(0, console_line++);  // 현재 라인에 출력 후 라인 증가
            printf("메시지 %d 가 %s 님으로부터 수정되었습니다 ==> : %s\n", message_id, client_name, new_message);
        }
        //메시지 삭제
        else if (strncmp(message, "delete ", 7) == 0) {
            int message_id;
            sscanf(message, "delete %d", &message_id);
            delete_message(message_id);
            gotoxy(0, console_line++);  // 현재 라인에 출력 후 라인 증가
            printf("메시지 %d가 %s 님으로부터 삭제되었습니다\n", message_id, client_name);
        }
        //메시지 복원
        else if (strncmp(message, "restore ", 8) == 0) {
            int message_id;
            sscanf(message, "restore %d", &message_id);
            restore_message(message_id);
            gotoxy(0, console_line++);  // 현재 라인에 출력 후 라인 증가
            printf("메시지 %d가 %s님으로부터 복원되었습니다 \n", message_id, client_name);
        }
        //메시지 서칭
        else if (strncmp(message, "search ", 7) == 0) {
            char search_term[2000];
            sscanf(message, "search %[^\n]", search_term);
            gotoxy(0, console_line++);  // 현재 라인에 출력 후 라인 증가
            printf(" %s님으로부터 '%s' 가 포함된 메시지를 찾았습니다 :\n", search_term,client_name );
            search_message(client_id, search_term);
        }
        //일반 메시지에 대한 처리
        else {
            add_message_to_client(client_id, client_name, message);
            gotoxy(0, console_line++);  // 현재 라인에 출력 후 라인 증가
            printf("%s\n", message);
            broadcast_message(message, sock);
        }
    }
    //클라이언트 연결 해제됐을때
    if (read_size == 0) {
        gotoxy(0, console_line++);  // 현재 라인에 출력 후 라인 증가
        printf("클라이언트 %s가 연결 해제되었습니다\n", client_name);
    }
    else if (read_size == SOCKET_ERROR) {
        printf("연결 해제: %d\n", WSAGetLastError());
    }
    //클라이언트를 리스트에서 제거하고 소켓을 닫음
    remove_client(sock);
    closesocket(sock);
    free(socket_desc);
    return 0;
}

//서버에서 하는 입력에 따른 기능 구현
DWORD WINAPI handle_server_input(LPVOID lpParam) {
    char input[2000];
    while (1) {
        gotoxy(0, INPUT_LINE);
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = '\0';  // 개행 문자 제거

        //특정 클라이언트의 메시지 출력 요청
        if (strncmp(input, "print ", 6) == 0) {
            char client_name[50];
            sscanf(input, "print %s", client_name);
            print_all_messages_for_client(client_name);
        }
        //특정 클라이언트의 메시지 서칭 요청
        else if (strncmp(input, "search ", 7) == 0) {
            char client_name[50];
            char search_term[2000];
            sscanf(input, "search %s %[^\n]", client_name, search_term);
            int client_id = get_client_id_by_name(client_name);
            if (client_id != -1) {
                gotoxy(0, console_line++);  // 현재 라인에 출력 후 라인 증가
                printf("  %s님으로 부터 '%s' 가 포함된 메시지를 찾았습니다:\n", client_name, search_term);
                search_message(client_id, search_term);
            }
            else {
                gotoxy(0, console_line++);  // 현재 라인에 출력 후 라인 증가
                printf("클라이언트 %s를 찾을 수 없습니다.\n", client_name);
            }
        }
        //모든 클라이언트의 메시지를 출력
        else if (strcmp(input, "all_message") == 0) {
            print_all_messages();
        }
        //콘솔 화면 초기화
        else if (strcmp(input, "clear") == 0) {
            system("cls");
            gotoxy(0, 17);
            printf("******************입력하는 공간입니다******************\n");
            printf("******************공간을 침범하면 clear로 지워주세요***\n");
            console_line = 0;
        }
        //그 외 입력에 대한 처리
        else {
            gotoxy(0, console_line++);  // 현재 라인에 출력 후 라인 증가
            printf("서버 input: %s\n", input);
        }
    }
    return 0;
}


int main() {
    SetConsoleTitle(TEXT("현카오톡")); //콘솔 이름  

    //구조체 초기화
    WSADATA wsa;
    SOCKET server_socket, client_socket;
    struct sockaddr_in server, client;
    int c = sizeof(struct sockaddr_in);
    InitializeCriticalSection(&client_list_cs);

    printf("Winsock을 사용할 수 있도록 초기화 중입니다...\n");
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("실패. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    //소켓 생성
    printf("소켓을 생성중입니다...\n");
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("소켓을 생성하지 못했습니다. failed with error: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }


    //소켓 바인딩(소켓을 지정한 주소와 포트에 바인딩)
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    printf("소켓을 바인딩중입니다...\n");
    if (bind(server_socket, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        printf("바인딩에 실패했습니다.failed with error: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    printf("요청받을 수 있는 상태 대기중...\n");
    listen(server_socket, MAX_CLIENTS);

    //서버 입력 처리 쓰레드 생성
    HANDLE server_input_thread = CreateThread(NULL, 0, handle_server_input, NULL, 0, NULL);

    printf("접속 대기중...\n");
    while ((client_socket = accept(server_socket, (struct sockaddr*)&client, &c)) != INVALID_SOCKET) {
        printf("연결되었습니다.\n");

        SOCKET* new_sock = (SOCKET*)malloc(sizeof(SOCKET));
        *new_sock = client_socket;

        HANDLE client_thread = CreateThread(NULL, 0, handle_client, (LPVOID)new_sock, 0, NULL);
        if (client_thread == NULL) {
            printf("쓰레드 오류 : %d\n", GetLastError());
            closesocket(client_socket);
            free(new_sock);
        }
        else {
            CloseHandle(client_thread);
        }
    }

    if (client_socket == INVALID_SOCKET) {
        printf("승인 에러: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    WaitForSingleObject(server_input_thread, INFINITE);
    CloseHandle(server_input_thread);

    DeleteCriticalSection(&client_list_cs);
    closesocket(server_socket);
    WSACleanup();

    return 0;
}


