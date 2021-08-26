#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
// #include <time.h>

#include "/home/fly/Desktop/WebServer/lock/locker.h"
#include "/home/fly/Desktop/WebServer/threadpool/threadpool.h"
// #include "/home/fly/Desktop/TinyWebServer-raw_version注释版/timer/lst_timer.h"
#include "/home/fly/Desktop/WebServer/timer/time_heap.h"
#include "/home/fly/Desktop/WebServer/http/http_conn.h"
#include "/home/fly/Desktop/WebServer/log/log.h"


#define MAX_FD 65536           //最大文件描述符数量
#define MAX_EVENT_NUMBER 10000 //最大事件数

#define SYNLOG  //同步写日志
//#define ASYNLOG //异步写日志

//这三个函数在http_conn.cpp中定义，改变链接属性
extern int addfd(int epollfd, int fd, bool one_shot);
extern int remove(int epollfd, int fd);
extern int setnonblocking(int fd);

//设置定时器相关参数
static int pipefd[2];
// static sort_timer_lst timer_lst;
static time_heap timeHeap;
// static int epollfd = 0;

//信号处理函数
void sig_handler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

//设置信号函数
void addsig(int sig, void(handler)(int), bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void timer_handler()
{
    string s="心跳函数";
    cout<<s<<endl;
    timeHeap.tick();
    alarm(TIMESLOT);
  
}


void show_error(int connfd, const char *info)
{
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}
    
int main(int argc, char *argv[])
{
#ifdef ASYNLOG //和#define对应使用，若ASYNLOG被宏定义，执行#endif前的程序
    Log::get_instance()->init("ServerLog", 2000, 800000, 8); //异步日志模型
#endif    //#ifdef的结束标志

#ifdef SYNLOG
    Log::get_instance()->init("ServerLog", 2000, 800000, 0); //同步日志模型
#endif

    if (argc <= 1)
    {
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }
    int port = atoi(argv[1]);
    addsig(SIGPIPE, SIG_IGN);   //   
   
    //创建线程池
    threadpool<http_conn> *pool = NULL;
    try
    {
        pool = new threadpool<http_conn>;  //类对象 参数初始化
                                                     
    }
    catch (...)
    {
        return 1;
    }

    http_conn *users = new http_conn[MAX_FD];  //为每一个用户连接分配一个资源对象，用来存储http请求内容和响应内容
    assert(users);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);


    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    int flag = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(listenfd, 5);
    assert(ret >= 0);

    //创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];  //当内核事件表中有就绪事件，复制到events中
    epollfd = epoll_create(5);
    assert(epollfd != -1);

    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;

    //创建管道   Q：管道是利用信号来管理超时连接？
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    assert(ret != -1);
    setnonblocking(pipefd[1]);//管道1（写端）设为非阻塞   把超时和终止信号发送给管道0，再被主线程检测到管道0上的事件
    addfd(epollfd, pipefd[0], false);  //把管道0的文件描述符（读端）放入内核事件表：用来读取超时和终止信号

    addsig(SIGALRM, sig_handler, false);
    addsig(SIGTERM, sig_handler, false);
    bool stop_server = false;

    // client_data *users_timer = new client_data[MAX_FD];  //用来给每个新客户连接的socket地址，socket，定时器资源，感觉很鸡肋，没有它，定时器一样工作

    bool timeout = false;
    alarm(TIMESLOT);   //发送一次超时信号,被信号处理函数中的pipe[1]捕获，发送给pipe[0]

    while (!stop_server)
    {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR)
        {
            break;
        }

        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;

            //处理新到的客户连接
            if (sockfd == listenfd)
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);
                if (connfd < 0)
                {
                    continue;
                }
                if (http_conn::m_user_count >= MAX_FD)
                {
                    show_error(connfd, "Internal server busy");
                    
                    continue;
                }
                string s="新连接";
                cout<<s<<endl;
                users[connfd].init(connfd);//对新客户连接的资源对象进行参数初始化
                time_t cur=time(NULL);
                timeHeap.add_timer(connfd,cur+3*TIMESLOT);
            }

            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                timeHeap.del_timer(timeHeap.m[sockfd]);
                epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, 0);//删除内核事件注册表中fd上的事件
                close(sockfd);
            }

            //处理信号
            else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN)) //若就绪事件为管道0上的事件，且检测到超时信号，当就绪事件被处理完后，删除过期任务,tick()
            {
                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1)
                {
                    continue;
                }
                else if (ret == 0)
                {
                    continue;
                }
                else
                {
                    for (int i = 0; i < ret; ++i)
                    {
                        switch (signals[i])
                        {
                        case SIGALRM: //超时信号
                        {
                            string s="超时信号";
                            cout<<s<<endl;
                            timeout = true;
                            break;
                        }
                        case SIGTERM://终止信号
                        {
                            stop_server = true;
                        }
                        }
                    }
                }
            }

            //处理客户连接上接收到的数据
            else if (events[i].events & EPOLLIN)
            {
                if (users[sockfd].read_once()) //数据读入完成后，输出客户端的socket地址
                {
                    //若监测到读事件，将该事件放入请求队列
                    pool->append(users + sockfd); //append是bool类型，可以不用返回值吗？ 可以，（append设为void是不是更好）
                    time_t cur=time(NULL);
                    timeHeap.adjust_timer(sockfd,cur+3*TIMESLOT);
                }
                else //读物出错
                {
                    timeHeap.del_timer(timeHeap.m[sockfd]);
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, 0);//删除内核事件注册表中fd上的事件
                    close(sockfd);
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                // util_timer *timer = users_timer[sockfd].timer;
                if (users[sockfd].write())
                {
                    time_t cur=time(NULL);
                    timeHeap.adjust_timer(sockfd,cur+3*TIMESLOT);
                }
                else
                {
                    timeHeap.del_timer(timeHeap.m[sockfd]);
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, 0);//删除内核事件注册表中fd上的事件
                    close(sockfd);
                }
            }
        }
        
        //内核事件表中的所有事件处理完，开始检查有没有过期连接
        if (timeout)  //删除过期任务
        {
            timer_handler();
            timeout = false;
        }
    }
    close(epollfd);
    close(listenfd);
    close(pipefd[1]);
    close(pipefd[0]);
    delete[] users;
    delete pool;
    return 0;
}
