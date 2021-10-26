#include "chap11.h"

int main(int argc, char *argv[]) {
  const char *hostname = 0;
  int port = 22;
  // verificamos se a quantidade de argumentos esta correta
  if (argc < 2) {
    fprintf(stderr, "Usage: ssh_connect hostname port\n");
    return 1;
  }

  hostname = argv[1];
  if (argc > 2)
    port = atol(argv[2]);

  // iniciamos um objeto SSH
  ssh_session ssh = ssh_new();
  if (!ssh) {
    fprintf(stderr, "ssh_new() falhou.\n");
    return 1;
  }
  // setamos, host e porta
  ssh_options_set(ssh, SSH_OPTIONS_HOST, hostname);
  ssh_options_set(ssh, SSH_OPTIONS_PORT, &port);
  int verbosity = SSH_LOG_PROTOCOL;
  // definimos que a lib deve usar o modo de depuração e printar praticamente
  // tudo o que faz
  ssh_options_set(ssh, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);

  // realizamos a conexão ssh
  int ret = ssh_connect(ssh);
  // se deu erro
  if (ret != SSH_OK) {
    fprintf(stderr, "ssh_connect() falhou.\n%s\n", ssh_get_error(ssh));
    return -1;
  }

  printf("Conectando a %s na porta %d.\n", hostname, port);

  printf("Banner:\n%s\n", ssh_get_serverbanner(ssh));

  ssh_disconnect(ssh);
  ssh_free(ssh);

  return 0;
}
