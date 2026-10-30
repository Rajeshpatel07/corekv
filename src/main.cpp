#include "server/server.h"

#include <cstdlib>
#include <exception>
#include <iostream>

// main() only wires the application together. Constructing Server no longer
// starts an infinite loop, which keeps construction and execution separate and
// makes exceptions thrown during setup catchable here.
int main() {
  try {
    corekv::server::Server server;
    server.run();
  } catch (const std::exception &error) {
    std::cerr << "fatal: " << error.what() << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
