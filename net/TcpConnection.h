/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-09 10:54
#
# Filename: TcpConnection.h
#
# Description: 封装TCP Connection，包含通信函数和连接管理函数
#
=============================================================================*/
#ifndef __TCPCONNECTION_H__
#define __TCPCONNECTION_H__

#include "Util.h"
#include <functional>
#include <string>
#include <memory>

class EventLoop;
class Channel;
class Timer;

class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
public:
    using TcpConnectionSP = std::shared_ptr<TcpConnection>;
    using TcpCallback = std::function<void( TcpConnectionSP )>;

    TcpConnection(  EventLoop*, int fd );
    ~TcpConnection();

    void addChannelToLoop();

    void send( const std::string& );
    void sendInLoop();

    void shutdown();
    void shutdownInLoop();

    void forceClose();
    void forceCloseInLoop();

    bool isDisConn() const { return m_isDisConn; }
    int getFd() { return m_fd; }
    Channel* getChannel() { return m_ch; }
    std::string& getInput() { return m_input; }
    std::string& getOutput() { return m_output; }
    int& getInIdx() { return m_inIdx; }
    EventLoop* getOwnerLoop() const { return m_loop; }

    void setInIdx( int idx = 0 ) { m_inIdx = idx; }
    void setParseFin( bool flag ) { m_parseFin = true; }
    void setMsgCb( const TcpCallback& cb ) { m_msgCb = cb; }
    void setCloseCb( const TcpCallback& cb ) { m_closeCb = cb; }

private:
/*= func*/
    void handleRead();
    void handleWrite();
    void handleError();
    void handleClose();

    void timerFunc();
/*= data*/
    EventLoop* m_loop { nullptr };
    int m_fd { -1 };
    Channel* m_ch { nullptr };
    Timer* m_timer { nullptr };

    bool m_isDisConn { false };
    bool m_parseFin { false };                      //应用层任务正在执行标志
    bool m_closing { false };                       //半关闭标志位s

    /* 读写缓冲*/
    std::string m_input;
    int m_inIdx { 0 };
    std::string m_output;

    TcpCallback m_msgCb;
    TcpCallback m_closeCb;                          //清理连接
};

#endif