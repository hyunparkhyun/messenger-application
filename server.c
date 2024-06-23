#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")


#define PORT 8080
#define MAX_CLIENTS 100
#define INPUT_LINE 20  // �Է��� ��ġ, �ܼ� �ϴ���

// �޽��� ��� ����ü ����
typedef struct MessageNode {
    int id;
    char message[2000];
    char client_name[50];
    char timestamp[20];
    int is_deleted;
    struct MessageNode* next;
} MessageNode;

// Ŭ���̾�Ʈ ��� ����ü ����
typedef struct ClientNode {
    SOCKET socket;
    int id;
    char name[50];
    MessageNode* message_head;
    struct ClientNode* next;
} ClientNode;

//���� ����
ClientNode* head = NULL;    //Ŭ���̾�Ʈ ����Ʈ�� ���
CRITICAL_SECTION client_list_cs;    //�Ӱ� ����
int client_id_counter = 1;  //Ŭ���̾�Ʈ IDī����
int message_id_counter = 1; //�޽��� ID ī����
int console_line = 0; // �ֿܼ��� ����� ���� ��ȣ

//�ܼ� Ŀ���� ������ ��ġ�� �̵���Ű�� �Լ�
void gotoxy(int x, int y) {
    COORD coord;
    coord.X = x;
    coord.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

//���ο� Ŭ���̾�Ʈ�� �߰��ϴ� �Լ�
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

//Ŭ���̾�Ʈ�� ����Ʈ���� �����ϴ� �Լ�
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

//Ư�� Ŭ���̾�Ʈ�� �޽����� �߰��ϴ� �Լ�
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

//�޽����� �����ϴ� �Լ�
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

//�޽����� �����ϴ� �Լ�
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

//�޽����� �����ϴ� �Լ�
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

//Ư�� Ŭ���̾�Ʈ�� ������ ��� Ŭ���̾�Ʈ���� �޽����� ��ε�ĳ��Ʈ�ϴ� �Լ�
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

//Ư�� Ŭ���̾�Ʈ�� ��� �޽����� ����ϴ� �Լ�
void print_all_messages_for_client(char* client_name) {
    EnterCriticalSection(&client_list_cs);

    ClientNode* current = head;
    while (current != NULL) {
        if (strcmp(current->name, client_name) == 0) {
            MessageNode* message_current = current->message_head;
            gotoxy(0, console_line++);  // ���� ���ο� ��� �� ���� ����
            printf(" Ŭ���̾�Ʈ %s (%d) �� ���� �޽���:\n", client_name, current->id);
            while (message_current != NULL) {
                if (!message_current->is_deleted) {
                    gotoxy(0, console_line++);  // ���� ���ο� ��� �� ���� ����
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

//Ư�� Ŭ���̾�Ʈ�� �޽��� �� Ư�� ���ڿ��� ������ �޽����� �˻��ϴ� �Լ�
void search_message(int client_id, const char* message_part) {
    EnterCriticalSection(&client_list_cs);

    ClientNode* client_current = head;
    while (client_current != NULL) {
        if (client_current->id == client_id) {
            MessageNode* message_current = client_current->message_head;
            while (message_current != NULL) {
                if (strstr(message_current->message, message_part) != NULL && !message_current->is_deleted) {
                    gotoxy(0, console_line++);  // ���� ���ο� ��� �� ���� ����
                    printf("�޽��� number: %d, �ð�: %s, Ŭ���̾�Ʈ: %s, �޽���: %s\n",
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

//Ŭ���̾�Ʈ �̸����� Ŭ���̾�Ʈ ID�� ��� �Լ�
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
//��� Ŭ���̾�Ʈ�� ��� �޽����� ����ϴ� �Լ�
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
//Ŭ���̾�Ʈ �ڵ鷯 �Լ�
DWORD WINAPI handle_client(LPVOID socket_desc) {
    SOCKET sock = *(SOCKET*)socket_desc;
    char message[2000]; //�޽����� ������ ����
    int read_size;      //���� �޽����� ũ�⸦ ������ ����
    char client_name[50];//Ŭ���̾�Ʈ �̸��� ������ ����

    //Ŭ���̾�Ʈ �̸��� ����
    if ((read_size = recv(sock, client_name, sizeof(client_name), 0)) == SOCKET_ERROR) {
        gotoxy(0, console_line++);  // ���� ���ο� ��� �� ���� ����
        printf("���� �߻�: %d\n", WSAGetLastError());
        return 1;
    }
    client_name[read_size] = '\0';

    //����Ʈ�� Ŭ���̾�Ʈ�� �߰���
    add_client(sock, client_name);
    gotoxy(0, console_line++);  // ���� ���ο� ��� �� ���� ����
    printf("Ŭ���̾�Ʈ %s ���� ����Ǿ����ϴ�.\n", client_name);

    //Ŭ���̾�Ʈ�� ID�� ������
    int client_id = get_client_id_by_name(client_name);
    if (client_id == -1) {
        gotoxy(0, console_line++);  // ���� ���ο� ��� �� ���� ����
        printf("Ŭ���̾�Ʈ ID�� ã�� �� �����ϴ� �̸�: %s\n", client_name);
        return 1;
    }

    //Ŭ���̾�Ʈ�κ��� �޽����� �ݺ������� ������
    while ((read_size = recv(sock, message, sizeof(message), 0)) > 0) {
        message[read_size] = '\0';

        //�޽��� ����
        if (strncmp(message, "modify ", 7) == 0) {
            int message_id;
            char new_message[2000];
            sscanf(message, "modify %d %[^\n]", &message_id, new_message);
            modify_message(message_id, new_message);
            gotoxy(0, console_line++);  // ���� ���ο� ��� �� ���� ����
            printf("�޽��� %d �� %s �����κ��� �����Ǿ����ϴ� ==> : %s\n", message_id, client_name, new_message);
        }
        //�޽��� ����
        else if (strncmp(message, "delete ", 7) == 0) {
            int message_id;
            sscanf(message, "delete %d", &message_id);
            delete_message(message_id);
            gotoxy(0, console_line++);  // ���� ���ο� ��� �� ���� ����
            printf("�޽��� %d�� %s �����κ��� �����Ǿ����ϴ�\n", message_id, client_name);
        }
        //�޽��� ����
        else if (strncmp(message, "restore ", 8) == 0) {
            int message_id;
            sscanf(message, "restore %d", &message_id);
            restore_message(message_id);
            gotoxy(0, console_line++);  // ���� ���ο� ��� �� ���� ����
            printf("�޽��� %d�� %s�����κ��� �����Ǿ����ϴ� \n", message_id, client_name);
        }
        //�޽��� ��Ī
        else if (strncmp(message, "search ", 7) == 0) {
            char search_term[2000];
            sscanf(message, "search %[^\n]", search_term);
            gotoxy(0, console_line++);  // ���� ���ο� ��� �� ���� ����
            printf(" %s�����κ��� '%s' �� ���Ե� �޽����� ã�ҽ��ϴ� :\n", search_term,client_name );
            search_message(client_id, search_term);
        }
        //�Ϲ� �޽����� ���� ó��
        else {
            add_message_to_client(client_id, client_name, message);
            gotoxy(0, console_line++);  // ���� ���ο� ��� �� ���� ����
            printf("%s\n", message);
            broadcast_message(message, sock);
        }
    }
    //Ŭ���̾�Ʈ ���� ����������
    if (read_size == 0) {
        gotoxy(0, console_line++);  // ���� ���ο� ��� �� ���� ����
        printf("Ŭ���̾�Ʈ %s�� ���� �����Ǿ����ϴ�\n", client_name);
    }
    else if (read_size == SOCKET_ERROR) {
        printf("���� ����: %d\n", WSAGetLastError());
    }
    //Ŭ���̾�Ʈ�� ����Ʈ���� �����ϰ� ������ ����
    remove_client(sock);
    closesocket(sock);
    free(socket_desc);
    return 0;
}

//�������� �ϴ� �Է¿� ���� ��� ����
DWORD WINAPI handle_server_input(LPVOID lpParam) {
    char input[2000];
    while (1) {
        gotoxy(0, INPUT_LINE);
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = '\0';  // ���� ���� ����

        //Ư�� Ŭ���̾�Ʈ�� �޽��� ��� ��û
        if (strncmp(input, "print ", 6) == 0) {
            char client_name[50];
            sscanf(input, "print %s", client_name);
            print_all_messages_for_client(client_name);
        }
        //Ư�� Ŭ���̾�Ʈ�� �޽��� ��Ī ��û
        else if (strncmp(input, "search ", 7) == 0) {
            char client_name[50];
            char search_term[2000];
            sscanf(input, "search %s %[^\n]", client_name, search_term);
            int client_id = get_client_id_by_name(client_name);
            if (client_id != -1) {
                gotoxy(0, console_line++);  // ���� ���ο� ��� �� ���� ����
                printf("  %s������ ���� '%s' �� ���Ե� �޽����� ã�ҽ��ϴ�:\n", client_name, search_term);
                search_message(client_id, search_term);
            }
            else {
                gotoxy(0, console_line++);  // ���� ���ο� ��� �� ���� ����
                printf("Ŭ���̾�Ʈ %s�� ã�� �� �����ϴ�.\n", client_name);
            }
        }
        //��� Ŭ���̾�Ʈ�� �޽����� ���
        else if (strcmp(input, "all_message") == 0) {
            print_all_messages();
        }
        //�ܼ� ȭ�� �ʱ�ȭ
        else if (strcmp(input, "clear") == 0) {
            system("cls");
            gotoxy(0, 17);
            printf("******************�Է��ϴ� �����Դϴ�******************\n");
            printf("******************������ ħ���ϸ� clear�� �����ּ���***\n");
            console_line = 0;
        }
        //�� �� �Է¿� ���� ó��
        else {
            gotoxy(0, console_line++);  // ���� ���ο� ��� �� ���� ����
            printf("���� input: %s\n", input);
        }
    }
    return 0;
}


int main() {
    SetConsoleTitle(TEXT("��ī����")); //�ܼ� �̸�  

    //����ü �ʱ�ȭ
    WSADATA wsa;
    SOCKET server_socket, client_socket;
    struct sockaddr_in server, client;
    int c = sizeof(struct sockaddr_in);
    InitializeCriticalSection(&client_list_cs);

    printf("Winsock�� ����� �� �ֵ��� �ʱ�ȭ ���Դϴ�...\n");
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("����. Error Code: %d\n", WSAGetLastError());
        return 1;
    }

    //���� ����
    printf("������ �������Դϴ�...\n");
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("������ �������� ���߽��ϴ�. failed with error: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }


    //���� ���ε�(������ ������ �ּҿ� ��Ʈ�� ���ε�)
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    printf("������ ���ε����Դϴ�...\n");
    if (bind(server_socket, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        printf("���ε��� �����߽��ϴ�.failed with error: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    printf("��û���� �� �ִ� ���� �����...\n");
    listen(server_socket, MAX_CLIENTS);

    //���� �Է� ó�� ������ ����
    HANDLE server_input_thread = CreateThread(NULL, 0, handle_server_input, NULL, 0, NULL);

    printf("���� �����...\n");
    while ((client_socket = accept(server_socket, (struct sockaddr*)&client, &c)) != INVALID_SOCKET) {
        printf("����Ǿ����ϴ�.\n");

        SOCKET* new_sock = (SOCKET*)malloc(sizeof(SOCKET));
        *new_sock = client_socket;

        HANDLE client_thread = CreateThread(NULL, 0, handle_client, (LPVOID)new_sock, 0, NULL);
        if (client_thread == NULL) {
            printf("������ ���� : %d\n", GetLastError());
            closesocket(client_socket);
            free(new_sock);
        }
        else {
            CloseHandle(client_thread);
        }
    }

    if (client_socket == INVALID_SOCKET) {
        printf("���� ����: %d\n", WSAGetLastError());
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


