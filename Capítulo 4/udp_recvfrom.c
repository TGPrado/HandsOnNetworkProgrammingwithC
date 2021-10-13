#include "chap04.h"
#include <netdb.h>
#include <stdio.h>
#include <sys/socket.h>

int main() {
#if defined(_WIN32)
  WSADATA d;
  if (WSAStartp(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Falha ao inicializar.\n");
    return 1;
  }
#endif
  // Se você está acompanhando todos os meus programas em ordem
  // Voce já deve estar acostumando a montar essa parte do código
  // Portanto, vou me abster de explicá-la todo código
  // Caso ainda esteja com dúvidas visite os códigos do capítulo 3
  printf("Configurando endereço local...\n");
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  struct addrinfo *bind_address;
  getaddrinfo(0, "8080", &hints, &bind_address);

  printf("Criando socket...\n");
  SOCKET socket_listen;
  socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype,
                         bind_address->ai_protocol);
  if (!ISVALIDSOCKET(socket_listen)) {
    fprintf(stderr, "socket() falhou. (%d)\n", GETSOCKETERRNO());
    return 1;
  }

  printf("Conectando socket ao endereço local...\n");
  if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {
    fprintf(stderr, "bind() falhou. (%d)\n", GETSOCKETERRNO());
    return 1;
  }

  freeaddrinfo(bind_address);

  // A partir daqui o código começa a ficar um pouco diferente do usado no tcp
  // Primeiro criamos uma variável para armazenar o endereço do client

  struct sockaddr_storage client_address;
  // E criamos client_len para armazenar o tamanho do endereço dele
  // Isso facilita a portabilidade do código para Ipv6
  socklen_t client_len = sizeof(client_address);
  char read[1024];
  // recvfrom funciona como recv, o que o difere é que ele retorna o
  // endereço do cliente que se conectou bem como a mensagem enviada
  int bytes_received =
      recvfrom(socket_listen, read, 1024, 0, (struct sockaddr *)&client_address,
               &client_len);

  printf("(%d bytes) Recebidos: %.*s\n", bytes_received, bytes_received, read);

  // Aqui pegamos e transformamos o endereço recebido em um endereço legivel
  // usando a função getnameinfo
  printf("O endereço remoto é: ");
  char address_buffer[100];
  char service_buffer[100];
  getnameinfo(((struct sockaddr *)&client_address), client_len, address_buffer,
              sizeof(address_buffer), service_buffer, sizeof(service_buffer),
              NI_NUMERICHOST | NI_NUMERICSERV);
  printf("%s %s\n", address_buffer, service_buffer);

#if defined(_WIN32)
  WSACleanup();
#endif

  printf("Finalizado.\n");

  return 0;
}
