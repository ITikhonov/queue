#include <signal.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <arpa/inet.h>
#include <linux/limits.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

int indir=-1;
int tmpdir=-1;

struct {
	uint32_t magic;
	uint16_t nlen;
	uint64_t flen;
	char name[PATH_MAX];
} __attribute__ ((packed)) hdr;

int recvhdr(int s) {
	char *buf=(char*)&hdr;
	int r=0;
	for(;r<14;) {
		int rr=read(s,buf+r,14-r);
		if(rr<1) return 0;
		r+=rr;
	}

	if(*(uint32_t*)buf != 0x45454551) return 0;

	r=0;
	for(;r<hdr.nlen;) {
		int rr=read(s,hdr.name+r,hdr.nlen-r);
		if(rr<1) return 0;
		r+=rr;
	}
	
	return 1;
}

int recvfile(int s) {
	fprintf(stderr,"receiving %s\n",hdr.name);
	int f=openat(tmpdir,hdr.name,O_WRONLY|O_CREAT,0640);
	if(f==-1) { return 0; }

	char buf[65535];
	int r=0;
	for(;r<hdr.flen;) {
		int tr=hdr.flen-r;
		if(tr>sizeof(buf)) tr=sizeof(buf);

		int rr=read(s,buf,tr);
		if(rr<1) goto err;
		write(f,buf,rr);
		r+=rr;
	}
	close(f);

	write(s,"O",1);
	shutdown(s,SHUT_WR);

	renameat(tmpdir,hdr.name,indir,hdr.name);
	fprintf(stderr,"done %s\n",hdr.name);
	write(1,&hdr.nlen,2);
	write(1,hdr.name,hdr.nlen);
	return 1;

err:
	close(f);
	unlinkat(tmpdir,hdr.name,0);
	printf("error %s\n",hdr.name);
	return 0;
}

int s=-1;
int goon=1;
void terminate(int sig) { goon=0; close(s); }

int main(int argc,char *argv[]) {
	struct sigaction act={.sa_handler=SIG_IGN,.sa_flags=SA_RESTART};
	sigemptyset(&act.sa_mask);
	sigaction(SIGPIPE,&act,0);

	act.sa_handler=terminate;
	sigaction(SIGTERM,&act,0);

	struct sockaddr_in ba={.sin_family=AF_INET};
	ba.sin_addr.s_addr=inet_addr(argv[2]);
	ba.sin_port=htons(atoi(argv[3]));
        s=socket(AF_INET,SOCK_STREAM,IPPROTO_IP);
	bind(s,(struct sockaddr*)&ba,sizeof(ba));
	listen(s,1024);

	indir=open("in",O_RDONLY);
	if(indir==-1) exit(1);
	tmpdir=open("tmp",O_RDONLY);
	if(tmpdir==-1) exit(1);

	for(;goon;) {
		int a=accept(s,0,0);
		if(a!=-1 && recvhdr(a)) {
			recvfile(a);
		}
		close(a);
	}
	close(s);
	return 0;
}

