# OFS Phase 1: Design Choices & Architecture

## 1. Overview
This document details the data structure and architectural decisions made for the Omni File System (OFS) Phase 1 implementation. The system is designed to operate as a single-file virtual file system (`.omni`) managed by a C++ backend server with a Python-based client interface.

---

## 2. Data Structure Decisions

### 2.1 User Management: AVL Tree
**Choice:** Users are loaded into an in-memory **AVL Tree** (Self-Balancing Binary Search Tree).

* **Reasoning:**
    * **Fast Lookup ($O(\log n)$):** The `user_login` operation is frequency-critical. An AVL tree guarantees logarithmic time complexity for search operations, ensuring that even in the worst-case scenario (unlike a skewed BST), login checks remain fast.
    * **Sorted Order:** The `user_list` (admin) operation requires displaying users in alphabetical order. An in-order traversal of the AVL tree naturally yields sorted data in $O(n)$ time without requiring a separate sorting step (which would be $O(n \log n)$).
    * **Memory Efficiency:** Unlike a Hash Table, which might require pre-allocating a large array to minimize collisions, the AVL tree allocates memory dynamically per user node.

### 2.2 File System Hierarchy: N-ary Tree
**Choice:** The directory structure is represented by an **N-ary Tree** where each `FSNode` contains a dynamic list (`std::vector`) of children.

* **Reasoning:**
    * **Natural Representation:** File systems are inherently hierarchical. An N-ary tree perfectly models the "folder containing $N$ items" relationship.
    * **Traversal:** Path resolution (e.g., `/home/docs/file.txt`) is implemented by traversing the tree from the Root node down to the leaves.
    * **Flexibility:** Unlike a fixed-degree tree (like a Binary Tree), an N-ary tree allows a directory to hold an unlimited number of files, limited only by disk block size.

### 2.3 Free Space Management: Bitmap
**Choice:** Free blocks are tracked using a **Bitmap** (implemented as `std::vector<bool>`).

* **Reasoning:**
    * **Space Efficiency:** A bitmap uses only 1 bit per block. For a 100MB file system with 4KB blocks (25,600 blocks), the bitmap requires only ~3.2 KB of RAM. A Linked List implementation would require significantly more memory (4-8 bytes per free node).
    * **Contiguous Allocation:** The `file_create` operation often requires allocating $N$ consecutive blocks to minimize fragmentation and improve read speeds. Scanning a bitmap for $N$ consecutive zeros is a linear and cache-friendly operation.
    * **Fast State Checking:** Determining if a specific block is free is an $O(1)$ array access.

---

## 3. On-Disk Architecture (.omni File)

The `.omni` container file is structured as a flat binary file divided into fixed-size blocks (4096 bytes).

### 3.1 Layout Strategy
| Block Index | Content | Description |
| :--- | :--- | :--- |
| **0** | **OMNIHeader** | Contains FS metadata (version, total size, offsets). |
| **1** | **User Table** | Fixed array of `UserInfo` structs. Loaded into AVL Tree at boot. |
| **2** | **Root Directory** | Stores `FileEntry` structs for the root `/` directory. |
| **3...N** | **Data / Subdirs** | Used for file content or subdirectory listings. |

### 3.2 Persistence Strategy
* **Metadata Persistence:** When a file is created, its `FileEntry` metadata is written immediately to the parent directory's data block. We utilize the `reserved` field in the `FileEntry` struct to store the **Start Block Index**, ensuring we can locate the file's data after a reboot.
* **Data Persistence:** File content is written directly to the allocated data block(s) using `std::fstream`.
* **Recovery (`fs_init`):**
    1.  The system reads the **Header** to validate the magic number.
    2.  It reads **Block 1** to populate the **User AVL Tree**.
    3.  It reads **Block 2** (Root) to rebuild the first level of the **N-ary File Tree**.
    4.  (Future Phase 2) Recursively reads subdirectory blocks to rebuild the full tree.

---

## 4. Client-Server Architecture

* **Communication:** TCP Sockets.
* **Protocol:** JSON-based request/response format.
* **Concurrency:** A **FIFO Queue** handles incoming requests. A dedicated worker thread pops requests one by one, ensuring thread safety without complex locking mechanisms on the file system data structures.

## 5. Complexity Analysis

| Operation | Data Structure | Time Complexity |
| :--- | :--- | :--- |
| `user_login` | AVL Tree | $O(\log U)$ where $U$ is users. |
| `user_create` | AVL Tree | $O(\log U)$ (due to rebalancing). |
| `file_create` | Bitmap + N-ary Tree | $O(B)$ to scan bitmap + $O(L)$ to traverse path. |
| `dir_list` | N-ary Tree | $O(C)$ where $C$ is children count. |