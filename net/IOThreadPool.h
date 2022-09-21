/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-13 13:31
#
# Filename: IOThreadPool.h
#
# Description: 
#
=============================================================================*/
#ifndef __IOTHREADPOOL_H__
#define __IOTHREADPOOL_H__

#include <thread>
#include <vector>

class EventLoop;

class IOThreadPool
{
public:
    // IOThreadPool(int size = std::thread::hardware_concurrency());
    IOThreadPool(int size = std::thread::hardware_concurrency());
    ~IOThreadPool();

    void thFunc();

    EventLoop* getEventLoop(int fd);

    size_t size() const { return m_ths.size();}
private:
    std::vector<std::thread*> m_ths;
    std::vector<EventLoop*> m_reactors;
};

#endif
