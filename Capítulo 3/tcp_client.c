#include "chap03.h"

#if defined(_WIN32)
#include <conio.h>
#endif

int main(int argc, char *argv[]) {
// Verificando se estamos em um sistema Unix, se não, inicia o socket
#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Falha ao inicializar.\n");
    return 1;
  }
#endif
  // Verifica se a chamada do programa esta correta
  if (argc < 3) {
    fprintf(stderr, "usage: tcp_client hostname port\n");
    return 1;
  }
  // Especifica uma estrutura ou interface para o getaddrinfo retornar
  // Nesse caso, SOCK_STREAM indica uma conexão TCP
  printf("Configurando endereço remoto...\n");
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *peer_address;
  // Passa os parametros da interface para getaddrinfo (essa função retorna um
  // endereço para que possamos criar um bind) o primeiro parametro é o endereço
  // da conexão o segundo a porta o terceiro são caracteristicas de nossa
  // interface que devem ser usadas o quarto onde os dados devem ser armazenados
  if (getaddrinfo(argv[1], argv[2], &hints, &peer_address)) {
    fprintf(stderr, "getaddinfo() failed. (%d)\n", GETSOCKETERRNO());
    return 1;
  }

  // O getnameinfo é usado para converter o valor retornado por getaddrinfo em
  // string
  printf("O endereço remoto é: ");
  char address_buffer[100];
  char service_buffer[100];
  getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen, address_buffer,
              sizeof(address_buffer), service_buffer, sizeof(service_buffer),
              NI_NUMERICHOST);
  printf("%s %s\n", address_buffer, service_buffer);
  // Apos, tentamos criar o socket
  printf("Criando socket...\n");
  SOCKET socket_peer;
  socket_peer = socket(peer_address->ai_family, peer_address->ai_socktype,
                       peer_address->ai_protocol);
  if (!ISVALIDSOCKET(socket_peer)) {
    fprintf(stderr, "socket() falhou (%d)\n", GETSOCKETERRNO());
    return 1;
  }
  // E realizar a conexão
  printf("Conectando...\n");
  if (connect(socket_peer, peer_address->ai_addr, peer_address->ai_addrlen)) {
    fprintf(stderr, "connect() falhou (%d)\n", GETSOCKETERRNO());
    return 1;
  }
  freeaddrinfo(peer_address);

  printf("Conectado.\n");
  printf("Para enviar dados, coloque o texto seguido de enter.\n");
  // Aqui começamos a solicitar dados para enviar
  // Se o dado for inserido, o enviamos, se não ficamos esperando
  // Não chamamos recv() diretamente pois isso iria bloquear o socket e
  // perderiamos dados Enviados para ele
  while (1) {
    // Criamos  lista de sockets
    // E zeramos ela, após inserimos nosso socket
    fd_set reads;
    FD_ZERO(&reads);
    FD_SET(socket_peer, &reads);
#if !defined(_WIN32)
    FD_SET(0, &reads);
#endif

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    // Definimos um tempo para esperar uma atividade no socket
    // Pelo struct timeout
    // O valor é passado para select que espera pela atividade do socket
    if (select(socket_peer + 1, &reads, 0, 0, &timeout) < 0) {
      fprintf(stderr, "select() failed. (%) \n", GETSOCKETERRNO());
      return 1;
    }
    // O valor retornado informa se houve alguma mudança no socket
    // Verificamos isso usando FD_ISSET
    // Caso tenhamos alteração o dado recebido é printado em tela
    // Caso a conexão tenha sido fechada encerramos o programa
    // É interessante perceber que só usamos recv caso haja dados
    if (FD_ISSET(socket_peer, &reads)) {
      char read[4096];
      int bytes_received = recv(socket_peer, read, 4096, 0);
      if (bytes_received < 1) {
        printf("Conexão fechada pelo peer\n");
        break;
      }
      printf("%d Bytes recebidos: %.*s", bytes_received, bytes_received, read);
    }
    // No Windows  usamos _kbhit() para identificar se há alguma entrada no
    // console Ele retorna 0 se o pressionamento de tecla não tratado estiver na
    // fila Para unix verificamos se o descritor de arquivos stdin(0) está
    // definido Caso sim, usamos fgets para escrever nele e enviamos o dado com
    // send para o socket
#if defined(_WIN32)
    if (_kbhit()) {
#else
    if (FD_ISSET(0, &reads)) {
#endif
      // observe que fgets sempre encerra a entrada com uma chamada de nova
      // linha
      char read[4096];
      if (!fgets(read, 4096, stdin))
        break;
      printf("Enviando: %s", read);
      int bytes_sent = send(socket_peer, read, strlen(read), 0);
      printf("%d bytes enviados.\n", bytes_sent);
    }
  }

  printf("Fechando socket...\n");
  CLOSESOCKET(socket_peer);

#if defined(_WIN32)
  WSACleanup();
#endif

  printf("Finished.\n");
  return 0;
}
