#include <openssl/ssl.h>

int main(int argc, char *argv[]) {

  printf("OpenSSL versão: %s\n", OpenSSL_version(OPENSSL_VERSION));
  return 0;
}
