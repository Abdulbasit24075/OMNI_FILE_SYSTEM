/**
 * @file ofs_server.cpp
 * @brief Server Logic with Multi-User Isolation (Jail) & Full Features
 * @location source/server/core/ofs_server.cpp
 */

#include "../../include/ofs_server.hpp"
#include <iostream>
#include <cstring>
#include <sstream>
#include <thread>
#include <chrono>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <iomanip>

// ============================================================================
// HELPERS
// ============================================================================

// Recursive helper to count files and folders
void countEntries(FSNode* node, int& files, int& dirs) {
    if (!node) return;
    
    if (node->metadata.getType() == EntryType::DIRECTORY) {
        if (std::string(node->metadata.name) != "/") dirs++; 
        for (FSNode* child : node->children) {
            countEntries(child, files, dirs);
        }
    } else {
        files++;
    }
}

std::string cleanString(std::string val) {
    size_t first = val.find_first_not_of(" \t\"\n\r");
    if (std::string::npos == first) return "";
    size_t last = val.find_last_not_of(" \t\"\n\r");
    return val.substr(first, (last - first + 1));
}

std::string getJsonValue(std::string json, std::string key) {
    std::string search_key = "\"" + key + "\"";
    size_t key_pos = json.find(search_key);
    if (key_pos == std::string::npos) return "";
    
    size_t colon_pos = json.find(':', key_pos);
    if (colon_pos == std::string::npos) return "";
    
    size_t value_start = json.find_first_not_of(" \t\n\r", colon_pos + 1);
    if (value_start == std::string::npos) return "";
    
    bool is_string = (json[value_start] == '"');
    size_t value_end;
    if (is_string) value_end = json.find('"', value_start + 1);
    else value_end = json.find_first_of(",}", value_start);
    
    if (value_end == std::string::npos) return "";
    return cleanString(json.substr(value_start, value_end - value_start + (is_string ? 1 : 0)));
}

std::string simpleHash(std::string password) {
    if (password == "admin123") return "8c6976e5b5410415bde908bd4dee15df";
    return "password123"; // Simplified
}

// ============================================================================
// SERVER CORE
// ============================================================================

OFSServer::OFSServer(int p, std::string path) 
    : omni_file_path(path), port(p), is_running(false), blockManager(nullptr) {
}

OFSServer::~OFSServer() {
    shutdown();
    if (blockManager) delete blockManager;
}

// --- PATH TRANSLATION (THE JAIL LOGIC) ---
std::string OFSServer::translatePath(std::string client_path, std::string session_id) {
    // 1. Validate Session
    if (active_sessions.find(session_id) == active_sessions.end()) {
        // No session found. 
        return ""; 
    }
    std::string username = active_sessions[session_id];

    // 2. ADMIN: God Mode (Sees everything)
    if (username == "admin") {
        return client_path;
    }

    // 3. REGULAR USER: Is Jailed inside /home/{username}
    std::string jail_root = "/home/" + username;

    // Security: Prevent ".." traversal
    if (client_path.find("..") != std::string::npos) {
        return ""; 
    }

    // Handle Root
    if (client_path == "/" || client_path.empty()) {
        return jail_root;
    }

    // Ensure virtual path starts with /
    if (client_path[0] != '/') {
        client_path = "/" + client_path;
    }

    // Combine: Jail + Virtual Path
    // Example: "/home/alice" + "/docs/note.txt"
    return jail_root + client_path;
}

void OFSServer::loadConfig(std::string config_path) {
    std::ifstream conf(config_path);
    if (!conf.is_open()) {
        std::cerr << "[ERROR] Could not open config file." << std::endl;
        return;
    }
    std::string line;
    std::map<std::string, std::string> settings;
    while (std::getline(conf, line)) {
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#' || line[first] == '[') continue;
        line = line.substr(first);
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string key = cleanString(line.substr(0, eq));
            std::string val = cleanString(line.substr(eq + 1));
            settings[key] = val;
        }
    }
    if (settings.count("port")) port = std::stoi(settings["port"]);
    std::cout << "[CONFIG] Loaded configuration. Port: " << port << std::endl;
}

