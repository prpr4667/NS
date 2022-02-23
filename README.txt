I have implemented a reliable UDP based file transfer between a client and a server.
I have implemented:
1. Get: Through get [file_name] - we get the files present in the current directory where the program is run (./), and puts
it in the root directory. Takes relative file path and filename and returns an error if file is not found.
2. Put: Through put [file_name] - we get the files present in the current directory (./). Also accepts relative paths and filename, and returns an error if not found.
3. ls: Lists all the files present in the current directory (along with current and parent (. and ..)).
4. Delete: deletes file specified by delete[file_name] if found, else throws and error.
5. exit: exits client gracefully
6. echoes back unknown commands to the stdout

For file transfer, packet size of 10240 bytes is set to be a packet size, and any file larger than that is sent as multiple files. The server receives it one by one,
waiting for an ACK for each file received/sent by the client with a timeout resending the packet if no ACK is received.
Duplicate get/puts of files are handled by appending current mmddhhmmss timestamp to the filename ensuring uniqueness to a degree.

Running:
Run makefile as "make" from the parent directory of server and client. It compiles both the server and client source files and creates the
executables in the respective directories.
