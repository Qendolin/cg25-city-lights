#include <cpptrace/utils.hpp>
#include <filesystem>
#include <iostream>

#include "Application.h"
#include "util/globals.h"

int main() {

    if (!std::filesystem::exists("resources")) {
        std::cerr << "Directory 'resources' not found. Current working directory is '"
                  << std::filesystem::absolute(std::filesystem::current_path()) << "'" << std::endl;
        return EXIT_FAILURE;
    }

    if (!globals::Debug) {
        // ReSharper disable once CppDeprecatedEntity
        auto evn_var = std::getenv("DEBUG");
        if (evn_var != nullptr && std::strcmp(evn_var, "1") == 0) {
            globals::Debug = true;
            std::cerr << "Debug mode enabled via DEBUG env var." << std::endl;
        }
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
