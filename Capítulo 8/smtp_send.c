#include "chap08.h"
#include <ctype.h>
#include <stdarg.h>

#define MAXINPUT 512
#define MAXRESPONSE 1024

void get_input(const char *prompt, char *buffer) {
  // Essa função é usada para pegar informações do usuário
  // Preste atenção que, ela usa a depreciada função fgets
  // Num exemplo atualizado o ideal seria usar scanf pois fgets
  // É vulnerável a Buffer Overflow.
  printf("%s", prompt);

  buffer[0] = 0;
  fgets(buffer, MAXINPUT, stdin);
  const int read = strlen(buffer);
  // Substituimos o ultimo caracter da string "\n" por um NULL
  // O que indica o fim da string.
  if (read > 0)
    buffer[read - 1] = 0;
}

void send_format(SOCKET server, const char *text, ...) {
  // Essa função é utilizada para enviar strings pela rede
  char buffer[1024];
  // va_list é usado para recuperar parametros adicionais de funções
  // Então, quando não se sabe a quantidade de argumentos pode-se utilizar
  // Essa estrutura é usada para armazena-los
  va_list args;
  // em va_start precisamos indicar os valores de um va_list e do último
  // parametro da função antes dos ...
  va_start(args, text);
  // Armazenamos os dados em buffer
  vsprintf(buffer, text, args);
  // encerra a lista
  va_end(args);
  // envia os dados do buffer para o servidor
  send(server, buffer, strlen(buffer), 0);
  // printa o que há no buffer e o atribui ao usuário
  printf("C: %s", buffer);
}

int parse_response(const char *response) {
  const char *k = response;
  // Verificamos se um dos 3 primeiros caracteres é nulo, se for
  // finalizamos a função pois a resposta não é longa o suficiente para
  // constituir uma resposta SMTP válida.
  if (!k[0] || !k[1] || !k[2])
    return 0;
  // Percorremos a string até encontrar um elemento nulo
  for (; k[3]; ++k) {
    // se o ultimo elemento for igual a pular linha
    if (k == response || k[-1] == '\n') {
      // e os três primeiros forem digitos
      if (isdigit(k[0]) && isdigit(k[1]) && isdigit(k[2])) {
        // se o quarto não for - então, k[0] indica o fim da linha
        if (k[3] != '-') {
          // verificando se o final da linha foi recebida, ou seja
          // a mensagem inteira chegou
          if (strstr(k, "\r\n")) {
            // transformando o código de resposta em um inteiro
            return strtol(k, 0, 10);
          }
        }
      }
    }
  }
  return 0;
}

void wait_on_response(SOCKET server, int expecting) {
  // response é usado para armazenar a resposta do servido SMTP
  char response[MAXRESPONSE + 1];
  // um ponteiro p é usado para indicar o inicio do arquivo
  char *p = response;
  // end é usado para apontar para o fim dele
  char *end = response + MAXRESPONSE;
  // code é usado para indicar que não recebemos resposta ainda
  int code = 0;

  do {
    // tentamos pegar a resposta do servidor SMTP.
    // usamos end - p para indicar o tamanho do nosso buffer
    // O que deve ser feito para ter certeza que a resposta
    // nao ultrapassou o tamanho máximo
    int bytes_received = recv(server, p, end - p, 0);
    // Se o valor recebido for menor que 1
    if (bytes_received < 1) {
      fprintf(stderr, "A conexão caiu.\n");
      exit(1);
    }
    // Enviamos p para o fim da string de resposta
    p += bytes_received;
    // E colocamos um NULL para indicar o fim da string
    *p = 0;

    // Se p for o fim do buffer
    if (p == end) {
      fprintf(stderr, "A resposta recebida é muito grande:\n");
      fprintf(stderr, "%s", response);
      exit(1);
    }
    // Então, retornamos o código da resposta
    code = parse_response(response);
    // repetimos o processo enquanto não recebemos uma resposta
  } while (code == 0);

  // Se o código for diferente do esperado
  if (code != expecting) {
    fprintf(stderr, "Erro no servidor:\n");
    fprintf(stderr, "%s", response);
    exit(1);
  }

  printf("S: %s", response);
}

SOCKET connect_to_host(const char *hostname, const char *port) {
  // Código muito conhecido e usado para realizar conexões com o host
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

int main() {
// Código padrão usado para iniciar socket em windows
#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Falha ao inicializar.\n");
    return 1;
  }
#endif

  char hostname[MAXINPUT];
  get_input("mail server: ", hostname);

  printf("Conectando ao host: %s:25\n", hostname);
  // Tentamos nos conectar com o servidor, o código de resposta aguardado é 220
  SOCKET server = connect_to_host(hostname, "25");
  wait_on_response(server, 220);
  // enviamos uma msg e esperamos um código 250
  send_format(server, "HELO HONPWC\r\n");
  wait_on_response(server, 250);

  char sender[MAXINPUT];
  get_input("from: ", sender);
  send_format(server, "MAIL FROM:<%s>\r\n", sender);
  wait_on_response(server, 250);

  char recipient[MAXINPUT];
  get_input("to: ", recipient);
  send_format(server, "RCPT TO:<%s>\r\n", recipient);
  wait_on_response(server, 250);

  send_format(server, "DATA\r\n");
  wait_on_response(server, 354);

  char subject[MAXINPUT];
  get_input("subject: ", subject);

  send_format(server, "From:<%s>\r\n", sender);
  send_format(server, "To:<%s>\r\n", recipient);
  send_format(server, "Subject:%s\r\n", subject);

  time_t timer;
  time(&timer);

  struct tm *timeinfo;
  timeinfo = gmtime(&timer);

  char date[128];
  strftime(date, 128, "%a, %d %b %Y %H:%M:%S +0000", timeinfo);

  send_format(server, "Date:%s\r\n", date);

  send_format(server, "\r\n");

  printf("Escreva seu email, finalize com \".\" e uma linha.\n");

  while (1) {
    char body[MAXINPUT];
    get_input("> ", body);
    send_format(server, "%s\r\n", body);
    if (strcmp(body, ".") == 0) {
      break;
    }
  }

  wait_on_response(server, 250);

  send_format(server, "QUIT\r\n");
  wait_on_response(server, 221);

  printf("\nFinalizando socket...\n");
  CLOSESOCKET(server);

#if defined(_WIN32)
  WSACleanup();
#endif

  printf("Finalizado.\n");
  return 0;
}