OFSErrorCodes OFSServer::init(std::string config_path) {
    loadConfig(config_path);

    file_stream.open(omni_file_path, std::ios::in | std::ios::out | std::ios::binary);
    
    if (!file_stream.is_open()) {
        std::cout << "[INFO] Creating NEW Multi-User File System..." << std::endl;
        std::ofstream create(omni_file_path, std::ios::binary);
        if (!create) return OFSErrorCodes::ERROR_IO_ERROR;
        
        uint64_t total_size = 104857600; 
        uint64_t block_size = 4096;
        
        header = OMNIHeader(0x00010000, total_size, 512, block_size);
        strcpy(header.magic, "OMNIFS01");
        header.user_table_offset = block_size * 1; 
        header.max_users = 50;
        
        UserInfo admin("admin", "8c6976e5b5410415bde908bd4dee15df", UserRole::ADMIN, std::time(nullptr));
        
        // 1. Create ROOT (/) at Block 2
        FileEntry root("/", EntryType::DIRECTORY, 0, 0755, "admin", 0, 0);
        uint32_t root_block = 2;
        std::memcpy(root.reserved, &root_block, sizeof(uint32_t));

        // 2. Create HOME (/home) inside Root. Assign it Block 3.
        FileEntry homeDir("home", EntryType::DIRECTORY, 0, 0755, "admin", 0, root.inode);
        uint32_t home_block = 3; // Assign Block 3 to /home listing
        std::memcpy(homeDir.reserved, &home_block, sizeof(uint32_t));

        // --- WRITING INITIAL STRUCTURE ---
        
        // Write Header (Block 0)
        create.seekp(0);
        create.write(reinterpret_cast<char*>(&header), sizeof(OMNIHeader));
        
        // Write Users (Block 1)
        create.seekp(header.user_table_offset);
        create.write(reinterpret_cast<char*>(&admin), sizeof(UserInfo));
        
        // Write Root Data (Block 2) - Contains "home" entry
        create.seekp(header.block_size * root_block);
        create.write(reinterpret_cast<char*>(&homeDir), sizeof(FileEntry));
        char empty[4096] = {0};
        create.write(empty, 4096 - sizeof(FileEntry));

        // Write Home Data (Block 3) - Initially Empty
        create.seekp(header.block_size * home_block);
        create.write(empty, 4096);
        
        create.seekp(total_size - 1);
        create.write("", 1);
        create.close();
        
        // Re-open
        file_stream.open(omni_file_path, std::ios::in | std::ios::out | std::ios::binary);
        
        userTree.insert(admin);
        fileTree.setRoot(root);
        
        // Manually add "home" to memory tree since we just created it
        fileTree.addChild(fileTree.getRoot(), homeDir);

        blockManager = new BlockManager(total_size / block_size); 
        blockManager->markUsed(0, 4); // Reserve 0,1,2,3 (Header, Users, Root, Home)

        std::cout << "[INFO] Formatted. Created / and /home." << std::endl;
    } else {
        std::cout << "[INFO] Loading existing File System..." << std::endl;
        loadFileSystem();
    }
    return OFSErrorCodes::SUCCESS;
}

void OFSServer::loadFileSystem() {
    file_stream.seekg(0);
    file_stream.read(reinterpret_cast<char*>(&header), sizeof(OMNIHeader));
    
    if (strncmp(header.magic, "OMNIFS01", 8) != 0) {
        std::cerr << "[CRITICAL] Invalid .omni file format!" << std::endl;
        return;
    }
    
    uint64_t blk_size = (header.block_size > 0) ? header.block_size : 4096;
    blockManager = new BlockManager(header.total_size / blk_size);
    
    // Reserve System Blocks
    blockManager->markUsed(0, 4); // Header, Users, Root, Home

    // Load Users
    file_stream.seekg(header.user_table_offset);
    for(uint32_t i=0; i < header.max_users; i++) {
        UserInfo u;
        file_stream.read(reinterpret_cast<char*>(&u), sizeof(UserInfo));
        if (u.is_active && u.username[0] != '\0') {
            userTree.insert(u);
        }
    }
    
    // Load Root
    FileEntry root("/", EntryType::DIRECTORY, 0, 0755, "admin", 0, 0);
    uint32_t root_block = 2;
    std::memcpy(root.reserved, &root_block, sizeof(uint32_t));
    fileTree.setRoot(root);
    
    // RECURSIVE LOAD (Depth 2: Root -> Home -> Users)
    // 1. Load Children of Root (should find "home")
    file_stream.seekg(root_block * blk_size);
    int max_entries = blk_size / sizeof(FileEntry);
    
    for(int i=0; i < max_entries; i++) {
        FileEntry entry;
        file_stream.read(reinterpret_cast<char*>(&entry), sizeof(FileEntry));
        if (entry.name[0] != '\0') {
            FSNode* child = fileTree.addChild(fileTree.getRoot(), entry);
            
            // If child is "home", load its children (User Directories)
            if (child && std::string(entry.name) == "home") {
                uint32_t h_block = 0;
                std::memcpy(&h_block, entry.reserved, sizeof(uint32_t));
                
                // Save Position
                std::streampos old_pos = file_stream.tellg(); 
                file_stream.seekg(h_block * blk_size);
                
                for(int j=0; j < max_entries; j++) {
                    FileEntry userDir;
                    file_stream.read(reinterpret_cast<char*>(&userDir), sizeof(FileEntry));
                    if (userDir.name[0] != '\0') {
                        fileTree.addChild(child, userDir);
                        
                        // Mark block used
                         uint32_t ub = 0;
                         std::memcpy(&ub, userDir.reserved, sizeof(uint32_t));
                         if (ub > 3) blockManager->markUsed(ub, 1);
                    }
                }
                // Restore Position
                file_stream.seekg(old_pos);
            }
        }
    }
    std::cout << "[INFO] File System Loaded." << std::endl;
}

