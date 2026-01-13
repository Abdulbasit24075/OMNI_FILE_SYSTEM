# OFS (Omni File System) - Phase 1

**Student ID:** [Your Student ID]
**Course:** Data Structures & Algorithms

## Project Overview
This is a persistent, single-file virtual file system implemented in C++ (-Backend) with a Python (PyQt6) User Interface. It demonstrates the use of custom data structures to manage users, files, and disk space within a single `.omni` binary container.

## 📂 Project Structure
The submission follows the required Phase 1 structure:

```text
/
├── source/
│   ├── include/           # Header files (.h, .hpp)
│   ├── server/
│   │   ├── core/          # Server logic and persistence
│   │   ├── data_structures/ # AVL Tree, N-ary Tree, Bitmap implementations
│   │   └── main.cpp       # Entry point
│   └── ui/                # Python Client (client.py)
├── documentation/         # Design choices and reports
├── default.uconf          # Server configuration
├── omni_fs.omni           # The binary file system (Generated automatically)
└── README.md              # This file
```

## 🛠️ Prerequisites

- C++ Compiler: g++ (supporting C++11 or later)

-    Python: Python 3.x

-    Python Libraries: PyQt6 (pip install PyQt6)

-    OS: Linux / Ubuntu (Tested on Ubuntu 22.04)

## 🚀 How to Compile & Run

#### Step 1: Compile the Backend Server

Open a terminal in the root directory of the project and run:

```bash
g++ source/server/main.cpp source/server/core/ofs_server.cpp source/server/data_structures/ofs_structures.cpp -o ofs_server -I source/include -pthread
```
#### Step 2: Run the Server

Start the server. It will look for default.uconf and omni_fs.omni. If the .omni file does not exist, it will be created and formatted automatically.

```bash
./ofs_server
```
Expected Output:
```text
[MAIN] Initializing File System from: omni_fs.omni
[CONFIG] Loaded configuration. Port: 8081
[INFO] Loading existing File System...
[SERVER] Listening on port 8081...
```
#### Step 3: Run the Client UI

Open a new terminal window and run the Python interface:
```bash
python3 source/ui/client.py
```
(Note: If your client.py is in the root, use python3 client.py)

## 🧪 Features & Usage

#### 1. Login (Authentication)

  -  **Operation**: user_login
  -  **Default Admin**: username: admin, password: admin123
  -  **Data Structure**: AVL Tree (O(logn) lookup)

#### 2. File Operations

-    **List Files**: Use dir_list on path /.

-   **Create File**: Use file_create to write data to the virtual disk.

-    **Read File**: Use file_read to retrieve data.

-    **Create Folder**: Use dir_create to make subdirectories.

-    **Data Structure**: N-ary Tree for directory hierarchy; Bitmap for free block allocation.

#### 3. Persistence

The server saves all file content and the Root Directory structure to omni_fs.omni. You can stop the server (Ctrl+C) and restart it; your files and folders will be recovered automatically.

## ⚠️ Troubleshooting

-    **"Bind failed"**: The port 8081 might be in use. Wait a minute or change the port in default.uconf.

-    **"File not found" (Client)**: Ensure the path starts with / (e.g., /readme.txt).

-    **"Permission denied"**: Ensure you have read/write permissions in the project folder.
