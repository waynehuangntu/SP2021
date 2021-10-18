#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdbool.h>


#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)
#define GET_VARIABLE_NAME(Variable) (#Variable)

typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    char buf[512];  // data sent by/to client
    size_t buf_len;  // bytes used by buf
    // you don't need to change this.
    int id;
    int wait_for_write;  // used by handle_read to know if the header is read or not.
} request;

server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list

const char* accept_read_header = "ACCEPT_FROM_READ";
const char* accept_write_header = "ACCEPT_FROM_WRITE";

static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void free_request(request* reqP);
// free resources used by a request instance

typedef struct {
    int id;          //902001-902020
    int AZ;          
    int BNT;         
    int Moderna;     
}registerRecord;

int handle_read(request* reqP) {
    int r;
    char buf[512];

    // Read in request from client
    r = read(reqP->conn_fd, buf, sizeof(buf));
    if (r < 0) return -1;
    if (r == 0) return 0;
    char* p1 = strstr(buf, "\015\012");
    int newline_len = 2;
    if (p1 == NULL) {
       p1 = strstr(buf, "\012");
        if (p1 == NULL) {
            ERR_EXIT("this really should not happen...");
        }
    }
    size_t len = p1 - buf + 1;
    memmove(reqP->buf, buf, len);
    reqP->buf[len - 1] = '\0';
    reqP->buf_len = len-1;
    return 1;
}

int main(int argc, char** argv) {

    // Parse args.
    if (argc != 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }

    struct sockaddr_in cliaddr;  // used by accept()
    int clilen;

    int conn_fd;  // fd for a new connection with client
    int file_fd;  // fd for file that we open for reading
    char buf[512];
    int buf_len;
    struct flock lock;
    bool write_lock = false;
    bool read_lock = false;


    file_fd = open("registerRecord",O_RDWR);
    if(file_fd < 0)
    {
        fprintf(stderr,"no registerRecord file exist\n");
    }

    // Initialize server
    init_server((unsigned short) atoi(argv[1]));
    printf("the fd is %d\n",svr.listen_fd);


    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);

    struct timeval tv;
    fd_set original_set,workingset;
    FD_ZERO(&original_set);
    FD_SET(svr.listen_fd,&original_set);
    fcntl(svr.listen_fd,F_SETFL,O_NONBLOCK);
    while (1) 
    {
        // TODO: Add IO multiplexing
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        //memcpy(&workingset,&original_set,sizeof(original_set));
        workingset = original_set;
        int ret = select(maxfd,&workingset,NULL,NULL,&tv); 
        if(ret< 0)
        {
            fprintf(stderr,"select error \n");
            perror("select");
            exit(1);
        }
        if (ret == 0)
        {
            continue;
            //break;
        }
       
        /*如果不用select "accept為slow syscall process may be blocked eternally, so we use select to make sure the reading data  is ready*/
       for(int i = 0;i<maxfd;i++)
       {
            if(FD_ISSET(i,&workingset))
            {
                if(i == svr.listen_fd)
                {    
                    printf("Listening socket is readable\n");
                    clilen = sizeof(cliaddr);
                    conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);//new connection established
                    if (conn_fd < 0) {
                        if (errno == EINTR || errno == EAGAIN) continue;  // try again
                        if (errno == ENFILE) 
                        {
                            (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                            continue;
                        }
                        ERR_EXIT("accept");
                    }
                    else // accept succeed
                    {
                        printf("New connection incoming%d\n",conn_fd);
                        FD_SET(conn_fd,&original_set);
                        // Check new connection
                        requestP[conn_fd].conn_fd = conn_fd;
                        strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
                        fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);
                        char* entry_buf = "Please enter your id (to check your preference order):";
                        write(conn_fd,entry_buf,strlen(entry_buf));
                    }
                }

                else // data from existing connection not establishing new connection, receive it
                {
                    
                    int ret = handle_read(&requestP[conn_fd]); // parse data from client to requestP[conn_fd].buf
	                if (ret < 0) {
                        fprintf(stderr, "bad request from %s\n", requestP[conn_fd].host);
                        continue;
                    }
                    int input_id = atoi(requestP[conn_fd].buf);
                    

                    if(input_id > 902020 || input_id < 902001){
                        sprintf(buf,"Invalid ID, please try it again.\n");
                        write(requestP[conn_fd].conn_fd,buf,strlen(buf));
                        continue;
                    }
                    requestP[conn_fd].conn_fd = atoi(requestP[conn_fd].buf);
                

#ifdef READ_SERVER  
                    
                    registerRecord record;
                    
                    //set file lock to "the whole file"
                    lock.l_type = F_RDLCK;
                    lock.l_whence = SEEK_SET;
                    lock.l_start = 0;
                    lock.l_len = 0;
                    
                    if(fcntl(file_fd,F_SETLK,&lock) != -1 && write_lock != true) // file is not locked
                    {
                        read_lock = true;
                        lseek(file_fd,sizeof(registerRecord) * (input_id - 902001) ,SEEK_SET);
                        read(file_fd,&record,sizeof(registerRecord));//將data寫進來
                        sprintf(buf,"Your preference order is ");
                        
                        for(int i = 1 ;i<=3;i++)
                        {
                            if(record.AZ == i)
                                sprintf(buf+strlen(buf),"AZ");
                            else if(record.BNT == i)
                                sprintf(buf+strlen(buf),"BNT");
                            else
                                sprintf(buf+strlen(buf),"Moderna");
                            if(i != 3)
                                sprintf(buf+strlen(buf)," > ");
                        }
                        sprintf(buf+strlen(buf),".\n");
                        write(requestP[conn_fd].conn_fd,buf,strlen(buf));
                    }
                    
                    else{ // the file is locked
                        sprintf(buf,"Locked.\n");
                        write(requestP[conn_fd].conn_fd,buf,strlen(buf));
                    }

                    //unlock the file
                    lock.l_type = F_UNLCK;
                    fcntl(file_fd,F_SETLK,&file_fd);
                    read_lock = false;
                    



                    
                    fprintf(stderr, "%s", requestP[conn_fd].buf);
                    sprintf(buf,"%s : %s",accept_read_header,requestP[conn_fd].buf);
                    fprintf(stderr,"the buffer content is %s \n",buf);
                    write(requestP[conn_fd].conn_fd, buf, strlen(buf));// write message to client  

#elif defined WRITE_SERVER
                    
                    registerRecord record;
                    

                    fprintf(stderr, "%s", requestP[conn_fd].buf);
                    sprintf(buf,"%s : %s",accept_write_header,requestP[conn_fd].buf);
                    write(requestP[conn_fd].conn_fd, buf, strlen(buf));                
#endif 

                    close(requestP[conn_fd].conn_fd);
                    free_request(&requestP[conn_fd]);
                    

                }
            }
       }
        
    }
    free(requestP);
    return 0;
}

// ======================================================================================================
// You don't need to know how the following codes are working
#include <fcntl.h>

static void init_request(request* reqP) {
    reqP->conn_fd = -1;
    reqP->buf_len = 0;
    reqP->id = 0;
}

static void free_request(request* reqP) {
    /*if (reqP->filename != NULL) {
        free(reqP->filename);
        reqP->filename = NULL;
    }*/
    init_request(reqP);
}

static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }

    // Get file descripter table size and initialize request table
    maxfd = getdtablesize();
    requestP = (request*) malloc(sizeof(request) * maxfd);
    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (int i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);

    return;
}
