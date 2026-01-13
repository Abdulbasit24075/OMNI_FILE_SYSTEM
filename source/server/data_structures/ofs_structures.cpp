/**
 * @file ofs_structures.cpp
 * @brief Implementation of AVL Tree, N-ary Tree, and Bitmap
 * @location source/server/data_structures/ofs_structures.cpp
 */

#include "../../include/ofs_structures.hpp" // Adjust path if necessary based on your build system
#include <iostream>
#include <sstream>
#include <cstring>

// ============================================================================
// 1. AVL Tree Implementation (User Management)
// ============================================================================

UserAVLTree::UserAVLTree() : root(nullptr) {}

UserAVLTree::~UserAVLTree() {
    destroyTree(root);
}

void UserAVLTree::destroyTree(UserNode* node) {
    if (node) {
        destroyTree(node->left);
        destroyTree(node->right);
        delete node;
    }
}

// Height helper (handles nullptr)
int UserAVLTree::height(UserNode* N) {
    if (N == nullptr)
        return 0;
    return N->height;
}

// Get Balance factor of node N
int UserAVLTree::getBalance(UserNode* N) {
    if (N == nullptr)
        return 0;
    return height(N->left) - height(N->right);
}

// Right Rotation (for Left-Left case)
UserNode* UserAVLTree::rightRotate(UserNode* y) {
    UserNode* x = y->left;
    UserNode* T2 = x->right;

    // Perform rotation
    x->right = y;
    y->left = T2;

    // Update heights
    y->height = std::max(height(y->left), height(y->right)) + 1;
    x->height = std::max(height(x->left), height(x->right)) + 1;

    // Return new root
    return x;
}

// Left Rotation (for Right-Right case)
UserNode* UserAVLTree::leftRotate(UserNode* x) {
    UserNode* y = x->right;
    UserNode* T2 = y->left;

    // Perform rotation
    y->left = x;
    x->right = T2;

    // Update heights
    x->height = std::max(height(x->left), height(x->right)) + 1;
    y->height = std::max(height(y->left), height(y->right)) + 1;

    // Return new root
    return y;
}

// Recursive Insert
UserNode* UserAVLTree::insertRec(UserNode* node, UserInfo info) {
    // 1. Perform the normal BST insertion
    if (node == nullptr)
        return new UserNode(info);

    // Convert char arrays to string for comparison
    std::string currentName(node->data.username);
    std::string newName(info.username);

    if (newName < currentName)
        node->left = insertRec(node->left, info);
    else if (newName > currentName)
        node->right = insertRec(node->right, info);
    else // Equal keys are not allowed in BST
        return node;

    // 2. Update height of this ancestor node
    node->height = 1 + std::max(height(node->left), height(node->right));

    // 3. Get the balance factor of this ancestor node
    int balance = getBalance(node);

    // 4. If this node becomes unbalanced, then there are 4 cases

    // Left Left Case
    if (balance > 1 && newName < std::string(node->left->data.username))
        return rightRotate(node);

    // Right Right Case
    if (balance < -1 && newName > std::string(node->right->data.username))
        return leftRotate(node);

    // Left Right Case
    if (balance > 1 && newName > std::string(node->left->data.username)) {
        node->left = leftRotate(node->left);
        return rightRotate(node);
    }

    // Right Left Case
    if (balance < -1 && newName < std::string(node->right->data.username)) {
        node->right = rightRotate(node->right);
        return leftRotate(node);
    }

    // Return the (unchanged) node pointer
    return node;
}

void UserAVLTree::insert(UserInfo info) {
    root = insertRec(root, info);
}

// Recursive Search
UserNode* UserAVLTree::searchRec(UserNode* root, std::string username) {
    if (root == nullptr) return nullptr;

    std::string currentName(root->data.username);

    if (currentName == username)
        return root;
    
    if (username < currentName)
        return searchRec(root->left, username);
    
    return searchRec(root->right, username);
}

UserInfo* UserAVLTree::search(std::string username) {
    UserNode* result = searchRec(root, username);
    if (result)
        return &(result->data);
    return nullptr;
}

// In-order traversal to get sorted list
void UserAVLTree::inorderRec(UserNode* root, std::vector<UserInfo>& list) {
    if (root != nullptr) {
        inorderRec(root->left, list);
        list.push_back(root->data);
        inorderRec(root->right, list);
    }
}

std::vector<UserInfo> UserAVLTree::getAllUsers() {
    std::vector<UserInfo> list;
    inorderRec(root, list);
    return list;
}


// ============================================================================
// 2. N-ary Tree Implementation (File System Hierarchy)
// ============================================================================

FileSystemTree::FileSystemTree() : root(nullptr), next_inode_counter(1) {}

