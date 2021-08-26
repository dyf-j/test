#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include "/home/fly/Desktop/WebServer/lock/locker.h"
class http_conn
{
public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    void init(int sockfd);
    void close_conn(bool real_close = true);
    void process();
    bool read_once();
    bool write();
    sockaddr_in *get_address()
    {
        return &m_address;
    }
    // void initmysql_result(connection_pool *connPool);

private:
    void init();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line() { return m_read_buf + m_start_line; };
    LINE_STATUS parse_line();
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;
    static int m_user_count;

private:
    int m_sockfd;  //读客户连接socket
    sockaddr_in m_address;
    
    char m_read_buf[READ_BUFFER_SIZE];//读缓存
    int m_read_idx;   //读缓冲区已经读入的客户数据的最后一个字节的下一个位置 （读到服务端）
    int m_checked_idx;  //正在分析的  字符  在读缓冲区的位置 
    int m_start_line;   //正在解析的行的起始位置
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;  //待发送的字节数
    CHECK_STATE m_check_state;  //主机状态
    METHOD m_method;
    char m_real_file[FILENAME_LEN];   //客户请求的目标文件的完整路径
    char *m_url;   //目标文件的文件名
    char *m_version;  //http协议版本号
    char *m_host;//主机名
    int m_content_length;  //请求消息体的长度
    bool m_linger;        //http请求是否要求保持连接
    char *m_file_address;    //目标文件被mmap到内存中的起始位置
    struct stat m_file_stat;   //目标文件的状态：如是否存在，是否为目录，是否可读，大小，
    struct iovec m_iv[2];     //采用writev来执行写操作， 分散写
    int m_iv_count;   //内存块的数量
    // int cgi;        //是否启用的POST
    char *m_string; //存储请求头数据

    //下面两个变量，在书中在write函数内被声明并定义，两者范围在使用时有什么区别？
    int bytes_to_send;
    int bytes_have_send;
};

#endif
