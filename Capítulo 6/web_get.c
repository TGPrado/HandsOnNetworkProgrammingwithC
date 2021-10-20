#include "chap06.h"

#define TIMEOUT 5.0

// Funcão usada para separar as partes da URL, como hostname porta e path
void parse_url(char *url, char **hostname, char **port, char **path) {
  printf("URL: %s\n", url);
  // Inicialmente criamos uma variável p para armazenar o local da memória onde
  // se encontra ://
  char *p;
  p = strstr(url, "://");
  // Após criamos um protocol que tem valor igual a zero
  char *protocol = 0;
  if (p) {
    // Colocamos o protocol para apontar para o endereço de url, ou seja, em seu
    // começo
    protocol = url;
    // E setamos o valor de : como o inteiro zero
    // Isso faz com que a string seja cortada, pois int 0 == null
    *p = 0;
    // Antes p apontava para ":", agora utilizamos de += para faze-lo apontar
    // para a letra após //
    p += 3;
  } else {
    // Caso, não exista :// Na url fazemos p ser igual a Url
    p = url;
  }
  // Se protocol existir
  if (protocol) {
    // Se protocol for diferente de http
    if (strcmp(protocol, "http")) {
      // printamos o erro e dizemos que o protocolo é desconhecido.
      fprintf(stderr,
              "Protocolo desconhecido '%s'. Apenas 'http' é suportado.\n",
              protocol);
      exit(1);
    }
  }

  // colocamos que o hostname aponte para o endereço que p aponta
  *hostname = p;
  // Andamos com p até o fim do domínio
  while (*p && *p != ':' && *p != '/' && *p != '#')
    ++p;

  // Apontamos port para o endereço da string 80
  *port = "80";

  if (*p == ':') {
    *p++ = 0;
    *port = p;
  }
  while (*p && *p != '/' && *p != '#')
    ++p;

  *path = p;
  if (*p == '/') {
    *path = p + 1;
  }
  *p = 0;

  while (*p && *p != '#')
    ++p;
  if (*p == '#')
    *p = 0;

  printf("hostname: %s\n", *hostname);
  printf("port: %s\n", *port);
  printf("path: %s\n", *path);
}

void send_request(SOCKET s, char *hostname, char *port, char *path) {
  char buffer[2048];

  sprintf(buffer, "GET /%s HTTP/1.1\r\n", path);
  sprintf(buffer + strlen(buffer), "Host: %s:%s\r\n", hostname, port);
  sprintf(buffer + strlen(buffer), "Connection: close\r\n");
  sprintf(buffer + strlen(buffer), "User-Agent: honpwc web_get 1.0\r\n");
  sprintf(buffer + strlen(buffer), "\r\n");

  send(s, buffer, strlen(buffer), 0);
  printf("Enviando Headers:\n%s", buffer);
}

SOCKET connect_to_host(char *hostname, char *port) {
  printf("Configurando endereço remoto...\n");
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *peer_address;
  if (getaddrinfo(hostname, port, &hints, &peer_address)) {
    fprintf(stderr, "getaddrinfo() falhou. (%d)\n", GETSOCKETERRNO());
    exit(1);
  }

  printf("O endereço remoto é: ");
  char address_buffer[100];
  char service_buffer[100];
  getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen, address_buffer,
              sizeof(address_buffer), service_buffer, sizeof(service_buffer),
              NI_NUMERICHOST);
  printf("%s %s\n", address_buffer, service_buffer);

  printf("Criando socket...\n");
  SOCKET server;
  server = socket(peer_address->ai_family, peer_address->ai_socktype,
                  peer_address->ai_protocol);
  if (!ISVALIDSOCKET(server)) {
    fprintf(stderr, "socket() falhou. (%d)\n", GETSOCKETERRNO());
    exit(1);
  }

  printf("Conectando...\n");
  if (connect(server, peer_address->ai_addr, peer_address->ai_addrlen)) {
    fprintf(stderr, "connect() falhou. (%d)\n", GETSOCKETERRNO());
    exit(1);
  }
  freeaddrinfo(peer_address);

  printf("Conectado.\n\n");

  return server;
}

int main(int argc, char *argv[]) {

// Configurando o socket para Windows
#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Falha ao inicializar.\n");
    return 1;
  }
