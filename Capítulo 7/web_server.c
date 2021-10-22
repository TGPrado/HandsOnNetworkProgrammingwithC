#include "chap07.h"

const char *get_content_type(const char *path) {
  // Inicialmente, usamos essa função para verificar qual a extensão do arquivo.
  // strrchr retorna uma string a partir da "dica"
  const char *last_dot = strrchr(path, '.');
  if (last_dot) {
    if (strcmp(last_dot, ".css") == 0)
      return "text/css";
    if (strcmp(last_dot, ".csv") == 0)
      return "text/csv";
    if (strcmp(last_dot, ".gif") == 0)
      return "image/gif";
    if (strcmp(last_dot, ".htm") == 0)
      return "text/html";
    if (strcmp(last_dot, ".html") == 0)
      return "text/html";
    if (strcmp(last_dot, ".ico") == 0)
      return "image/x-icon";
    if (strcmp(last_dot, ".jpeg") == 0)
      return "image/jpeg";
    if (strcmp(last_dot, ".jpg") == 0)
      return "image/jpeg";
    if (strcmp(last_dot, ".js") == 0)
      return "application/javascript";
    if (strcmp(last_dot, ".json") == 0)
      return "application/json";
    if (strcmp(last_dot, ".png") == 0)
      return "image/png";
    if (strcmp(last_dot, ".pdf") == 0)
      return "application/pdf";
    if (strcmp(last_dot, ".svg") == 0)
      return "image/svg+xml";
    if (strcmp(last_dot, ".txt") == 0)
      return "text/plain";
  }

  return "application/octet-stream";
}

SOCKET create_socket(const char *host, const char *port) {
  // Essa função é usada para criar nosso socket, já a conhecemos de outros
  // exemplos portanto, não irei explica-la
  printf("Configurando endereço local...\n");
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  struct addrinfo *bind_address;
  getaddrinfo(host, port, &hints, &bind_address);

  printf("Criando socket...\n");
  SOCKET socket_listen;
  socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype,
                         bind_address->ai_protocol);
  if (!ISVALIDSOCKET(socket_listen)) {
    fprintf(stderr, "socket() falhou. (%d)\n", GETSOCKETERRNO());
    exit(1);
  }

  printf("Conectando ao endereço local...\n");
  if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {
    fprintf(stderr, "bind() falhou. (%d)\n", GETSOCKETERRNO());
    exit(1);
  }
  freeaddrinfo(bind_address);

  printf("Ouvindo...\n");
  if (listen(socket_listen, 10) < 0) {
    fprintf(stderr, "listen() falhou. (%d)\n", GETSOCKETERRNO());
    exit(1);
  }

  return socket_listen;
}

#define MAX_REQUEST_SIZE 2047

// Definimos uma estrutura para armazenar todos os dados dos clientes
struct client_info {
  socklen_t address_length;
  struct sockaddr_storage address;
  SOCKET socket;
  char request[MAX_REQUEST_SIZE + 1];
  int received;
  struct client_info *next;
};
// criamos uma estrutura client_info global chamada clients
static struct client_info *clients = 0;

struct client_info *get_client(SOCKET s) {
  // Criamos uma função responsável por pegar informações dos clientes
  // Caso o socket seja encontrado dentro de clients, nós o retornamos
  // Caso contrário criamos um novo e o inserimos em clients
  struct client_info *ci = clients;

  while (ci) {
    if (ci->socket == s)
      break;
    ci = ci->next;
  }
  // O código acima verifica se encontramos algum socket dentro de ci que
  // por sua vez é igual a client
  if (ci)
    return ci;
  // Se encontrado, o retornamos
  // Caso contrário criamos um novo socket n
  struct client_info *n =
      (struct client_info *)calloc(1, sizeof(struct client_info));

  // Se não for possível alocar a memória dizemos isso e encerramos
  if (!n) {
    fprintf(stderr, "Não foi possível alocar a memória.\n");
    exit(1);
  }
  // Armazenamos o tamanho do endereço em n
  n->address_length = sizeof(n->address);
  n->next = clients;
  // Apontamos o next de n para clients, como ele era vazio, significa o fim da
  // lista
  clients = n;
  // fazemos clients igual a n
  // retornamos n
  return n;
}

void drop_client(struct client_info *client) {
  // Apagar elemento da lista
  // Inicialmente fechamos o socket
  CLOSESOCKET(client->socket);

  // Com este método podemos modificar os valores de elementos diretamente
  // Pois clients é um ponteiro, então devemos criar um ponteiro para apontar
  // para outro ponteiro
  struct client_info **p = &clients;

  while (*p) {
    // se p for igual ao client que queremos remover
    if (*p == client) {
      // colocamos p para apontar para o próximo client
      *p = client->next;
      // limpamos a memória de client
      free(client);
      // encerramos a função
      return;
    }
    p = &(*p)->next;
  }

  fprintf(stderr, "drop_client não encontrado.\n");
  exit(1);
}

