Raw_version文档
===============
Linux下C++轻量级Web服务器，助力初学者快速实践网络编程，搭建属于自己的服务器.

* 使用**线程池 + epoll(ET和LT均实现) + 模拟Proactor模式**的并发模型
* 使用**状态机**解析HTTP请求报文，支持解析**GET**请求
* 通过访问服务器，可以请求服务器**图片和视频文件**
* 时间堆模式定时器
* 经Webbench压力测试可以实现**上万的并发连接**数据交换

基础测试
------------
* 服务器测试环境
	* Ubuntu版本18.04
	
* 浏览器测试环境
	* Chrome
	* FireFox

    ```

