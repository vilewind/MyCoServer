/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-13 13:31
#
# Filename: IOThreadPool.h
#
# Description: 利用static thread_local 创建IO线程池
#
=============================================================================*/
#ifndef __IOTHREADPOOL_H__
#define __IOTHREADPOOL_H__

#include <thread>
#include <vector>
#include <memory>

class EventLoop;

class IOThreadPool
{
public:
    using TUP = std::unique_ptr<std::thread>;

    IOThreadPool(int size = std::thread::hardware_concurrency());
    ~IOThreadPool();

    void thFunc();

    EventLoop* getEventLoop(int fd);

    size_t size() const { return m_ths.size();}
private:
    std::vector<TUP> m_ths;
    std::vector<EventLoop*> m_reactors;
};

#endif
