#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

int main() {
  // Aqui iniciamos uma estrutura usada para armazenar os endereços dos
  // dispositivos de rede
  struct ifaddrs *addresses;
  // Usando a função getifaddrs eles são armazenados no endereço de memória de
  // addresses É realizado uma verificação para ver se foi possível obter a
  // lista corretamente
  if (getifaddrs(&addresses) == -1) {
    printf("getifaddrs call failed");
    return -1;
  }
  // Este while é reponsável por printar os dados dos dispositivos
  while (addresses) {
    // Uma variável que armazena o tipo de família do dispositivo atual é
    // iniciada
    int family = addresses->ifa_addr->sa_family;
    // Verifica-se se o dispositivo é do tipo Ipv4 ou IPv6
    // Isto deve ocorrer para que não peguemos dispositivos de outro tipo a não
    // ser de rede
    if (family == AF_INET || family == AF_INET6) {
      // Printamos o nome e se ele é Ipv4 ou Ipv6
      printf("%s\t", addresses->ifa_name);
      printf("%s\t", family == AF_INET ? "IPv4" : "IPv6");
      char ap[100];
      // Para armazenar o endereço é necessário saber se ele é IPv6 ou IPv4
      // Por isto usa-se uma estrutura de decisão ternária para armazenar em
      // family_size O tamanho necessário para o endereço
      const int family_size = family == AF_INET ? sizeof(struct sockaddr_in)
                                                : sizeof(struct sockaddr_in6);
      // Esta função converte o endereço de um soquete em um host
      // O primeiro argumento é o ponteiro para a estrutura de socket
      // O segundo argumento é o tamanho do endereço IP da estrutura
      // O terceiro e quarto argumento são o local onde deve-se salvar
      // o resultado bem como seu tamanho
      // O quinto e sexto tem a mesma função dos dois anteriores mas para
      // service names O último argumento pede para getnameinfo retornar o
      // número do endereço da estrutura
      getnameinfo(addresses->ifa_addr, family_size, ap, sizeof(ap), 0, 0,
                  NI_NUMERICHOST);
      printf("\t%s\n", ap);
    }
    addresses = addresses->ifa_next;
    freeifaddrs(addresses);
    return 0;
  }
}