#endif

  if (argc < 2) {
    fprintf(stderr, "Uso: web_get url\n");
    return 1;
  }
  // Criamos uma variável que aponta para o endereço de memória em que
  // O argumento respectivo a URL está.
  char *url = argv[1];
  // Também criamos variáveis para armazenar as strings do path hostname e port
  //É interesante que, na verdade, criamos um char que aponta para o endereço
  // De memória em que armazenaremos os dados
  char *hostname, *port, *path;
  // Passamos o endereço de memória de argv para parse_url
  // Também fornecemos os endereços de hostname, port e path
  parse_url(url, &hostname, &port, &path);
  // Connect to host usa o código padraão conhecido para realizar uma conexão
  // com o host
  SOCKET server = connect_to_host(hostname, port);
  // Enviado os headers para o servidor e solicitando a página
  send_request(server, hostname, port, path);

  const clock_t start_time = clock();

#define RESPONSE_SIZE 32768
  // sabendo que response_size é o tamanho maximo de nossa resposta
  // definimos response como uma variável para armazenar a resposta
  char response[RESPONSE_SIZE + 1];
  // p é um ponteiro que rastreia quantos já escrevemos da resposta
  char *p = response, *q;
  // end é usado para definir o final de nossa resposta
  // e garantir que não a ultrapassemos
  char *end = response + RESPONSE_SIZE;
  char *body = 0;

  enum { length, chunked, connection };
  int encoding = 0;
  int remaining = 0;
  // Esse loop é usado para processar a resposta
  while (1) {
    // apresentamos em quanto tempo a resposta demorou para chegar
    if ((clock() - start_time) / CLOCKS_PER_SEC > TIMEOUT) {
      fprintf(stderr, "Tempo de resposta:  %.2f segundos\n", TIMEOUT);
      return 1;
    }
    // Se estouramos o buffer
    if (p == end) {
      fprintf(stderr, "Buffer estourado\n");
      return 1;
    }
    // utilizamos o select para verificar se obtivemos alguma resposta do socket
    fd_set reads;
    FD_ZERO(&reads);
    FD_SET(server, &reads);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 200000;
    // caso ele falhe enceramos
    if (select(server + 1, &reads, 0, 0, &timeout) < 0) {
      fprintf(stderr, "select() falhou. (%d)\n", GETSOCKETERRNO());
      return 1;
    }
    // Verificamos se o socket server esta na lista retornada
    if (FD_ISSET(server, &reads)) {
      int bytes_received = recv(server, p, end - p, 0);
      if (bytes_received < 1) {
        if (encoding == connection && body) {
          printf("%.*s", (int)(end - body), body);
        }

        printf("\nConexão fechada pelo par.\n");
        break;
      }

      /*printf("Received (%d bytes): '%.*s'",
              bytes_received, bytes_received, p);*/

      p += bytes_received;
      *p = 0;

      if (!body && (body = strstr(response, "\r\n\r\n"))) {
        *body = 0;
        body += 4;

        printf("Headers recebidos:\n%s\n", response);

        q = strstr(response, "\nContent-Length: ");
        if (q) {
          encoding = length;
          q = strchr(q, ' ');
          q += 1;
          remaining = strtol(q, 0, 10);

        } else {
          q = strstr(response, "\nTransfer-Encoding: chunked");
          if (q) {
            encoding = chunked;
            remaining = 0;
          } else {
            encoding = connection;
          }
        }
        printf("\nCorpo recebido:\n");
      }

      if (body) {
        if (encoding == length) {
          if (p - body >= remaining) {
            printf("%.*s", remaining, body);
            break;
          }
        } else if (encoding == chunked) {
          do {
            if (remaining == 0) {
              if ((q = strstr(body, "\r\n"))) {
                remaining = strtol(body, 0, 16);
                if (!remaining)
                  goto finish;
                body = q + 2;
              } else {
                break;
              }
            }
            if (remaining && p - body >= remaining) {
              printf("%.*s", remaining, body);
              body += remaining + 2;
              remaining = 0;
            }
          } while (!remaining);
        }
      } // if (body)
    }   // if FDSET
  }     // end while(1)
finish:

  printf("\nFechando socket...\n");
  CLOSESOCKET(server);

#if defined(_WIN32)
  WSACleanup();
#endif

  printf("Finalizado.\n");
  return 0;
}
