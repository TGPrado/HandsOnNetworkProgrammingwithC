#include "chap10.h"

int main() {
// Iniciando socket em sistema windows
#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Falha ao inicializar.\n");
    return 1;
  }
#endif
  // Iniciando SSL

  SSL_library_init();
  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
  // Gerando um objeto SSL_CTX
  SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
  if (!ctx) {
    fprintf(stderr, "SSL_CTX_new() falhou.\n");
    return 1;
  }

  // Configurando nosso certificado pessoal
  if (!SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM) ||
      !SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM)) {
    fprintf(stderr, "SSL_CTX_use_certificate_file() falhou.\n");
    ERR_print_errors_fp(stderr);
    return 1;
  }

  // Criação de socket padrão
  printf("Configurando endereço local...\n");
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
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

  printf("Ouvindo...\n");
  if (listen(socket_listen, 10) < 0) {
    fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
    return 1;
  }

  while (1) {

    printf("Aguardadndo por conexão...\n");
    struct sockaddr_storage client_address;
    socklen_t client_len = sizeof(client_address);
    SOCKET socket_client =
        accept(socket_listen, (struct sockaddr *)&client_address, &client_len);
    if (!ISVALIDSOCKET(socket_client)) {
      fprintf(stderr, "accept() falhou. (%d)\n", GETSOCKETERRNO());
      return 1;
    }

    printf("Cliente conectado... ");
    char address_buffer[100];
    getnameinfo((struct sockaddr *)&client_address, client_len, address_buffer,
                sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
    printf("%s\n", address_buffer);

    // criamos um novo objeto ssl usando o nosso antigo contexto inserido no
    // objeto ctx
    SSL *ssl = SSL_new(ctx);
    if (!ssl) {
      fprintf(stderr, "SSL_new() failed.\n");
      return 1;
    }
    // linkamos nosso socket_client com o objeto SSL
    SSL_set_fd(ssl, socket_client);
    if (SSL_accept(ssl) <= 0) {
      fprintf(stderr, "SSL_accept() falhou.\n");
      ERR_print_errors_fp(stderr);

      SSL_shutdown(ssl);
      CLOSESOCKET(socket_client);
      SSL_free(ssl);

      continue;
    }

    printf("Conexão SSL usando %s\n", SSL_get_cipher(ssl));

    printf("Lendo requisição...\n");
    char request[1024];
    // usamos essa função SSL_read para pegar os dados do socket
    int bytes_received = SSL_read(ssl, request, 1024);
    printf("Recebendo %d bytes.\n", bytes_received);

    printf("Enviando resposta...\n");
    const char *response = "HTTP/1.1 200 OK\r\n"
                           "Connection: close\r\n"
                           "Content-Type: text/plain\r\n\r\n"
                           "Local time is: ";
    // usado para enviar msg pelo tunel SSL
    int bytes_sent = SSL_write(ssl, response, strlen(response));
    printf("Enviado %d de %d bytes.\n", bytes_sent, (int)strlen(response));

    time_t timer;
    time(&timer);
    char *time_msg = ctime(&timer);
    bytes_sent = SSL_write(ssl, time_msg, strlen(time_msg));
    printf("Enviado %d de %d bytes.\n", bytes_sent, (int)strlen(time_msg));

    printf("Fechando conexão...\n");
    SSL_shutdown(ssl);
    CLOSESOCKET(socket_client);
    SSL_free(ssl);
  }

  printf("Fechando socket...\n");
  CLOSESOCKET(socket_listen);
  SSL_CTX_free(ctx);

#if defined(_WIN32)
  WSACleanup();
#endif

  printf("Finalizado.\n");

  return 0;
}
