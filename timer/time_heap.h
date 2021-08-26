#ifndef intIME_HEAP
#define intIME_HEAP
#include <iostream>
#include <netinet/in.h>
#include <time.h>
#include<vector>
#include<unordered_map>
#include <algorithm>
#include <arpa/inet.h> 
#include <functional> 
#include <assert.h> 
#include <chrono>
#include<sys/epoll.h>
#include<fcntl.h>
#include<unistd.h>
#include "/home/fly/Desktop/WebServer/http/http_conn.h"
using namespace std;

#define TIMESLOT 3       //超时单位
int epollfd=0;

class heap_timer
{  
public:
    int sockfd;
    time_t expire;
};

class time_heap
{
public:
    time_heap(){
        cur_size=0;
	    heap_.resize(64); //开辟64个元素的数组
    }

    ~time_heap()
    {
        heap_.clear();
        m.clear();
    }
    
    //新连接来到，根据新的sockfd和超时时间（timeOut=cur+3*T）,创建定时器
    void add_timer(int id,time_t timeOut) 
    {
        int i=cur_size;
        ++cur_size;
        if(m.count(id) == 0) {
            /* 新节点：堆尾插入，调整堆 */
            m[id] = i;
            // heap_.push_back({id, timeOut});//定时器添加
            heap_[i]={id,timeOut};
        }else{
            time_t cur=time(NULL);
            adjust_timer(id,cur+3*TIMESLOT);
        }
    }

    //根据已连接skcfd和新的超时时间，以及哈希表找到定时器在数组中的下标，进行下滤操作，维持最小堆性质
    void adjust_timer(int id,time_t timeOut)
    {
        if(!heap_.empty() && m.count(id) > 0){
            heap_[m[id]].expire=timeOut;
            sift_down(m[id],heap_.size());
        }
    }

    //当sockfd读写出错或超时，把定时器从文件描述符中删除，且关闭sockfd
    void del_timer(int index)
    {
        if(!heap_.empty() && index >= 0 && index <cur_size){
            int i = index;
            int n = cur_size - 1;
            if(i <= n) {
                swap(heap_[i], heap_[n]);
                m[heap_[i].sockfd] = i;
                m[heap_[n].sockfd] = n;
                sift_down(i,n);
            }
            /* 队尾元素删除 */
            m.erase(heap_[n].sockfd);
            // heap_.pop_back();
            --cur_size;
        }
    }

    void tick()
    {
        while(cur_size>0){
            heap_timer timer=heap_[0];
            time_t cur=time(NULL);
            if(cur<timer.expire) break;
            string s1="有超时事件";
            cout<<s1<<endl;
            http_conn::m_user_count--;
            del_timer(0);
            epoll_ctl(epollfd, EPOLL_CTL_DEL, timer.sockfd, 0);//删除内核事件注册表中fd上的事件
            close(timer.sockfd);//关闭文件描述符
            string s2="删除超时事件";
            cout<<s2<<endl;
        }
    }

    void sift_down(int index, int n)
    {
        if(index >= 0 && index < heap_.size()){
            int i = index;
            int j = i * 2 + 1;
            while(j < n) {
                if(j + 1 < n && heap_[j + 1].expire < heap_[j].expire) j++;
                if(heap_[i].expire <= heap_[j].expire) break;
                swap(heap_[i], heap_[j]);
                m[heap_[i].sockfd] = i;
                m[heap_[j].sockfd] = j;
                i = j;
                j = i * 2 + 1;
            }
        }
    }
   
    vector<heap_timer> heap_;
    unordered_map<int,int> m;  //前面是sockfd，后面是下标（因为fd每个都不同,下标在进行删除操作后，可能相同，所以fd放在前面）,用于下滤操作
    int cur_size;
};

#endif
