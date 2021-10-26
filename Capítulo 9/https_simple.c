#include "chap09.h"

int main(int argc, char *argv[]) {
  // A macro abaixo é usada para inicar sockets em sistemas windows
#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Falha ao incializar.\n");
    return 1;
  }
#endif
  // iniciamos a biblioteca SSL
  SSL_library_init();
  // Carregamos todos os algoritmos disponíveis
  OpenSSL_add_all_algorithms();
  // Chamamos a função para lidar com erros no OpenSSL
  SSL_load_error_strings();
  // Criamos um objeto SSL, ele funciona como uma espécie de fábrica
  // para conexões SSL/TLS
  SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
  if (!ctx) {
    fprintf(stderr, "SSL_CTX_new() falhou.\n");
    return 1;
  }

  if (argc < 3) {
    fprintf(stderr, "usage: https_simple hostname port\n");
    return 1;
  }

  // É possível reutilizar os  objetos SSL_CTX em mais de uma conexão
  char *hostname = argv[1];
  char *port = argv[2];

  // O código abaixo já nos é conhecido
  printf("Confugurando endereço remoto...\n");
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

  // Agora iniciaremos a parte diferente, usando o SSL
  SSL *ssl = SSL_new(ctx);
  if (!ctx) {
    fprintf(stderr, "SSL_new() falhou.\n");
    return 1;
  }
  // Inserimos qual host desejamos nos conectar
  if (!SSL_set_tlsext_host_name(ssl, hostname)) {
    fprintf(stderr, "SSL_set_tlsext_host_name() falhou.\n");
    ERR_print_errors_fp(stderr);
    return 1;
  }
  // Inserimos nosso socket na lista e tentamos nos conectar
  SSL_set_fd(ssl, server);
  if (SSL_connect(ssl) == -1) {
    fprintf(stderr, "SSL_connect() falhou.\n");
    ERR_print_errors_fp(stderr);
    return 1;
  }
  // Usamos essa função apenas para verificar qual cifra o ssl esta usando
  printf("SSL/TLS usando %s\n", SSL_get_cipher(ssl));

  // Apos, printamos o certificado usado pelo servidor
  X509 *cert = SSL_get_peer_certificate(ssl);
  if (!cert) {
    fprintf(stderr, "SSL_get_peer_certificate() falhou.\n");
    return 1;
  }

  char *tmp;
  if ((tmp = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0))) {
    printf("subject: %s\n", tmp);
    OPENSSL_free(tmp);
  }

  if ((tmp = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0))) {
    printf("issuer: %s\n", tmp);
    OPENSSL_free(tmp);
  }

  X509_free(cert);
  // Agora podemos escrever a solicitação web

  char buffer[2048];

  sprintf(buffer, "GET / HTTP/1.1\r\n");
  sprintf(buffer + strlen(buffer), "Host: %s:%s\r\n", hostname, port);
  sprintf(buffer + strlen(buffer), "Connection: close\r\n");
  sprintf(buffer + strlen(buffer), "User-Agent: https_simple\r\n");
  sprintf(buffer + strlen(buffer), "\r\n");

  // Escrita, basta enviar utilizando SSL_write
  SSL_write(ssl, buffer, strlen(buffer));
  printf("Enviando Headers:\n%s", buffer);
  // Após, ficamos verificando para ver se a entrada retornou para
  // bytes_receveid, caso afirmativo fechamos o socket
  while (1) {
    int bytes_received = SSL_read(ssl, buffer, sizeof(buffer));
    if (bytes_received < 1) {
      printf("\nConexão fechada pelo par.\n");
      break;
    }

    printf("(%d bytes) Recebidos: '%.*s'\n", bytes_received, bytes_received,
           buffer);

  } // end while(1)

  printf("\nFechando socket...\n");
  SSL_shutdown(ssl);
  CLOSESOCKET(server);
  SSL_free(ssl);
  SSL_CTX_free(ctx);

#if defined(_WIN32)
  WSACleanup();
#endif

  printf("Fechando.\n");
  return 0;
}
