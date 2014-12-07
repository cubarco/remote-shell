#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#define __USE_BSD
#include <termios.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#define DEBUG
#include "pw.c"

#define PORT    55555
#define MAXBUF  1024
#define MAX(a,b) (((a)>(b))?(a):(b))

void pty_func(int);

int main(int argc, char **argv)
{
        int s_sockfd, c_sockfd;
        struct sockaddr_in s_addr, c_addr;
        socklen_t len = sizeof(struct sockaddr);
        
        /**
         * SIGINT signal Call-back
         */
        void before_kill()
        {
                putchar('\n');
                shutdown(s_sockfd, 2);
                shutdown(c_sockfd, 2);
                close(s_sockfd);
                close(c_sockfd);
                exit(EXIT_SUCCESS);
        }
        signal(SIGINT, before_kill);
        
        if((s_sockfd=socket(AF_INET, SOCK_STREAM, 0)) == -1){
                eprintf("Can not open socket.\n");
                exit(EXIT_FAILURE);
        }else{
                dprintf("Socket opened.\n");
        }
        
        s_addr.sin_family = AF_INET;
        s_addr.sin_port = htons(PORT);
        s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        
        // set SO_REUSEADDR option to avoid TIME_WAIT state
        int reuse_addr=1;
        setsockopt(s_sockfd, SOL_SOCKET, SO_REUSEADDR, 
                        (const char*)&reuse_addr, sizeof(reuse_addr));

        if(bind(s_sockfd, (struct sockaddr*)&s_addr, len) == -1){
                eprintf("Can not bind.\n");
                exit(EXIT_FAILURE);
        }else{
                dprintf("Socket binded.\n");
        }

        if(listen(s_sockfd, 5) == -1){
                eprintf("Socket listen error.\n");
                exit(EXIT_FAILURE);
        }else{
                dprintf("Socket listened.\n");
        }
        
        iprintf("Waiting for connection.\n");
        
        pid_t cli_pid_tmp;
        int i;
        while(1){
                // ACCEPT
                c_sockfd = accept(s_sockfd, (struct sockaddr*)&c_addr, &len);
                if(-1 == c_sockfd){
                        eprintf("Accept error or server shutdown.\n");
                        exit(EXIT_FAILURE);
                }
                // Double fork to get pty and shell
                if((cli_pid_tmp=fork())==-1){
                        eprintf("cli fork error\n");
                        break;
                }else if(cli_pid_tmp==0){
                        close(s_sockfd);
                        iprintf("Got connection from %s:%d\n", inet_ntoa(c_addr.sin_addr), ntohs(c_addr.sin_port));
                        pid_t cli_pid;
                        if((cli_pid=fork()) == -1){
                                eprintf("Double fork error\n");
                        }else if(cli_pid == 0){
                                pty_func(c_sockfd);
                        }
                        exit(EXIT_SUCCESS);
                }else{
                        wait(NULL);
                        dprintf("Double fork trick done\n");
                }
        }

        return EXIT_SUCCESS;
}

void run_server(int fdm, int c_sockfd, char *buf, socklen_t len, pid_t pid_cli)
{
	int maxfdp1;
	fd_set rset;

	FD_ZERO(&rset);
	for (;;) {
		FD_SET(fdm, &rset);
		FD_SET(c_sockfd, &rset);
		maxfdp1 = MAX(fdm, c_sockfd) + 1;
		select(maxfdp1, &rset, NULL, NULL, NULL);

		memset(buf, 0, MAXBUF);
		if (FD_ISSET(fdm, &rset)) {
			len = read(fdm, buf, MAXBUF);
			if (0 == len)
				break ;
			send(c_sockfd, buf, strlen(buf), 0);
		}

		if (FD_ISSET(c_sockfd, &rset)) {
			len = recv(c_sockfd, buf, MAXBUF, 0);
	                if(len > 0){
                                iprintf("PID: %d, RUNNING: %s", pid_cli, buf);
                                write(fdm, buf, strlen(buf));
                        }else if(len==0){
                                wprintf("Client quit.\n");
                                kill(pid_cli, SIGINT);
                                break ;
                        }else{
                                eprintf("recv error.\n");
                                kill(pid_cli, SIGINT);
                                break ;
                        }
 		}
	}

}

/**
 * Give a pseudo-terminal to bash.
 */
void pty_func(int c_sockfd)
{
        int fdm, fds, rc;
        char buf[MAXBUF+1];
        int len;
        
        /**
         * SIGCHLD signal Call-back
         */
        void shell_shutdown()
        {
                wprintf("Shell shutdown.\n");
                shutdown(c_sockfd, 2);
                exit(EXIT_SUCCESS);
        }        

        fdm = posix_openpt(O_RDWR); 
        if (fdm < 0){
                eprintf("Error %d on posix_openpt()\n", errno); 
                exit(EXIT_FAILURE); 
        } 
        rc = grantpt(fdm); 
        if (rc != 0){
                eprintf("Error %d on grantpt()\n", errno); 
                exit(EXIT_FAILURE); 
        } 
        rc = unlockpt(fdm); 
        if (rc != 0){
                eprintf("Error %d on unlockpt()\n", errno); 
                exit(EXIT_FAILURE); 
        } 
        // Open the slave PTY
        fds = open(ptsname(fdm), O_RDWR);

        pid_t pid_cli;
        struct termios options;
        if(-1 == (pid_cli = fork())){
                eprintf("Fork error\n");
                exit(EXIT_FAILURE);
        }else if(0 == pid_cli){
                close(fdm);
                dup2(fds, fileno(stdin));
                dup2(fds, fileno(stdout));
                dup2(fds, fileno(stderr));
                close(fds);
                setsid();
                ioctl(STDIN_FILENO, TIOCSCTTY, 1);
                tcgetattr(STDIN_FILENO, &options);
                options.c_lflag &= ~ECHO;       // do not send back user input
                options.c_lflag |= (ICANON | ECHOE);
                options.c_iflag |= (IGNCR);     // set iflag to ignore CR from input
                tcsetattr(STDIN_FILENO, TCSANOW, &options);
                execl("/bin/bash", "bash", NULL);
        }else{
                signal(SIGCHLD, shell_shutdown);
                close(fds);
		run_server(fdm, c_sockfd, buf, len, pid_cli);
        }
        dprintf("Going to close connetion.\n");
        shutdown(c_sockfd, 2);
}
