---
MyCoServer

---

# Introduction

本项目基于c++11和epoll实现了简单回射服务器和HTTP服务器。

## 并发模型

本项目采取主从reactor模式，使用固定数量的IO线程池，保证“one eventloop per thread”。主reactor负责监听和分发——监听连接并分配给随机选取的subreactor；从reactor负责执行IO事件（包括定时事件）。并发模型的细节参考了muduo库，比如基于eventfd实现runInLoop和queueInLoop的跨线程执行用户回调；使用timerfd实现定时器，避免使用sleep，造成资源浪费。

<img src="https://github.com/vilewind/MyCoServer/blob/main/IMG/1.png" />

## 协程设计

本项目的协程实现参考了libco和[云风](https://github.com/cloudwu/coroutine)的协程库，采用非对称模式和共享栈模式，使用汇编代码（libco）切换协程上下文。

## HTTP状态机解析

本项目参考了《linux高性能服务器》第15章，使用状态机解析HTTP协议。当服务器端读到客户的http请求，则唤醒http解析协程，根据主状态机的情况，确定继续读（NO_RQUEST）或响应/写（BAD_REUQEST、GET_REQUEST等），流程图如下：

<img src="https://github.com/vilewind/MyCoServer/blob/main/IMG/2.png" style="zoom:50%;" />



# Tech

- 基于EPOLL实现reactor模式；
- IO采用非阻塞模式；读写采用边缘触发，使用循环读写直到errno==EAGAIN；主reactor监听连接，采用水平触发；
- 定时器使用timerfd实现，将定时器channel化，避免通过sleep浪费cpu资源，同时也能保证时间精度较高；使用时间轮完成定时器功能，定时剔除不活跃连接；
- 使用eventfd实现线程间通信，并实现跨线程“执行”用户回调；
- 采取非对程模式和共享栈模式的协程池，主要用于HTTP协议解析；
- 采用static thread_local关键字，保证IO线程池、协程池合理的构造和析构。

# Build

```bash
./build.sh
```

# Perfomance Test

- 本项目采取HTTP_LOAD进行测试，服务器IO线程数为2，并发客户进程数为1000时，10秒内的QPS为1141.68。具体结果如下：
<img src="https://github.com/vilewind/MyCoServer/blob/main/IMG/3.png" style="zoom:50%;" />
   

# Others

本项目的性能较差还需继续优化。并未将IO任务与解析任务分离，需重新设计IO线程和解析线程。http解析仍存在问题，亟待解决。

