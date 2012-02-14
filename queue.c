#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/inotify.h>
#include <sys/sendfile.h>
#include <arpa/inet.h>
#include <linux/limits.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

struct sockaddr_in da={.sin_family=AF_INET};

void transfer_file(char *name) {
	int s=socket(AF_INET,SOCK_STREAM,IPPROTO_IP);
	int f=open(name,O_RDONLY);

	connect(s,(struct sockaddr*)&da,sizeof(da));

	uint64_t len=lseek(f,0,SEEK_END);
	off_t start=0;
	uint16_t ln=strlen(name);

	write(s,&ln,2);
	write(s,name,ln);
	write(s,&len,8);

	sendfile(s,f,&start,len);

	close(s);
	close(f);
	unlink(name);
}

int main(int argc,char *argv[]) {
	da.sin_addr.s_addr=inet_addr(argv[2]);
	da.sin_port=htons(atoi(argv[3]));

	if(chdir(argv[1])) exit(1);

	int fs=inotify_init();
	inotify_add_watch(fs,".",IN_MOVED_TO);

	for(;;) {
		char buf[sizeof(struct inotify_event)+PATH_MAX+1];
		struct inotify_event *e=(struct inotify_event*)buf;
		read(fs,e,sizeof(buf));
		transfer_file(e->name);
	}
	close(fs);

}