const char *get_client_address(struct client_info *ci) {
  static char address_buffer[100];
  getnameinfo((struct sockaddr *)&ci->address, ci->address_length,
              address_buffer, sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
  return address_buffer;
}

fd_set wait_on_clients(SOCKET server) {
  // Como o servidor deve ser capaz de manejar muitas solicitações
  // simultaneamente devemos definir, nessa função, esperar novos dados ou novas
  // conexões Já conhecemoms um pedaço desse código
  fd_set reads;
  FD_ZERO(&reads);
  FD_SET(server, &reads);
  SOCKET max_socket = server;

  struct client_info *ci = clients;
  // Então, adicionamos todos os sockets de client em &reads, além do server
  while (ci) {
    FD_SET(ci->socket, &reads);
    if (ci->socket > max_socket)
      max_socket = ci->socket;
    ci = ci->next;
  }
  // Verificamos se há algum dado, e retornamos o resultado
  if (select(max_socket + 1, &reads, 0, 0, 0) < 0) {
    fprintf(stderr, "select() falhou. (%d)\n", GETSOCKETERRNO());
    exit(1);
  }

  return reads;
}

void send_400(struct client_info *client) {
  // É usado para avisar que houve erro na request
  const char *c400 = "HTTP/1.1 400 Bad Request\r\n"
                     "Connection: close\r\n"
                     "Content-Length: 11\r\n\r\nBad Request";
  send(client->socket, c400, strlen(c400), 0);
  // fechamos o socket de quem fez a solicitação
  drop_client(client);
}

void send_404(struct client_info *client) {
  // Usado para avisar que o recurso solicitado não foi encontrado
  const char *c404 = "HTTP/1.1 404 Not Found\r\n"
                     "Connection: close\r\n"
                     "Content-Length: 9\r\n\r\nNot Found";
  send(client->socket, c404, strlen(c404), 0);
  // fechamos o socket com o cliente
  drop_client(client);
}

void serve_resource(struct client_info *client, const char *path) {
  // Printamos qual recurso foi solicitado para armazenar em logs
  printf("serve_resource %s %s\n", get_client_address(client), path);
  // Se o path for igual a / fazemos path = index.html
  if (strcmp(path, "/") == 0)
    path = "/index.html";

  if (strlen(path) > 100) {
    send_400(client);
    return;
  }

  if (strstr(path, "..")) {
    send_404(client);
    return;
  }

  char full_path[128];
  sprintf(full_path, "public%s", path);

#if defined(_WIN32)
  char *p = full_path;
  while (*p) {
    if (*p == '/')
      *p = '\\';
    ++p;
  }
#endif
  // abrimos o arquivo
  FILE *fp = fopen(full_path, "rb");
  // se algum erro ocorreu retornamos 404
  if (!fp) {
    send_404(client);
    return;
  }
  // definimos o ponteiro para o fim do arquivo
  fseek(fp, 0L, SEEK_END);
  // usado para verificar o tamanho do arquivo
  size_t cl = ftell(fp);
  // voltamos o ponteiro para o começo do arquivo
  rewind(fp);
  // Pegamos a extensão do arquivo para usar em Content-Type
  const char *ct = get_content_type(full_path);

#define BSIZE 1024
  char buffer[BSIZE];
  // enviamos a resposta pelo socket
  sprintf(buffer, "HTTP/1.1 200 OK\r\n");
  send(client->socket, buffer, strlen(buffer), 0);

  sprintf(buffer, "Connection: close\r\n");
  send(client->socket, buffer, strlen(buffer), 0);

  sprintf(buffer, "Content-Length: %u\r\n", cl);
  send(client->socket, buffer, strlen(buffer), 0);

  sprintf(buffer, "Content-Type: %s\r\n", ct);
  send(client->socket, buffer, strlen(buffer), 0);

  sprintf(buffer, "\r\n");
  send(client->socket, buffer, strlen(buffer), 0);
  // enviamos byte por byte do arquivo de texto
  int r = fread(buffer, 1, BSIZE, fp);
  while (r) {
    send(client->socket, buffer, r, 0);
    r = fread(buffer, 1, BSIZE, fp);
  }

  fclose(fp);
  drop_client(client);
}

// Com todas as funções definidas, podemos inicalizar a função principal
int main() {

#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Failed to initialize.\n");
    return 1;
  }
#endif

  SOCKET server = create_socket(0, "8080");

  while (1) {
    // iniciamos uma variável para armazenar os sockets
    fd_set reads;
    // esperamos alguma alteração no socket
    // lembrando que wait_on_clients funciona retornando o
    // select aplicado em clients
    reads = wait_on_clients(server);

    // caso haja alteração no servidor
    if (FD_ISSET(server, &reads)) {
      // Passamos -1 para get_client, como ele verifica se o socket existe
      // e socket é um int, aqui ele cria um novo cliente.
      struct client_info *client = get_client(-1);
      // aceitamos a conexão do socket
      client->socket = accept(server, (struct sockaddr *)&(client->address),
                              &(client->address_length));

      if (!ISVALIDSOCKET(client->socket)) {
        fprintf(stderr, "accept() falhou. (%d)\n", GETSOCKETERRNO());
        return 1;
      }

      printf("Nova conexão de %s.\n", get_client_address(client));
    }

    struct client_info *client = clients;
    while (client) {
      struct client_info *next = client->next;

      if (FD_ISSET(client->socket, &reads)) {
        // se o valor recebido for maior que o nosso buffer retornamos um 400
        if (MAX_REQUEST_SIZE == client->received) {
          send_400(client);
          client = next;
          continue;
        }
        // pegando os dados do cliente
        int r = recv(client->socket, client->request + client->received,
                     MAX_REQUEST_SIZE - client->received, 0);

        if (r < 1) {
          printf("Desconexão inesperada de %s.\n", get_client_address(client));
          drop_client(client);

        } else {
          client->received += r;
          client->request[client->received] = 0;

          char *q = strstr(client->request, "\r\n\r\n");
          if (q) {
            *q = 0;

            if (strncmp("GET /", client->request, 5)) {
              send_400(client);
            } else {
              char *path = client->request + 4;
              char *end_path = strstr(path, " ");
              if (!end_path) {
                send_400(client);
              } else {
                *end_path = 0;
                serve_resource(client, path);
              }
            }
          } // if (q)
        }
      }

      client = next;
    }

  } // while(1)

  printf("\nFechando socket...\n");
  CLOSESOCKET(server);

#if defined(_WIN32)
  WSACleanup();
#endif

  printf("Finalizado.\n");
  return 0;
}
