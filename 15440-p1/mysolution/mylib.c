#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <err.h>


#define OP_OPEN 0
#define OP_CLOSE 1
#define OP_READ 2
#define OP_WRITE 3
#define OP_LSEEK 4
#define OP_STAT 5
#define OP_UNLINK 6
#define OP_GETDIR 7
#define OP_GETDIRTREE 8
#define OP_FREETREE 9

// The following line declares a function pointer with the same prototype as the open function.  
int (*orig_open)(const char *pathname, int flags, ...);  // mode_t mode is needed when flags includes O_CREAT
int (*orig_close)(int fd);
ssize_t (*orig_read)(int fd, void *buf, size_t count);
ssize_t (*orig_write)(int fd, const void *buf, size_t count);
off_t (*orig_lseek)(int fd, off_t offset, int whence);
int (*orig_stat)(int ver, const char * path, struct stat * stat_buf);
//int __xstat(int ver, const char * path, struct stat * stat_buf);
int (*orig_unlink)(const char *pathname);
ssize_t (*orig_getdirentries)(int fd, char *buf, size_t nbytes , off_t *basep);
struct dirtreenode* (*orig_getdirtree)( const char *path);
void (*orig_freedirtree)( struct dirtreenode* dt);

void send_msg(char * buf);
void send_msg_by_type(int type);
int network_init();

int port = 15440;

// This is our replacement for the open function from libc.
int open(const char *pathname, int flags, ...) {
	mode_t m=0;
	if (flags & O_CREAT) {
		va_list a;
		va_start(a, flags);
		m = va_arg(a, mode_t);
		va_end(a); 
	}
	// we just print a message, then call through to the original open function (from libc)
	fprintf(stderr, "mylib: open called for path %s\n", pathname);
	send_msg_by_type(OP_OPEN);
	return orig_open(pathname, flags, m);
}

int close(int fd){
	fprintf(stderr, "mylib: close called for path %d\n", fd);
	send_msg_by_type(OP_CLOSE);
	return orig_close(fd);
}

ssize_t read(int fd, void *buf, size_t count){

	fprintf(stderr, "mylib: read called for path %d size %zd \n", fd,count);
	send_msg_by_type(OP_READ);
	return orig_read(fd,buf,count);
}

ssize_t write(int fd, const void *buf, size_t count){
	fprintf(stderr, "mylib: write called for path %d size %zd \n", fd,count);
	send_msg_by_type(OP_WRITE);
	return orig_write(fd,buf,count);
}

off_t lseek(int fd, off_t offset, int whence){
	fprintf(stderr, "mylib: lseek called for path %d \n", fd);
	send_msg_by_type(OP_LSEEK);
	return orig_lseek(fd,offset,whence);
}

int __xstat(int ver, const char * path, struct stat * stat_buf){
	fprintf(stderr, "mylib: stat called for path %s \n", path);
	send_msg_by_type(OP_STAT);
	return orig_stat(ver,path,stat_buf);
}

int unlink(const char *pathname){
	fprintf(stderr, "mylib: unlink called for path %s \n", pathname);
	send_msg_by_type(OP_UNLINK);
	return orig_unlink(pathname);
}

ssize_t getdirentries(int fd, char *buf, size_t nbytes , off_t *basep){
	fprintf(stderr, "mylib: getdirentries called for path %d \n", fd);
	send_msg_by_type(OP_GETDIR);
	return orig_getdirentries(fd,buf,nbytes,basep);
}

struct dirtreenode* getdirtree( const char *path){
	fprintf(stderr, "mylib: getdirtree called for path %s \n", path);
	send_msg_by_type(OP_GETDIRTREE);
	return orig_getdirtree(path);
}

void freedirtree( struct dirtreenode* dt){
	fprintf(stderr, "mylib: getdirtree called\n");
	send_msg_by_type(OP_GETDIRTREE);
	return orig_freedirtree(dt);
}



// This function is automatically called when program is started
void _init(void) {
	// set function pointer orig_open to point to the original open function
	orig_open = dlsym(RTLD_NEXT, "open");
	orig_close = dlsym(RTLD_NEXT,"close");
	orig_read = dlsym(RTLD_NEXT,"read");
	orig_write = dlsym(RTLD_NEXT,"write");
	orig_lseek = dlsym(RTLD_NEXT,"lseek");
	orig_stat = dlsym(RTLD_NEXT,"__xstat");
	orig_unlink = dlsym(RTLD_NEXT,"unlink");
	orig_getdirentries = dlsym(RTLD_NEXT,"getdirentries");
	orig_getdirtree = dlsym(RTLD_NEXT,"getdirtree");
	orig_freedirtree = dlsym(RTLD_NEXT, "freedirtree");
	fprintf(stderr, "Init mylib\n");
}

#define MAXMSGLEN 100

int network_init(){
	char *serverip;
	char *serverport;
	unsigned short port;
	int sockfd, rv;
	struct sockaddr_in srv;

	// Get environment variable indicating the ip address of the server
	serverip = getenv("server15440");
	if (serverip) printf("Got environment variable server15440: %s\n", serverip);
	else {
		printf("Environment variable server15440 not found.  Using 127.0.0.1\n");
		serverip = "127.0.0.1";
	}
	
	// Get environment variable indicating the port of the server
	serverport = getenv("serverport15440");
	if (serverport) fprintf(stderr, "Got environment variable serverport15440: %s\n", serverport);
	else {
		fprintf(stderr, "Environment variable serverport15440 not found.  Using 15440\n");
		serverport = "15440";
	}
	port = (unsigned short)atoi(serverport);
	
	// Create socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);	// TCP/IP socket
	if (sockfd<0) err(1, 0);			// in case of error
	
	// setup address structure to point to server
	memset(&srv, 0, sizeof(srv));			// clear it first
	srv.sin_family = AF_INET;			// IP family
	srv.sin_addr.s_addr = inet_addr(serverip);	// IP address of server
	srv.sin_port = htons(port);			// server port

	// actually connect to the server
	rv = connect(sockfd, (struct sockaddr*)&srv, sizeof(struct sockaddr));
	if (rv<0) err(1,0);	
	return sockfd;
}

void send_msg_by_type(int type){

	switch(type){
		case OP_OPEN:
			send_msg("open");
			break;
		case OP_CLOSE:
			send_msg("close");
			break;
		case OP_READ:
			send_msg("read");
			break;
		case OP_WRITE:
			send_msg("write");
			break;
		case OP_LSEEK:
			send_msg("lseek");
			break;
		case OP_STAT:
			send_msg("stat");
			break;
		case OP_UNLINK:
			send_msg("unlink");
			break;
		case OP_GETDIR:
			send_msg("getdirentries");
			break;
		case OP_GETDIRTREE:
			send_msg("getdirtree");
			break;
		case OP_FREETREE:
			send_msg("freedirtree");
			break;
	}
}

void send_msg(char * msg){
	int sockfd = network_init();
	send(sockfd, msg, strlen(msg)+1, 0);   // send 1024 bytes from bu
	// get message back
	char buf[MAXMSGLEN+1];
	int rv = recv(sockfd, buf, MAXMSGLEN, 0);	// get message
	if (rv<0) err(1,0);			// in case something went wrong
	buf[rv]=0;				// null terminate string to print
	fprintf(stderr, "client got messge: %s\n", buf);
	orig_close(sockfd); // client is done
}


