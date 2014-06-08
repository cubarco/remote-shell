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

#define MAXBUF 1024

void bash_server_func(int);
void pty_func(int, int);

int main(int argc, char **argv)
{
        int s_sockfd, c_sockfd;
        struct sockaddr_in s_addr, c_addr;
        socklen_t len = sizeof(struct sockaddr);
        
        /**
         *
         * SIGINT signal Call-back
         */
        void before_kill()
        {
                printf("\nServer going to shutdown\n");
                shutdown(s_sockfd, 2);
                close(s_sockfd);
                close(c_sockfd);
                exit(EXIT_SUCCESS);
        }
        signal(SIGINT, before_kill);

        if((s_sockfd=socket(AF_INET, SOCK_STREAM, 0)) == -1){
                printf("Can not open socket.\n");
                exit(EXIT_FAILURE);
        }else{
                printf("Socket opened.\n");
        }
        
        s_addr.sin_family = AF_INET;
        s_addr.sin_port = htons(55555);
        s_addr.sin_addr.s_addr = INADDR_ANY;
        
        if(bind(s_sockfd, (struct sockaddr*)&s_addr, len) == -1){
                printf("Can not bind.\n");
                exit(EXIT_SUCCESS);
        }else{
                printf("Socket binded.\n");
        }

        if(listen(s_sockfd, 5) == -1){
                perror("Socket listen error.\n");
                exit(EXIT_FAILURE);
        }else{
                printf("Socket listened.\n");
        }
        
        printf("Waiting for connection.\n");
        
        pid_t cli_pid_tmp;
        int i;
        while(1){
                // ACCEPT
                if((c_sockfd = accept(s_sockfd, (struct sockaddr*)&c_addr, &len)) == -1){
                        printf("Accept error or server shutdown.\n");
                        exit(EXIT_FAILURE);
                }
                // Double fork to get pty and shell
                if((cli_pid_tmp=fork())==-1){
                        perror("cli fork error\n");
                        break;
                }else if(cli_pid_tmp==0){
                        pid_t cli_pid;
                        if((cli_pid=fork()) == -1){
                                perror("Double fork error\n");
                        }else if(cli_pid == 0){
                                // TODO Run server side 
                                // bash_server_func(c_sockfd);
                                pty_func(c_sockfd, s_sockfd);
                        }
                        exit(EXIT_SUCCESS);
                }else{
                        wait(NULL);
                        printf("Double fork trick done\n");
                }
        }


        return EXIT_SUCCESS;
}

/**
 *
 * Give a pseudo-terminal to bash.
 */
void pty_func(int c_sockfd, int s_sockfd)
{
        int fdm, fds, rc;
        char buf[MAXBUF+1];
        int len;
        
        /**
         *
         * SIGCHLD signal Call-back
         */
        void shell_shutdown()
        {
                printf("Shell shutdown.\n");
                shutdown(c_sockfd, 2);
                close(s_sockfd);
                exit(EXIT_SUCCESS);
        }        

        fdm = posix_openpt(O_RDWR); 
        if (fdm < 0){
                fprintf(stderr, "Error %d on posix_openpt()\n", errno); 
                exit(EXIT_FAILURE); 
        } 
        rc = grantpt(fdm); 
        if (rc != 0){
                fprintf(stderr, "Error %d on grantpt()\n", errno); 
                exit(EXIT_FAILURE); 
        } 
        rc = unlockpt(fdm); 
        if (rc != 0){
                fprintf(stderr, "Error %d on unlockpt()\n", errno); 
                exit(EXIT_FAILURE); 
        } 
        
        // Open the slave PTY
        fds = open(ptsname(fdm), O_RDWR);
        pid_t pid_cli;
        struct termios options;
        if(-1 == (pid_cli = fork())){
                perror("Fork error\n");
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
                pid_t pid_serv;
                if(-1 == (pid_serv=fork())){
                        perror("Fork error\n");
                        exit(EXIT_FAILURE);
                }else if(0 == pid_serv){
                        // This fork receives and send fds's output
                        while(1){
                                memset(buf, '\0', MAXBUF+1);
                                len = read(fdm, buf, MAXBUF);
                                if(len>0){
                                        send(c_sockfd, buf, strlen(buf), 0);
                                }else{
                                        break;
                                }
                        }
                }else{
                        // This fork receives and send client's input
                        while(1){
                                memset(buf, '\0', MAXBUF+1);
                                len = recv(c_sockfd, buf, MAXBUF, 0);
                                if(len > 0){
                                        printf("PID: %d, RUNNING: %s", pid_cli, buf);
                                        write(fdm, buf, strlen(buf));
                                }else if(len==0){
                                        printf("Client quit.\n");
                                        kill(pid_cli, SIGINT);
                                        break;
                                }else{
                                        perror("recv error.\n");
                                        kill(pid_cli, SIGINT);
                                        break;
                                }
                        }
                }
        }

        printf("Going to close connetion.\n");
        shutdown(c_sockfd, 2);
        close(s_sockfd);
}

/**
 *
 * @deprecated
 * Force bash to work interactively.
 */
void bash_server_func(int c_sockfd)
{
        char buf[MAXBUF+1];
        pid_t pid;
        int len;
        int mypipe[2];
        pipe(mypipe);
        if(-1 == (pid = fork())){
                perror("Fork error\n");
                exit(EXIT_FAILURE);
        }else if(0==pid){
                close(mypipe[1]);
                printf("bash PID: %d\n", getpid());
                dup2(c_sockfd, STDOUT_FILENO);
                dup2(c_sockfd, STDERR_FILENO);
                dup2(mypipe[0], STDIN_FILENO);
                execl("/bin/bash", "bash", "-i", NULL);
        }else{
                printf("bash PPID: %d\n", getpid());
                close(mypipe[0]);
                while(1){
                        memset(buf, '\0', MAXBUF+1);
                        len = recv(c_sockfd, buf, MAXBUF, 0);
                        if(len>0){
                                if(buf[strlen(buf)-2] == '\r'){
                                        buf[strlen(buf)-2]='\n';
                                        buf[strlen(buf)-1]='\0';
                                }
                                printf("RUNNING: %s", buf);
                                write(mypipe[1], buf, strlen(buf));
                        }else if(len<0){
                                perror("Receive error.\n");
                                break;
                        }else{
                                printf("Client quit.\n");
                                kill(pid, SIGINT);
                                break;
                        }
                }
        }
}
