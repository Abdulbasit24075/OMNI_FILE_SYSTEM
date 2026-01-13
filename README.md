Omni File System (OMNI File System)

A custom virtual file system implemented in C++, inspired by real operating-system file systems (File Explorer on WINDOWS).
The project demonstrates core OS concepts, data structures, and a client–server architecture, with a Python (PyQt) GUI for interaction.

Data Structures Used in Your File System (brief content in eq300)
🗂️ Omni File System

Omni File System is a small virtual file system that works like the file system on your computer (Windows / Linux), but it runs inside a single file.

It lets users:

log in
create folders
create files
write and read file content
manage disk space

**All through a client–server system.**
----------------------------------------------------


**What does this project actually do?**

Think of Omni File System like this:

There is one backend main file which is one big file called disk.omni(.omni binary file).
Inside that file, User can creates folders, files, and users — just like a real computer.and this backend file store all infor related to directories , authorities , user , permission and saved data in file mean which bits or bytes are allocated 

You can:

create /home/docs/notes.txt
delete files and folders
see file sizes
manage users (admin)

But everything lives inside one .omni file at backend , you have no work with it and it is binary so it will not shown to you , In your project, the binary file (.omni) contains structured metadata and reserved space for file data, stored in raw binary form (not human-readable text). 

**What EXACTLY is stored inside your .omni binary file?**

Your binary file contains these metadata structures:
OMNIHeader (Superblock) <------------------------------------------------------------------------------

📍 Location:

First 512 bytes of the .omni file

Always at block 0

Contains:

Filesystem identity (magic)

Filesystem version

Total filesystem size

Block size

User table offset

Max users

Config hash & timestamp

Reserved future fields

👉 This tells the system:

“What kind of filesystem is this and how should I read it?”

2️⃣ User Table (User Metadata)  <------------------------------------------------------------------------------
📍 Location:

Offset given by user_table_offset in OMNIHeader

Each entry is a UserInfo structure containing:

username

password hash

user role (ADMIN / NORMAL)

account creation time

last login time

active/deleted flag

👉 This tells the system:

“Who are the users and what are their roles?”



3️⃣ File & Directory Metadata (FileEntry) <------------------------------------------------------------------------------

📍 Location:

After user table (planned layout)

Each FileEntry contains:

file/folder name

type (file or directory)

size

permissions

owner

inode

parent inode

timestamps

👉 This tells the system:

“What files exist, where they belong, and how they behave.”


4️⃣ Directory Relationships (via inodes table : this table is disscuss later in eq245) <------------------------------------------------------------------------------


This is implicit metadata.

Stored using:

inode

parent_inode

Together they describe:

folder hierarchy

parent–child relationships

👉 This allows the filesystem to rebuild the directory tree after restart.



5️⃣ Block Allocation Information (Bitmap) — Design Level  <------------------------------------------------------------------------------


📍 Logical metadata

In your current phase:

Bitmap exists in memory

Designed to be stored later in the .omni file

It represents:

which disk blocks are free

which disk blocks are used

👉 This tells the system:

“Where can I store file data safely?”



6️⃣ Reserved Space (Future Metadata)   <------------------------------------------------------------------------------

📍 Inside OMNIHeader

uint8_t reserved[328];


This space is intentionally kept for:

journaling

snapshots

change logs

crash recovery

👉 This shows your filesystem is future-proof.

--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


**Operating systems use a directory table / inode table that stores each folder (or file) along with a reference to its parent directory.** <------------------------------------- **eq245**

<img width="805" height="761" alt="image" src="https://github.com/user-attachments/assets/6a7c77f2-50b9-4b1e-b81b-caad837b796c" />

🔁 How OS finds the full path

To find the path of Notes (ID = 5):

Notes → parent ID = 4

Documents → parent ID = 3

Ali → parent ID = 2

Users → parent ID = 1

Root reached

Then reverse it:
C:\Users\Ali\Documents\Notes


In my project, this information is stored using:
struct FileEntry {
    uint32_t inode;
    uint32_t parent_inode;
};
So my filesystem already has this table implicitly.


How this is represented in memory (your N-ary tree)

In RAM, your project adds:

FSNode* parent;
vector<FSNode*> children;
This makes navigation fast.

But on disk, only inode + parent_inode are stored.

---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

**Data Structures Used in Your File System** <---------------------------------------------------------------------------------------------eq300

Your project uses three main data structures, each chosen for a specific real-world OS problem.

🔑 Big Picture (remember this first)
<img width="907" height="273" alt="image" src="https://github.com/user-attachments/assets/db0b6f30-7530-486f-8817-4e1b8abcfedc" />

<img width="508" height="767" alt="image" src="https://github.com/user-attachments/assets/9d0b5d16-e0a1-48e5-97f3-90050327fdb7" />


<img width="567" height="781" alt="image" src="https://github.com/user-attachments/assets/b3f64287-2bac-4e97-9cac-5bd0b4b4ff86" />

But remember below data structure of Bitmap is not built-in (remember it ) 




<img width="604" height="692" alt="image" src="https://github.com/user-attachments/assets/379fae98-0963-47cd-9833-b681c1e06df5" />

and always remember that these all details of metadata are stored in .omni binary fle and .omni binary file is not stored on disk but it is always in memory in my project 
