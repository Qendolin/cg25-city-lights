#include <iostream>
#include <cpptrace/utils.hpp>
#include <filesystem>

#include "Application.h"

int main() {

    if (!std::filesystem::exists("resources")) {
        std::cerr << "Directory 'resources' not found. Current working directory is '" << std::filesystem::absolute(std::filesystem::current_path()) << "'" << std::endl;
        return EXIT_FAILURE;
    }

    try {
        Application app;
        app.run();
    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        if (cpptrace::isatty(_fileno(stdout))) {
            std::cout << "Application crashed. Press Enter to terminate..." << std::endl;
            std::cin.get();
        }
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
