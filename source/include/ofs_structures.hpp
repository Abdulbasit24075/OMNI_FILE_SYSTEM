/**
 * @file ofs_structures.h
 * @brief In-Memory Data Structures (AVL Tree, N-ary Tree, Bitmap)
 * @location source/include/ofs_structures.h
 */

#ifndef OFS_STRUCTURES_H
#define OFS_STRUCTURES_H

// Include the official project types
#include "odf_types.hpp" 
#include <string>
#include <vector>
#include <algorithm>

// ============================================================================
// 1. AVL Tree (For User Management)
// Uses 'UserInfo' from ofs_types.hpp
// ============================================================================

struct UserNode {
    UserInfo data;          // The official UserInfo struct
    UserNode *left;
    UserNode *right;
    int height;

    UserNode(UserInfo info) : data(info), left(nullptr), right(nullptr), height(1) {}
};

class UserAVLTree {
private:
    UserNode* root;

    // Standard AVL Helpers
    int height(UserNode* N);
    int getBalance(UserNode* N);
    UserNode* rightRotate(UserNode* y);
    UserNode* leftRotate(UserNode* x);
    
    // Recursive operations
    UserNode* insertRec(UserNode* node, UserInfo info);
    UserNode* searchRec(UserNode* root, std::string username);
    void inorderRec(UserNode* root, std::vector<UserInfo>& list);
    
    // Helper for memory cleanup
    void destroyTree(UserNode* node);

public:
    UserAVLTree();
    ~UserAVLTree();

    void insert(UserInfo info);
    UserInfo* search(std::string username); // Returns pointer to data or nullptr
    std::vector<UserInfo> getAllUsers();    // Returns sorted list for Admin
};


// ============================================================================
// 2. N-ary Tree (For Directory Structure)
// Uses 'FileEntry' from ofs_types.hpp
// ============================================================================

struct FSNode {
    FileEntry metadata;            // The official FileEntry struct
    FSNode* parent;                // Pointer to parent directory
    std::vector<FSNode*> children; // List of children (Files/Dirs)

    // Constructor adapts to the provided FileEntry
    FSNode(FileEntry entry, FSNode* p = nullptr) : metadata(entry), parent(p) {}
};

class FileSystemTree {
private:
    FSNode* root;
    uint32_t next_inode_counter; // To assign unique inodes to new files

    // Helper to find a child by name in a specific node
    FSNode* findChild(FSNode* parent, std::string name);
    
    // Helper to collect all children recursively (for resolving paths)
    void destroyTree(FSNode* node);

public:
    FileSystemTree();
    ~FileSystemTree();
    
    // Initialization
    void setRoot(FileEntry rootEntry);
    FSNode* getRoot();
    
    // Traversal: Takes a path "/a/b/c" and returns the node
    FSNode* resolvePath(std::string path);
    
    // Management
    // Returns pointer to the created node on success, nullptr on failure
    FSNode* addChild(FSNode* parent, FileEntry entry); 
    
    bool removeChild(FSNode* parent, std::string name);
    std::vector<FileEntry> listDirectory(std::string path);
    
    // Inode management
    uint32_t getNextInode() { return next_inode_counter++; }
};


// ============================================================================
// 3. Bitmap (For Free Space Management)
// Tracks block usage for the .omni file
// ============================================================================

class BlockManager {
private:
    std::vector<bool> bitmap; // 0 = Free, 1 = Used
    uint32_t total_blocks;
    uint32_t used_blocks_count;

public:
    BlockManager(uint32_t num_blocks);
    
    // Allocates 'count' consecutive blocks. Returns start_index or -1.
    int allocateBlocks(int count);
    
    // Frees blocks starting at index
    void freeBlocks(int start_index, int count);
    
    // Helper to mark specific blocks as used (e.g., during fs_init loading)
    void markUsed(int start_index, int count);
    
    uint32_t getFreeBlocksCount() const;
    uint32_t getTotalBlocks() const;
};

#endif // OFS_STRUCTURES_H