FileSystemTree::~FileSystemTree() {
    destroyTree(root);
}

void FileSystemTree::destroyTree(FSNode* node) {
    if (node) {
        for (FSNode* child : node->children) {
            destroyTree(child);
        }
        delete node;
    }
}

void FileSystemTree::setRoot(FileEntry rootEntry) {
    if (root) destroyTree(root);
    root = new FSNode(rootEntry);
}

FSNode* FileSystemTree::getRoot() {
    return root;
}

// Helper to find a child node by name
FSNode* FileSystemTree::findChild(FSNode* parent, std::string name) {
    if (!parent) return nullptr;
    for (FSNode* child : parent->children) {
        if (std::string(child->metadata.name) == name) {
            return child;
        }
    }
    return nullptr;
}

// Path Resolution: Turns "/home/docs/file.txt" into the corresponding Node*
FSNode* FileSystemTree::resolvePath(std::string path) {
    if (!root || path.empty()) return nullptr;
    if (path == "/") return root;

    FSNode* current = root;
    std::stringstream ss(path);
    std::string segment;

    // Skip the first slash if present
    if (path[0] == '/') {
        // Just ensure we start at root, which is default
    }

    while (std::getline(ss, segment, '/')) {
        if (segment.empty()) continue; // Handle double slashes or trailing slash
        
        FSNode* next = findChild(current, segment);
        if (!next) return nullptr; // Path doesn't exist
        current = next;
    }

    return current;
}

FSNode* FileSystemTree::addChild(FSNode* parent, FileEntry entry) {
    if (!parent) return nullptr;
    
    // Check if child with same name already exists
    if (findChild(parent, std::string(entry.name))) {
        return nullptr; // Error: Already exists
    }

    // Set Inode and Parent Inode
    entry.inode = getNextInode();
    entry.parent_inode = parent->metadata.inode;

    FSNode* newNode = new FSNode(entry, parent);
    parent->children.push_back(newNode);
    return newNode;
}

bool FileSystemTree::removeChild(FSNode* parent, std::string name) {
    if (!parent) return false;

    auto it = parent->children.begin();
    while (it != parent->children.end()) {
        if (std::string((*it)->metadata.name) == name) {
            // Found it. Check if it's a directory, ensure it's empty
            if ((*it)->metadata.getType() == EntryType::DIRECTORY && !(*it)->children.empty()) {
                return false; // Error: Directory not empty
            }
            
            delete *it; // Free memory
            parent->children.erase(it); // Remove from vector
            return true;
        }
        ++it;
    }
    return false; // Not found
}

std::vector<FileEntry> FileSystemTree::listDirectory(std::string path) {
    FSNode* node = resolvePath(path);
    std::vector<FileEntry> entries;
    
    if (node && node->metadata.getType() == EntryType::DIRECTORY) {
        for (FSNode* child : node->children) {
            entries.push_back(child->metadata);
        }
    }
    return entries;
}


// ============================================================================
// 3. Bitmap Implementation (Free Space)
// ============================================================================

BlockManager::BlockManager(uint32_t num_blocks) : total_blocks(num_blocks), used_blocks_count(0) {
    // Initialize all blocks as free (false)
    bitmap.resize(num_blocks, false); 
    
    // Always mark block 0 as used (it's the Header)
    markUsed(0, 1);
}

// Find N consecutive free blocks
int BlockManager::allocateBlocks(int count) {
    if (count <= 0) return -1;
    
    int consecutive_found = 0;
    int start_index = -1;

    for (uint32_t i = 0; i < total_blocks; ++i) {
        if (!bitmap[i]) { // If block is free
            if (consecutive_found == 0) start_index = i;
            consecutive_found++;
            
            if (consecutive_found == count) {
                // Found enough space! Mark them as used.
                markUsed(start_index, count);
                return start_index;
            }
        } else {
            // Sequence broken, reset
            consecutive_found = 0;
            start_index = -1;
        }
    }
    
    return -1; // Not enough contiguous space found
}

void BlockManager::freeBlocks(int start_index, int count) {
    for (int i = 0; i < count; ++i) {
        int idx = start_index + i;
        if (idx < (int)total_blocks && bitmap[idx]) {
            bitmap[idx] = false;
            used_blocks_count--;
        }
    }
}

void BlockManager::markUsed(int start_index, int count) {
    for (int i = 0; i < count; ++i) {
        int idx = start_index + i;
        if (idx < (int)total_blocks && !bitmap[idx]) {
            bitmap[idx] = true;
            used_blocks_count++;
        }
    }
}

uint32_t BlockManager::getFreeBlocksCount() const {
    return total_blocks - used_blocks_count;
}

uint32_t BlockManager::getTotalBlocks() const {
    return total_blocks;
}