/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-09 09:22
#
# Filename: Server.h
#
# Description: 
#
=============================================================================*/
#ifndef __TCPSERVER_H__
#define __TCPSERVER_H__

#include "Util.h"
#include "TcpConnection.h"
#include "EventLoop.h"
#include "Channel.h"
#include "IOThreadPool.h"
#include <vector>
#include <map>
#include <mutex>

class TcpServer
{
public:
    using TcpConnectionSP = TcpConnection::TcpConnectionSP;
    using TcpCallback = TcpConnection::TcpCallback;

     TcpServer( EventLoop*, const int threadNum = 0, 
        const char* ip = "127.0.0.1", const uint16_t port = 8888);
    ~TcpServer();

    void start();

    void setAcceptNewConnCb( const TcpCallback& cb ) { m_acceptCb = cb; }

    void setCloseConnCb( const TcpCallback& cb ) { m_closeCb = cb; }

    void setErrorCb( const TcpCallback& cb ) { m_errorCb = cb; }

    void setMsgCb( const TcpCallback& cb ) { m_msgCb = cb; }

private:
/*= func*/
    void acceptNewConn();

    void handleMsg( TcpConnectionSP );

    void cleanConn( TcpConnectionSP );

    void dealErroOnConn();

/*= data*/
    EventLoop* m_loop;							//主reactor
    SocketUtil::Addr* m_addr;					//服务器Addr
    SocketUtil::Socket* m_sock;					//服务器Socket
    Channel* m_acceptor;						//主reactor负责监听并分发任务

    IOThreadPool* m_ioPool;		

    std::map<int, TcpConnectionSP> m_tcpConns;
    std::mutex m_mtx { };

    TcpCallback m_acceptCb;
    TcpCallback m_closeCb;
    TcpCallback m_errorCb;
    TcpCallback m_msgCb;

};

#endif