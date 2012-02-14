#define _GNU_SOURCE

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

#include <dirent.h>
#include <signal.h>

struct sockaddr_in da={.sin_family=AF_INET};

int s=-1;
void abort_transfer(int sig) {
	close(s);
}

int transfer_file(char *name) {
	printf("transfer %s\n",name);

	s=socket(AF_INET,SOCK_STREAM,IPPROTO_IP);
	int f=open(name,O_RDONLY);

	alarm(10);
	if(connect(s,(struct sockaddr*)&da,sizeof(da))==-1) goto err;
	alarm(0);

	uint64_t len=lseek(f,0,SEEK_END);
	off_t start=0;
	uint16_t ln=strlen(name);

	write(s,"QEEE",4);
	write(s,&ln,2);
	write(s,&len,8);
	write(s,name,ln);

	for(;start<len;) {
		alarm(10);
		if(sendfile(s,f,&start,10240)==-1) goto err;
	}
	alarm(0);

	char o=0;
	read(s,&o,1);
	if(o!='O') goto err;

	close(s);
	close(f);
	unlink(name);
	printf("done %s\n",name);
	return 1;
err:
	alarm(0);
	close(s);
	close(f);
	return 0;
}

int filter(const struct dirent *e) {
	return e->d_name[0]!='.';
}

void send_stale() {
	struct dirent **list;
	int n=scandir(".",&list,filter,versionsort);
	int i;
	for(i=0;i<n;i++) {
		while(!transfer_file(list[i]->d_name)) sleep(1);
		free(list[i]);
	}
	free(list);
}

int main(int argc,char *argv[]) {
	da.sin_addr.s_addr=inet_addr(argv[2]);
	da.sin_port=htons(atoi(argv[3]));

	if(chdir(argv[1])) exit(1);

	struct sigaction act={.sa_handler=abort_transfer,.sa_flags=0};
	sigemptyset(&act.sa_mask);
	sigaction(SIGALRM,&act,0);

	int fs=inotify_init();
	inotify_add_watch(fs,".",IN_MOVED_TO);

	send_stale();

	for(;;) {
		char buf[sizeof(struct inotify_event)+PATH_MAX+1];
		struct inotify_event *e=(struct inotify_event*)buf;
		if(read(fs,e,sizeof(buf))<1) { exit(1); }
		while(!transfer_file(e->name)) sleep(1);
	}
	close(fs);

}

