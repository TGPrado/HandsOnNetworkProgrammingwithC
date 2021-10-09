#include "chap03.h"
#include <ctype.h>
#include <sys/select.h>

int main() {
// Iniciando socket WINDOWS
#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Falha ao inicializar.\n");
    return 1;
  }
#endif
  // Configurando dica para pegar interface de rede
  printf("Configurando endereço local...\n");
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  // Pegando interface
  struct addrinfo *bind_address;
  getaddrinfo(0, "8080", &hints, &bind_address);
  // Criando socket
  printf("Criando socket...\n");
  SOCKET socket_listen;
  socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype,
                         bind_address->ai_protocol);
  // Verificando se o socket foi criado
  if (!ISVALIDSOCKET(socket_listen)) {
    fprintf(stderr, "socket() falhou. (%d)\n", GETSOCKETERRNO());
    return 1;
  }
  // Usamos o bind para LIGAR o nosso endereço local ao socket
  printf("Ligando socket ao endereço local...\n");
  if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {
    fprintf(stderr, "bind() falhou. (%d)\n", GETSOCKETERRNO());
    return 1;
  }
  // Liberando o espaço usado pelo getaddrinfo
  freeaddrinfo(bind_address);

  // colocamos o socket para ouvir as requisições, ele aceitará no maxímo 10
  // vinculadas ao mesmo socket
  printf("Listening...\n");
  if (listen(socket_listen, 10) < 0) {
    fprintf(stderr, "listen() falhou. (%d)\n", GETSOCKETERRNO());
    return 1;
  }
  // Criamos uma estrutura fd_set para aramazenar nosso socket
  // Enviamos o socket para lá e definimos max_socket como seu valor
  fd_set master;
  FD_ZERO(&master);
  FD_SET(socket_listen, &master);
  SOCKET max_socket = socket_listen;

  printf("Esperando conexões...\n");
  // Aqui esperamos por conexões ao socket

  while (1) {
    // criamos outra variável fd_set
    // e a igualamos a master
    // após usamos o select para verificar se foram feitas alteraçoes no socket
    // Perceba que usamos reads ao invs de master pois select altera a estrutura
    fd_set reads;
    reads = master;
    if (select(max_socket + 1, &reads, 0, 0, 0) < 0) {
      fprintf(stderr, "select() falhou. (%d)\n", GETSOCKETERRNO());
      return 1;
    }
    // Agora vamos passar por todos os sockets e verificar se estão contidos em
    // reads

    SOCKET i;
    for (i = 1; i <= max_socket; ++i) {
      // Se o número do socket estiver em reads
      if (FD_ISSET(i, &reads)) {
        // Em nosso sistema há dois tipos de socket: o usado para se conectar
        // E o usado para receber dados.
        // Nessa parte do projeto precisamos verificar qual é qual
        if (i == socket_listen) {
          // Caso nosso socket seja do tipo usado para conexão simplesmenete
          // a aceitamos usando o accept
          // Caso haja algum problema para aceitar a conexão o programa é
          // encerrado
          struct sockaddr_storage client_address;
          socklen_t client_len = sizeof(client_address);
          SOCKET socket_client = accept(
              socket_listen, (struct sockaddr *)&client_address, &client_len);

          if (!ISVALIDSOCKET(socket_client)) {
            fprintf(stderr, "accept() falhou. (%d)\n", GETSOCKETERRNO());
            return 1;
          }
          // Por fim, adicionamos o socket que foi aceito ao nosso master
          // Além disso, pegamos os dados de quem conectou
          // usando o getnameinfo e o exibimos
          FD_SET(socket_client, &master);
          if (socket_client > max_socket)
            max_socket = socket_client;

          char address_buffer[100];
          getnameinfo((struct sockaddr *)&client_address, client_len,
                      address_buffer, sizeof(address_buffer), 0, 0,
                      NI_NUMERICHOST);
          printf("Nova conexão de %s\n", address_buffer);

        } else {
          // Como não foi detectada uma conexão nova
          // O socket é de dados
          // Pegamos o dado e exibimos
          char read[1024];
          int bytes_received = recv(i, read, 1024, 0);
          if (bytes_received < 1) {
            FD_CLR(i, &master);
            CLOSESOCKET(i);
            continue;
          }

          SOCKET j;
          for (j = 1; j <= max_socket; ++j) {
            if (FD_ISSET(j, &master)) {
              if (j == socket_listen || j == 1)
                continue;
              else
                send(j, read, bytes_received, 0);
            }
          }
        }

      } // if FD_ISSET
    }   // for i to max_socket
  }     // while(1)
  // Fechamos o socket
  printf("Fechando socket ...\n");
  CLOSESOCKET(socket_listen);

#if defined(_WIN32)
  WSACleanup();
#endif

  printf("Finished.\n");

  return 0;
}
