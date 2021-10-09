#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#else
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if defined(_WIN32)
#define ISVALIDSOCKET(s) ((S) != INVALID_SOCKET)
#define CLOSESOCKET(s) closesocket(s)
#define GETSOCKETERRNO() (WSAGetLastError())

#else
#define ISVALIDSOCKET(s) ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define GETSOCKETERRNO() (errno)
#endif

// As macros acima são usadas para descobrir se estamos no Windows ou Linux
// Elas devem ser usadas pois cada sistema cria e manipula SOCKETS de forma
// diferente

#include <stdio.h>
#include <string.h>
#include <time.h>

int main() {
// O código abaixo verifica se o _WIN32 está definido, ou seja, se estamos
// usando um sistema WIN, caso sim, incializamos o WinSocket algo que não
// precisa ser feito no Linux, por isto do defined
#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Failed to initialize.\n");
    return 1;
  }
#endif

  printf("Configurando local address...\n");
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  // Nessas quatro linhas de código definimos uma estrutura addrinfo,
  // ela pode ser passada ao getaddrinfo para definir um tipo de estrutura
  // que ele deve retornar.
  // A linha 53 zera os dados de hints usando o memset
  // 54 define que procuramos por um endereço IPv4
  // 55 define que estamos usando um TCP
  // 56 define ao getaddrinfo que queremos que ele configurar o endereço em
  // qualquer interface de rede disponível
  struct addrinfo *bind_address;
  getaddrinfo(0, "8080", &hints, &bind_address);
  // Neste aso getaddrinfo gera um endereço para que possamos usar um bind
  // o primeiro parametro indica o hostname
  // como nos referimos ao local(127.0.0.1) pode-se usar 0
  // O segundo parametro indica a porta
  // Mesmo que alguns programas não usem o getaddrinfo o ideal é usá-lo pois
  // com ele fica muito mais fácil converter o código de IPv4 para IPv6
  printf("Criando socket...\n");
  SOCKET socket_listen;
  socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype,
                         bind_address->ai_protocol);
  // Essa função precisa de 3 coisas:socket family, socket type, socket
  // protocol.
  if (!ISVALIDSOCKET(socket_listen)) {
    fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
    return 1;
  }
  // O código acima utiliza-se das macros criadas para verificar se o socket
  // pôde ser criado com sucesso.
  printf("Vinculando socket ao endereço local...\n");
  if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {
    fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
    return 1;
  }
  // Tentamos ouvir o socket, caso de errado o programa é encerrado
  printf("Ouvindo...\n");
  if (listen(socket_listen, 10) < 0) {
    fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
    return 1;
  }
  // O código acima define que devemos ter no máximo 10 conexões em nosso
  // servidor, caso o número ultrapasse ele parará de aceitar conexões
  // até que uma se vá.
  printf("Esperando por conexão...\n");
  struct sockaddr_storage client_address;
  socklen_t client_len = sizeof(client_address);
  SOCKET socket_client =
      accept(socket_listen, (struct sockaddr *)&client_address, &client_len);
  if (!ISVALIDSOCKET(socket_client)) {
    fprintf(stderr, "accept() failed. (%d)\n", GETSOCKETERRNO());
    return 1;
  }
  // O código acima é responsável por aceitar uma conexão TCP que seja feita
  // enquanto ela não chega o processo fica dormindo, caso de erro o programa é
  // encerrado
  printf("Cliente conectado.\n");
  char address_buffer[100];
  getnameinfo((struct sockaddr *)&client_address, client_len, address_buffer,
              sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
  printf("Seu endereço eh: %s\n", address_buffer);
  // Após aceitar conexão nós pegamos do client_address seu endereço usando
  // getnameinfo
  printf("Lendo requisição...\n");
  char request[1024];
  int bytes_received = recv(socket_listen, request, 1024, 0);
  // O código acima é responsável por receber e printar os dados recebidos
  // do clientchar response[500];
  printf("%.*s", bytes_received, request);

  // printando os dados do servidor
  printf("Enviando resposta...\n");
  const char *response = "HTTP/1.1 200 OK\r\n"
                         "Connection: close\r\n"
                         "Content-Type: text/plain\r\n\r\n"
                         "Local time is: ";
  int bytes_sent = send(socket_client, response, strlen(response), 0);
  printf("Closing connection...\n");
  // Acima enviamos o cabeçalho HTTP bem como um pedaço do corpo
  // da requisição.
  time_t timer;
  time(&timer);
  char *time_msg = ctime(&timer);
  bytes_sent = send(socket_client, time_msg, strlen(time_msg), 0);
  // Aqui enviamos o restante da requisição. Nesse caso, as horas
  printf("Fechando conexão...\n");

  CLOSESOCKET(socket_client);
#if defined(_WIN32)
  WSACleanup();
#endif
  printf("Finished.\n");
  return 0;

  freeaddrinfo(bind_address);
}
