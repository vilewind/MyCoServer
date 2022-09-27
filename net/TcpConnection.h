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
#include <atomic>

class EventLoop;
class Channel;
class Timer;
class Coroutine;

class TcpConnection : Util::noncopyable, public std::enable_shared_from_this<TcpConnection>
{
public:
    using TcpConnectionSP = std::shared_ptr<TcpConnection>;
    using TcpCallback = std::function<void( const TcpConnectionSP& )>;

    TcpConnection(  EventLoop*, int fd );
    ~TcpConnection();

    EventLoop* getLoop() const { return m_loop; }
    std::string& getInput() { return m_input; }
    int getInIdx() const { return m_inIdx; }
    Coroutine* getCoroutine() const { return m_co; }
    int getFd() const { return m_sock->getFd(); }
    bool isParseFin() const { return m_parseFin; }

    void setInIdx( int idx = 0 ) { m_inIdx = idx; }
    void setOutput( const std::string& str ) { m_output = str; }
    void setMsgCb( const TcpCallback& tc ) { m_msgCb = tc; }
    void setCloseCb( const TcpCallback& tc ) { m_closeCb = tc; }
    void setParseFin( const bool flag ) { m_parseFin = flag; }

    void send( const std::string& );
    void shutdown();
    void forceClose();

    void establishConnection();
    void destroyConnection();
private:
/*= func*/
    void handleRead();
    
    void handleClose();

    void handleWrite();

    void handleError();

    void sendInLoop( const std::string& );

    void shutdownInLoop();

    void forceCloseInLoop();

    void timerFunc();
/*= data*/
    enum State : int { DisConnected = 0, Connecting, Connected, DisCOnnecting };

    EventLoop* m_loop;
    std::unique_ptr<SocketUtil::Socket> m_sock;
    std::unique_ptr<Channel> m_ch;
    std::unique_ptr<Timer> m_timer;
    Coroutine* m_co;
    std::atomic<int> m_state { Connecting };
    std::atomic<bool> m_parseFin { true };                                          //应用层任务是否完成

    std::string m_input;
    int m_inIdx { 0 };
    std::string m_output;

    TcpCallback m_msgCb;
    TcpCallback m_closeCb;
};

#endif