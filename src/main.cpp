#include "server/server.h"
#include <exception>
#include <iostream>

int main(int argc, char **argv) {
  try {
    corekv::server::Server app;
  } catch (std::exception &e) {
    std::cerr << e.what() << "\n";
  }
}
