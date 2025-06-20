# **Network File System (NFS)**

## **Ramantan Konomi 201800281**

This project implements a multi-threaded **Network File System (NFS)** in C. The system is designed to synchronize directories between source and target machines/ports using network sockets and threads.

---

## **Components**

- **NFS_MANAGER**  
  Coordinates the synchronization process between source and target directories. Handles configuration, manages worker threads, receives commands, and logs activity.

- **NFS_CLIENT**  
  Acts as a server, listening for commands (LIST, PULL, PUSH) and serving files to the manager or worker threads.

- **NFS_CONSOLE**  
  Provides a user interface for submitting requests (add, cancel, shutdown) to the manager.

---

## **How it Works**

1. **Manager Setup**
    - Parses a config file containing source/target directory pairs.
    - Creates a thread pool (number of workers chosen by user).
    - Listens on a user-specified port for commands from the console.

2. **Synchronization**
    - Manager connects to a running client and sends `LIST <source_dir>`.
    - Receives a list of files from the client, stores information in a buffer.
    - Receives commands from the console:  
      - `add <source> <target>`: Registers all files from the source dir for synchronization to the target.
      - `cancel <source_dir>`: Cancels the rest of the sync for the given source dir.
      - `shutdown`: Closes the manager and the console.
    - Worker threads:
      - Fetch jobs from the queue.
      - For each file, connect to the source client and send `PULL <source_dir>/<file>`.
      - Read file content and size.
      - Connect to the target client and send `PUSH <target_dir>/<file>`, then transfer the file in chunks (size sent as a string, followed by data).

3. **Clients**
    - Each NFS_CLIENT listens on a port for requests.
    - Supported commands:
      - `LIST <directory>`: Returns the list of files in the directory.
      - `PULL <file_path>`: Sends file size and data if the file exists.
      - `PUSH <file_path>`: Receives file data in chunks and writes it to the target directory.

---

## **Compilation and Execution**

**Compile all components** (from the parent directory):
```sh
make
```
To clean up executables and objects:
```sh
make clean
```

### **Running NFS_MANAGER**
Open a terminal:
```sh
./nfs_manager -l manager.logfile -c config.txt -n <worker_count> -p <port_number> -b <buffersize>
```
- `manager.logfile`: File to log manager activity.
- `config.txt`: Configuration file with source/target directory pairs.
- `worker_count`: Number of worker threads.
- `port_number`: Port for manager-console communication.
- `buffersize`: Maximum slots in the job queue.

### **Running NFS_CONSOLE**
Open another terminal:
```sh
./nfs_console -l console.logfile -h <host_ip> -p <host_port>
```
- `host_ip`: IP where the manager is running.
- `host_port`: Port manager listens for console commands.

### **Running NFS_CLIENT(s)**
As many as needed, based on config pairs:
```sh
./nfs_client -p <port_number>
```
- `port_number`: Port to serve LIST, PULL, PUSH commands from the manager/workers.

---

## **Implementation Details**

- **Thread-Safe Queue**:  
  Used for managing jobs between the manager and worker threads (see `queue.c`/`queue.h`). Employs mutexes and condition variables for synchronization and safe access.

- **Manager**:  
  - Parses config, stores sync pairs.
  - Sends LIST to clients, manages job queue.
  - Spawns and manages worker pool.
  - Handles `add`, `cancel`, `shutdown` commands from the console.

- **Worker Threads**:  
  - Fetch jobs from the queue.
  - Handle file transfers using PULL/PUSH commands and chunked data transfer.

- **Client**:  
  - Server that listens for LIST, PULL, PUSH commands.
  - Handles multiple connections with threads.
  - LIST returns file names in a directory.
  - PULL sends file size and data.
  - PUSH receives and writes file data in chunks.

---

## **Notes**

- All network communication is handled using sockets.
- Chunked transfers use a string to indicate chunk size, followed by the data.
- The system supports concurrent file transfers thanks to the worker pool.
- Logging is implemented for both manager and console.

---

For further details, refer to code comments and each component's source file.