void OFSServer::shutdown() {
    is_running = false;
    if (file_stream.is_open()) file_stream.close();
    if (server_socket > 0) close(server_socket);
}

void OFSServer::run() {
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) return;

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) { perror("Bind"); return; }
    if (listen(server_socket, 10) < 0) return;

    is_running = true;
    std::cout << "[SERVER] Listening on port " << port << "..." << std::endl;

    std::thread workerThread(&OFSServer::worker, this);
    workerThread.detach();

    while (is_running) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) continue;

        char buffer[8192];
        memset(buffer, 0, 8192);
        int bytes_read = read(client_sock, buffer, 8191);

        if (bytes_read > 0) {
            std::lock_guard<std::mutex> lock(queue_mutex);
            requestQueue.push({client_sock, std::string(buffer)});
        } else {
            close(client_sock);
        }
    }
}

void OFSServer::worker() {
    while (is_running) {
        ClientRequest req = {-1, ""};
        bool has_req = false;
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (!requestQueue.empty()) {
                req = requestQueue.front();
                requestQueue.pop();
                has_req = true;
            }
        }
        if (has_req) {
            processRequest(req);
            close(req.client_socket);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

// ============================================================================
// PROCESS REQUEST (WITH TRANSLATION & FEATURES)
// ============================================================================

void OFSServer::processRequest(ClientRequest req) {
    std::string json = req.json_payload;
    std::string op = getJsonValue(json, "operation");
    std::string rid = getJsonValue(json, "request_id");
    std::string sid = getJsonValue(json, "session_id"); 
    std::string resp;

    std::cout << "[OP] " << op << " | Sid: " << sid << std::endl;

    // --- LOGIN (Generate Session) ---
    if (op == "user_login") {
        std::string u = getJsonValue(json, "username");
        std::string p = getJsonValue(json, "password");
        UserInfo* user = userTree.search(u);
        
        if (user && std::string(user->password_hash) == simpleHash(p)) {
            std::string new_sid = "sess_" + u + "_" + std::to_string(std::time(nullptr));
            active_sessions[new_sid] = u;
            resp = "{ \"status\": \"success\", \"operation\": \"user_login\", \"request_id\": \"" + rid + "\", \"data\": { \"session_id\": \"" + new_sid + "\", \"message\": \"Login Successful\" } }";
        } else {
            resp = "{ \"status\": \"error\", \"request_id\": \"" + rid + "\", \"error_code\": -2, \"error_message\": \"Invalid credentials\" }";
        }
    }
    // --- USER CREATE (Auto-Home Provisioning) ---
    else if (op == "user_create") {
        std::string u = getJsonValue(json, "username");
        std::string p = getJsonValue(json, "password");
        
        if (userTree.search(u)) {
             resp = "{ \"status\": \"error\", \"request_id\": \"" + rid + "\", \"error_message\": \"User exists\" }";
        } else {
            UserInfo info(u, simpleHash(p), UserRole::NORMAL, std::time(nullptr));
            
            // Find User Slot
            uint64_t u_start = header.block_size;
            bool slot = false;
            for(uint32_t i=0; i < header.max_users; i++) {
                uint64_t off = u_start + (i * sizeof(UserInfo));
                UserInfo temp;
                file_stream.seekg(off);
                file_stream.read(reinterpret_cast<char*>(&temp), sizeof(UserInfo));
                if (temp.username[0] == '\0' || temp.is_active == 0) {
                    file_stream.seekp(off);
                    file_stream.write(reinterpret_cast<char*>(&info), sizeof(UserInfo));
                    slot = true;
                    break;
                }
            }
            
            if (slot) {
                userTree.insert(info);
                
                // PROVISION HOME DIRECTORY
                FSNode* homeNode = fileTree.resolvePath("/home");
                if (homeNode) {
                    // Create /home/{username}
                    FileEntry userHome(u, EntryType::DIRECTORY, 0, 0700, u, 0, homeNode->metadata.inode);
                    int db = blockManager->allocateBlocks(1); // Allocate block for user's files
                    
                    if (db != -1) {
                         uint32_t d_blk = (uint32_t)db;
                         std::memcpy(userHome.reserved, &d_blk, sizeof(uint32_t));
                         
                         // Init empty block
                         char empty[4096] = {0};
                         file_stream.seekp((uint64_t)d_blk * header.block_size);
                         file_stream.write(empty, 4096);
                         
                         if (fileTree.addChild(homeNode, userHome)) {
                             // Write to /home's block (Block 3)
                             uint32_t h_blk = 0;
                             std::memcpy(&h_blk, homeNode->metadata.reserved, sizeof(uint32_t));
                             uint64_t h_off = (uint64_t)h_blk * header.block_size;
                             int max_e = header.block_size / sizeof(FileEntry);
                             for(int k=0; k<max_e; k++) {
                                 FileEntry t;
                                 file_stream.seekg(h_off + (k * sizeof(FileEntry)));
                                 file_stream.read(reinterpret_cast<char*>(&t), sizeof(FileEntry));
                                 if (t.name[0] == '\0') {
                                     file_stream.seekp(h_off + (k * sizeof(FileEntry)));
                                     file_stream.write(reinterpret_cast<char*>(&userHome), sizeof(FileEntry));
                                     break;
                                 }
                             }
                             file_stream.flush();
                         }
                    }
                }
                resp = "{ \"status\": \"success\", \"operation\": \"user_create\", \"request_id\": \"" + rid + "\", \"data\": { \"message\": \"User and Home created\" } }";
            } else {
                resp = "{ \"status\": \"error\", \"error_message\": \"User table full\" }";
            }
        }
    }
    // --- USER LIST ---
    else if (op == "user_list") {
         std::vector<UserInfo> users = userTree.getAllUsers();
         std::string list = "[";
         for (size_t i=0; i<users.size(); ++i) {
             list += "{ \"username\": \"" + std::string(users[i].username) + "\", \"role\": " + (users[i].role == UserRole::ADMIN ? "\"admin\"" : "\"user\"") + " }";
             if (i < users.size() - 1) list += ", ";
         }
         list += "]";
         resp = "{ \"status\": \"success\", \"operation\": \"user_list\", \"request_id\": \"" + rid + "\", \"data\": { \"users\": " + list + " } }";
    }
    // --- USER DELETE ---
    else if (op == "user_delete") {
        std::string target = getJsonValue(json, "username");
        UserInfo* u = userTree.search(target);
        if (!u || std::string(u->username) == "admin") {
             resp = "{ \"status\": \"error\", \"error_message\": \"Invalid target\" }";
        } else {
            u->is_active = 0;
            uint64_t u_start = header.block_size;
            for(uint32_t i=0; i < header.max_users; i++) {
                uint64_t off = u_start + (i * sizeof(UserInfo));
                UserInfo temp;
                file_stream.seekg(off);
                file_stream.read(reinterpret_cast<char*>(&temp), sizeof(UserInfo));
                if (std::string(temp.username) == target) {
                    temp.is_active = 0;
                    file_stream.seekp(off);
                    file_stream.write(reinterpret_cast<char*>(&temp), sizeof(UserInfo));
                    file_stream.flush();
                    break;
                }
            }
            resp = "{ \"status\": \"success\", \"data\": { \"message\": \"User deleted\" } }";
        }
    }
    // --- GET STATS ---
    else if (op == "get_stats") {
        uint32_t free = blockManager->getFreeBlocksCount();
        uint32_t total = blockManager->getTotalBlocks();
        uint64_t fb = (uint64_t)free * header.block_size;
        uint64_t ub = (uint64_t)(total - free) * header.block_size;
        int fc = 0, dc = 0;
        countEntries(fileTree.getRoot(), fc, dc);
        
        resp = "{ \"status\": \"success\", \"operation\": \"get_stats\", \"data\": { \"stats\": { \"total_size\": " + std::to_string(header.total_size) + ", \"used_space\": " + std::to_string(ub) + ", \"free_space\": " + std::to_string(fb) + ", \"total_files\": " + std::to_string(fc) + ", \"total_directories\": " + std::to_string(dc) + " } } }";
    }
    // --- TRANSLATED OPERATIONS (FILE/DIR) ---
    else {
        std::string v_path = getJsonValue(json, "path");
        
        // *** TRANSLATE PATH ***
        std::string r_path = translatePath(v_path, sid);
        
        if (r_path.empty()) {
            resp = "{ \"status\": \"error\", \"request_id\": \"" + rid + "\", \"error_message\": \"Access Denied / Invalid Session\" }";
        } else {
            // Debug output to see translation working
            std::cout << "   -> Jail Translation: " << v_path << " => " << r_path << std::endl;

            // 1. DIR LIST
            if (op == "dir_list") {
                std::vector<FileEntry> files = fileTree.listDirectory(r_path);
                std::string list = "[";
                for (size_t i = 0; i < files.size(); ++i) {
                    list += "{ \"name\": \"" + std::string(files[i].name) + "\", \"type\": " + (files[i].getType() == EntryType::DIRECTORY ? "\"dir\"" : "\"file\"") + " }";
                    if (i < files.size() - 1) list += ", ";
                }
                list += "]";
                resp = "{ \"status\": \"success\", \"operation\": \"dir_list\", \"request_id\": \"" + rid + "\", \"data\": { \"files\": " + list + " } }";
            }
            // 2. FILE READ
            else if (op == "file_read") {
                FSNode* node = fileTree.resolvePath(r_path);
                if (!node || node->metadata.getType() == EntryType::DIRECTORY) {
                     resp = "{ \"status\": \"error\", \"error_message\": \"File not found\" }";
                } else {
                    uint32_t s_block = 0;
                    std::memcpy(&s_block, node->metadata.reserved, sizeof(uint32_t));
                    uint64_t offset = (uint64_t)s_block * header.block_size;
                    char* buf = new char[node->metadata.size + 1];
                    file_stream.seekg(offset);
                    file_stream.read(buf, node->metadata.size);
                    buf[node->metadata.size] = '\0';
                    std::string content(buf);
                    delete[] buf;
                    resp = "{ \"status\": \"success\", \"operation\": \"file_read\", \"data\": { \"content\": \"" + content + "\" } }";
                }
            }
            // 3. FILE DELETE
            else if (op == "file_delete") {
                FSNode* node = fileTree.resolvePath(r_path);
                if (!node) resp = "{ \"status\": \"error\", \"error_message\": \"Not Found\" }";
                else {
                     uint32_t sb = 0;
                     std::memcpy(&sb, node->metadata.reserved, sizeof(uint32_t));
                     if(sb > 3) blockManager->freeBlocks(sb, (node->metadata.size/4096)+1);
                     
                     // Remove from Disk (Parent)
                     FSNode* parent = node->parent;
                     if (parent) {
                         uint32_t pb = 0;
                         std::memcpy(&pb, parent->metadata.reserved, sizeof(uint32_t));
                         uint64_t po = (uint64_t)pb * header.block_size;
                         int me = header.block_size / sizeof(FileEntry);
                         for(int i=0; i<me; i++) {
                             FileEntry t;
                             file_stream.seekg(po + (i*sizeof(FileEntry)));
                             file_stream.read(reinterpret_cast<char*>(&t), sizeof(FileEntry));
                             if (std::string(t.name) == std::string(node->metadata.name)) {
                                 FileEntry empty; memset(&empty, 0, sizeof(FileEntry));
                                 file_stream.seekp(po + (i*sizeof(FileEntry)));
                                 file_stream.write(reinterpret_cast<char*>(&empty), sizeof(FileEntry));
                                 break;
                             }
                         }
                         file_stream.flush();
                     }
                     fileTree.removeChild(node->parent, node->metadata.name);
                     resp = "{ \"status\": \"success\", \"data\": { \"message\": \"Deleted\" } }";
                }
            }
            else if (op == "dir_delete") {
                FSNode* node = fileTree.resolvePath(r_path);
                if (!node) resp = "{ \"status\": \"error\", \"error_message\": \"Not Found\" }";
                else if (node->metadata.getType() != EntryType::DIRECTORY) {
                    resp = "{ \"status\": \"error\", \"error_message\": \"Not a dir\" }";
                } else if (!node->children.empty()) {
                    resp = "{ \"status\": \"error\", \"error_message\": \"Directory not empty\" }";
                } else {
                     uint32_t db = 0;
                     std::memcpy(&db, node->metadata.reserved, sizeof(uint32_t));
                     if(db > 3) blockManager->freeBlocks(db, 1);
                     FSNode* parent = node->parent;
                     if (parent) {
                         uint32_t pb = 0;
                         std::memcpy(&pb, parent->metadata.reserved, sizeof(uint32_t));
                         uint64_t po = (uint64_t)pb * header.block_size;
                         int me = header.block_size / sizeof(FileEntry);
                         for(int i=0; i<me; i++) {
                             FileEntry t;
                             file_stream.seekg(po + (i*sizeof(FileEntry)));
                             file_stream.read(reinterpret_cast<char*>(&t), sizeof(FileEntry));
                             if (std::string(t.name) == std::string(node->metadata.name)) {
                                 FileEntry empty; memset(&empty, 0, sizeof(FileEntry));
                                 file_stream.seekp(po + (i*sizeof(FileEntry)));
                                 file_stream.write(reinterpret_cast<char*>(&empty), sizeof(FileEntry));
                                 break;
                             }
                         }
                         file_stream.flush();
                     }
                     fileTree.removeChild(node->parent, node->metadata.name);
                     resp = "{ \"status\": \"success\", \"data\": { \"message\": \"Deleted\" } }";
                }
            }
            // 4. FILE CREATE
            else if (op == "file_create") {
                std::string content = getJsonValue(json, "data");
                std::string type_str = getJsonValue(json, "type");
                
                size_t ls = r_path.find_last_of('/');
                std::string p_path = r_path.substr(0, ls);
                std::string fname = r_path.substr(ls + 1);
                FSNode* parent = fileTree.resolvePath(p_path);
                
                if (!parent) {
                    resp = "{ \"status\": \"error\", \"error_message\": \"Parent not found\" }";
                } else {
                    int blks = (content.length() / 4096) + 1;
                    int sb = blockManager->allocateBlocks(blks);
                    if (sb == -1) resp = "{ \"status\": \"error\", \"error_message\": \"Disk full\" }";
                    else {
                        FileEntry nf(fname, (type_str=="dir"?EntryType::DIRECTORY:EntryType::FILE), content.length(), 0600, active_sessions[sid], 0, parent->metadata.inode);
                        uint32_t s_b = (uint32_t)sb;
                        std::memcpy(nf.reserved, &s_b, sizeof(uint32_t));
                        
                        if (fileTree.addChild(parent, nf)) {
                            if (type_str != "dir") {
                                file_stream.seekp((uint64_t)s_b * header.block_size);
                                file_stream.write(content.c_str(), content.length());
                            } else {
                                char e[4096] = {0}; // Init dir block
                                file_stream.seekp((uint64_t)s_b * header.block_size);
                                file_stream.write(e, 4096);
                            }
                            
                            uint32_t pb = 0;
                            std::memcpy(&pb, parent->metadata.reserved, sizeof(uint32_t));
                            uint64_t po = (uint64_t)pb * header.block_size;
                            int me = header.block_size / sizeof(FileEntry);
                            for(int i=0; i<me; i++) {
                                FileEntry t;
                                file_stream.seekg(po + (i*sizeof(FileEntry)));
                                file_stream.read(reinterpret_cast<char*>(&t), sizeof(FileEntry));
                                if (t.name[0] == '\0') {
                                    file_stream.seekp(po + (i*sizeof(FileEntry)));
                                    file_stream.write(reinterpret_cast<char*>(&nf), sizeof(FileEntry));
                                    break;
                                }
                            }
                            file_stream.flush();
                            resp = "{ \"status\": \"success\", \"operation\": \"file_create\", \"request_id\": \"" + rid + "\", \"data\": { \"message\": \"Created\" } }";
                        } else {
                            resp = "{ \"status\": \"error\", \"error_message\": \"Exists\" }";
                        }
                    }
                }
            }
            // 5. DIR CREATE
            else if (op == "dir_create") {
                // Same logic as file_create but with forced Directory type
                // Simplified: Reuse file_create logic above by sending type="dir" in JSON
                // Or implement explicit handler here if needed.
                // For brevity, client should send type="dir" to file_create logic which handles it.
                resp = "{ \"status\": \"error\", \"error_message\": \"Use file_create with type=dir\" }";
            }
            else {
                 resp = "{ \"status\": \"error\", \"error_message\": \"Unknown OP\" }";
            }
        }
    }
    write(req.client_socket, resp.c_str(), resp.length());
}