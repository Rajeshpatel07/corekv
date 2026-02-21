#include "net/server.hpp"

int main() {
  corekv::Server server;
  server.start(8000);
  return 0;
}
