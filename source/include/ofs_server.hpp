/**
 * @file ofs_server.h
 * @brief Main Server Class definitions
 * @location source/include/ofs_server.h
 */

#ifndef OFS_SERVER_H
#define OFS_SERVER_H

#include "odf_types.hpp"      // Use the official types
#include "ofs_structures.hpp"   // Use our custom AVL/N-ary trees
#include <queue>
#include <mutex>
#include <string>
#include <fstream>
#include <map>

// Structure for a queued client request
struct ClientRequest {
    int client_socket;
    std::string json_payload;
};

class OFSServer {
private:
    // -- Components --
    std::string omni_file_path;
    std::fstream file_stream;   // The actual .omni file handle
    
    // -- In-Memory Data Structures --
    OMNIHeader header;
    UserAVLTree userTree;       // DSA: AVL Tree
    FileSystemTree fileTree;    // DSA: N-ary Tree
    BlockManager* blockManager; // DSA: Bitmap

    // -- Networking & Queue --
    int server_socket;
    int port;
    bool is_running;
    
    // FIFO Queue components
    std::queue<ClientRequest> requestQueue;
    std::mutex queue_mutex;     // Thread safety for the queue

    // -- Internal Helpers --
    void processRequest(ClientRequest req); // The "Core Logic"
    void loadFileSystem();      // fs_init: Reads disk -> populates Trees
    void saveFileSystem();      // Writes Trees -> disk
    
    // Parsing the config file
    void loadConfig(std::string config_path);
    
    std::map<std::string, std::string> active_sessions; // Maps session_id -> username
    
    // Helper to convert "Virtual Path" (/) -> "Physical Path" (/home/alice)
    std::string translatePath(std::string client_path, std::string session_id);

public:
    OFSServer(int port, std::string omni_path);
    ~OFSServer();

    // Lifecycle
    OFSErrorCodes init(std::string config_path); // Opens file, loads AVL/N-ary trees
    void run();      // Main loop: Accepts clients -> pushes to Queue
    void worker();   // Worker loop: Pops from Queue -> processRequest()
    void shutdown();
};

#endif // OFS_SERVER_H