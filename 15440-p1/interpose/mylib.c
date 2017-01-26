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

// The following line declares a function pointer with the same prototype as the open function.  
int (*orig_open)(const char *pathname, int flags, ...);  // mode_t mode is needed when flags includes O_CREAT
int (*orig_close)(int fd);
ssize_t (*orig_read)(int fd, void *buf, size_t count);
ssize_t (*orig_write)(int fd, void *buf, size_t count);
off_t (*orig_lseek)(int fd, off_t offset, int whence);
int (*orig_stat)(const char *pathname, struct stat *buf);
//int __xstat(int ver, const char * path, struct stat * stat_buf);
int (*orig_unlink)(const char *pathname);
ssize_t (*orig_getdirentries)(int fd, char *buf, size_t nbytes , off_t *basep);


void send_msg(char * buf);
void send_msg_by_type(int type);
int network_init();

int port = 15440;
int socketfd = 0;

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
	//fprintf(stderr, "mylib: open called for path %s\n", pathname);
	send_msg_by_type(OP_OPEN);
	return orig_open(pathname, flags, m);
}

int close(int fd){
	//fprintf(stderr, "mylib: close called for path %d\n", fd);
	send_msg_by_type(OP_CLOSE);
	return orig_close(fd);
}

ssize_t read(int fd, void *buf, size_t count){

	//fprintf(stderr, "mylib: read called for path %d size %zd \n", fd,count);
	send_msg_by_type(OP_READ);
	return orig_read(fd,buf,count);
}

ssize_t write(int fd, const void *buf, size_t count){
	//fprintf(stderr, "mylib: write called for path %d size %zd \n", fd,count);
	send_msg_by_type(OP_WRITE);
	return orig_write(fd,buf,count);
}

off_t lseek(int fd, off_t offset, int whence){
	//fprintf(stderr, "mylib: lseek called for path %d \n", fd);
	send_msg_by_type(OP_LSEEK);
	return orig_lseek(fd,offset,whence);
}

int stat(const char *pathname, struct stat *buf){
	//fprintf(stderr, "mylib: stat called for path %s \n", pathname);
	send_msg_by_type(OP_STAT);
	return orig_stat(pathname,buf);
}

int unlink(const char *pathname){
	//fprintf(stderr, "mylib: unlink called for path %s \n", pathname);
	send_msg_by_type(OP_UNLINK);
	return orig_unlink(pathname);
}

ssize_t getdirentries(int fd, char *buf, size_t nbytes , off_t *basep){
	//fprintf(stderr, "mylib: getdirentries called for path %d \n", fd);
	send_msg_by_type(OP_GETDIR);
	return orig_getdirentries(fd,buf,nbytes,basep);
}


// This function is automatically called when program is started
void _init(void) {
	// set function pointer orig_open to point to the original open function
	orig_open = dlsym(RTLD_NEXT, "open");
	orig_close = dlsym(RTLD_NEXT,"close");
	orig_read = dlsym(RTLD_NEXT,"read");
	orig_write = dlsym(RTLD_NEXT,"write");
	orig_lseek = dlsym(RTLD_NEXT,"lseek");
	orig_stat = dlsym(RTLD_NEXT,"stat");
	orig_unlink = dlsym(RTLD_NEXT,"unlink");
	orig_getdirentries = dlsym(RTLD_NEXT,"getdirentries");
	socketfd = network_init();
	//fprintf(stderr, "Init mylib\n");
}

int network_init(){
	int sockfd = socket(AF_INET, SOCK_STREAM, 0); // TCP/IP socket
	struct sockaddr_in srv; // address structure
	memset(&srv, 0, sizeof(srv)); // zero it out
	srv.sin_family = AF_INET;
	//srv.sin_addr.s_addr = inet_addr("127.0.0.1"); 
	srv.sin_addr.s_addr = htonl(INADDR_ANY);	// don't care IP address
	srv.sin_port = htons(port); 
	int rv = connect(sockfd, (struct sockaddr *) &srv, sizeof(struct sockaddr)); //connecttoserver
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
	}
}

void send_msg(char * buf){
	//int sockfd = network_init();
	int rv = send(socketfd, buf, strlen(buf)+1, 0);   // send 1024 bytes from bu
	//fprintf(stderr, "return %d\n",rv);
	//close(sockfd); // client is done
}


