#include "chap04.h"
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

int main() {
#if defined(_WIN32)
  WSADATA d;
  if (
      WSAStartup(MAKEWORD(2, 2) & d) {
    fprintf(stderr, "Falha ao inicializar.\n");
    return 1;
      }
#endif
  // Se você está acompanhando todos os meus programas em ordem
  // Voce já deve estar acostumando a montar essa parte do código
  // Portanto, vou me abster de explicá-la todo código
  // Caso ainda esteja com dúvidas visite os códigos do capítulo 3
   
  printf("Configurando endereço remoto...\n");
  struct addrinfo hints; 
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_DGRAM; 
  struct addrinfo * peer_address;
  // O SOCK_DGRAM acima indicou que estamos trabalhando com o protocolo UDP

  if (getaddrinfo("127.0.0.1", "8080", &hints, &peer_address)) {
    fprintf(stderr, "getaddrinfo() falhou. (%d)\n", GETSOCKETERRNO());
    return 1;
  }
  
  printf("O endereço remoto é: ");
  char address_buffer[100]; 
  char service_buffer[100];
  getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen,
              address_buffer, sizeof(address_buffer), service_buffer,
              sizeof(service_buffer), NI_NUMERICHOST | NI_NUMERICSERV);
  printf("%s %s\n", address_buffer, service_buffer);


  printf("Criando socket...\n"); SOCKET socket_peer;
  socket_peer = socket(peer_address->ai_family, peer_address->ai_socktype,
                       peer_address->ai_protocol);

  if (!ISVALIDSOCKET(socket_peer)) {
    fprintf(stderr, "socket() falhou. (%d)\n", GETSOCKETERRNO());
    return 1;
  }

  const char *message = "Hello World";
  // A função sendto é usada para enviar dados ao socket criado
  printf("Enviando: %s\n", message);
  int bytes_sent = sendto(socket_peer,
      message, strlen(message),
      0,
      peer_address->ai_addr, peer_address->ai_addrlen);
  
  printf("Enviado %d bytes.\n", bytes_sent);

  freeaddrinfo(peer_address);
  CLOSESOCKET(socket_peer);

#if defined(_WIN32)
  WSACleanup();
#endif

  printf("Finalizado.\n");
  return 0;
}
