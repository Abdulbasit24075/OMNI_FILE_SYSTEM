/**
 * @file main.cpp
 * @brief Entry point for the OFS Server
 * @location source/server/main.cpp
 */

#include "../include/ofs_server.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "      OFS SERVER (Phase 1) STARTUP      " << std::endl;
    std::cout << "========================================" << std::endl;

    // Default paths
    std::string config_path = "compiled/default.uconf";
    std::string omni_path = "omni_fs.omni";

    // Allow command line overrides (Optional)
    if (argc > 1) config_path = argv[1];
    if (argc > 2) omni_path = argv[2];

    // 1. Instantiate Server
    // Note: 8080 is the default port, but it will be overridden by default.uconf
    OFSServer server(8080, omni_path);

    // 2. Initialize File System
    // This loads the User Tree (AVL), File Tree (N-ary), and Bitmap
    std::cout << "[MAIN] Initializing File System from: " << omni_path << std::endl;
    OFSErrorCodes status = server.init(config_path);

    if (status != OFSErrorCodes::SUCCESS) {
        std::cerr << "[MAIN] Critical Error: Failed to initialize file system. Error Code: " 
                  << static_cast<int>(status) << std::endl;
        return -1;
    }

    // 3. Start the Server Loop
    // This starts the Socket Listener and the FIFO Queue Worker
    std::cout << "[MAIN] System Ready. Starting Server Loop..." << std::endl;
    server.run();

    return 0;
}