#include <iostream>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<cstring>
#include<sys/time.h>
#include<sys/epoll.h>
#include<fcntl.h>
#include<errno.h>
#include"ThreadPoll.hpp"

void recmessage(int epfd,int clnt){
    char tempbuffer[4096];
    char buffer[20];//临时数据
    const char ans[]="这是我的回复:";
    std::cout<<"开始读取数据"<<std::endl;
    strcpy(tempbuffer,ans);
    int countlen=sizeof(ans)-1;
    while(true){
        int strlen=read(clnt,buffer,10);
        if(strlen ==0){
            std::cout<<"client :"<<clnt<<"is disconnected"<<std::endl;
            epoll_ctl(epfd,EPOLL_CTL_DEL,clnt,nullptr);//本身线程安全
            close(clnt);
        }else if(strlen<0){
            if(errno==EAGAIN){//errno也是线程安全
                write(clnt,tempbuffer,countlen);
                break;
            }
        }else{
            memcpy(&tempbuffer[countlen],buffer,strlen);
            countlen+=strlen;
            std::cout<<"读取数据长度为"<<countlen<<std::endl;
        }
    }
}

void setnoblocking(int fd){
    int flag=fcntl(fd,F_GETFL,0);
    fcntl(fd,F_SETFL,flag|O_NONBLOCK);
}
int main(int argc,char* argv[]) {
    ThreadPool &mypool=ThreadPool::getThreadPool();
    int serv_sock;
    int clnt_sock;
    sockaddr_in serv_addr;
    sockaddr_in clnt_addr;
    socklen_t clnt_addr_size;
    int strlen,i;
    char buffer[4096];
    epoll_event* ep_events;
    epoll_event event;
    int epfd,events_cnt;

    if(argc!=2){
        std::cout<<"请正确输入端口号"<<std::endl;
        return 0;
    }
    serv_sock=socket(PF_INET,SOCK_STREAM,0);
    if(serv_sock==-1){
        std::cout<<"socket()失败"<<std::endl;
        return 0;
    }
    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_addr.s_addr= htonl(INADDR_ANY);
    serv_addr.sin_port=htons(std::stol(argv[1]));
    if(bind(serv_sock,(sockaddr*)&serv_addr,sizeof(serv_addr))==-1){
        std::cout<<"bind()失败"<<std::endl;
        return 0;
    }
    if(listen(serv_sock,100)==1){
        std::cout<<"listen()失败"<<std::endl;
        return 0;
    }

    epfd= epoll_create(100);
    ep_events=new epoll_event[100];
    event.events=EPOLLIN;
    event.data.fd=serv_sock;
    epoll_ctl(epfd,EPOLL_CTL_ADD,serv_sock,&event);

    while(true){
        events_cnt= epoll_wait(epfd,ep_events,100,-1);
        if(events_cnt==-1){
            std::cout<<"epoll_wait() failed"<<std::endl;
            break;
        }
        for(i=0;i<events_cnt;++i){
            if(ep_events[i].data.fd==serv_sock){
                clnt_addr_size=sizeof(clnt_addr);
                clnt_sock=accept(serv_sock,(sockaddr*)&clnt_addr,&clnt_addr_size);
                setnoblocking(clnt_sock);
                event.events=EPOLLIN|EPOLLET;
                event.data.fd=clnt_sock;
                epoll_ctl(epfd,EPOLL_CTL_ADD,clnt_sock,&event);
                std::cout<<"client connected"<<clnt_sock <<std::endl;
            }else{
                int k=ep_events[i].data.fd;
                mypool.commit(recmessage,epfd,k);
            }
        }
    }
    close(serv_sock);
    close(epfd);
    return 0;
}
