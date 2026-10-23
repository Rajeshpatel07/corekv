#include "core/network.h"
#include <exception>
#include <iostream>
#include <vector>

int main(int argc, char **argv) {
  try {
    Corekv::Server server(8000);
  } catch (std::exception &e) {
    std::cerr << e.what() << "\n";
  }
}
