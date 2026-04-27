#include "logger.hpp"
#include <iostream>

int main() {
    Logger logger("[APP]: ");
    logger.log("Hello");
    std::cout << "End of main\n";
}