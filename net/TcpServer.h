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
#include <memory>


class TcpConnection;

class TcpServer
{
public:
     using TcpConnectionSP = TcpConnection::TcpConnectionSP;
    using TcpCallback = TcpConnection::TcpCallback;

    TcpServer( EventLoop*, const int threadNum = std::thread::hardware_concurrency() - 1, const char* ip = "127.0.0.1", const uint16_t port = 8888 );
    ~TcpServer();

    void start();

    void setInitConn( const TcpCallback& tc ) { m_initConnCb = tc; }

    void setClearConn( const TcpCallback& tc ) { m_clearConnCb = tc; }

    void setMsgCb( const TcpCallback& tc ) { m_msgCb = tc; }
private:
/*= func*/
    void acceptorFunc();

    void handleMsg();

    void handleCleanConn( const TcpConnectionSP&);
    
/*= data*/
    EventLoop* m_loop;							                //主reactor
    std::unique_ptr<SocketUtil::Addr> m_addr;					//服务器Addr
    std::unique_ptr<SocketUtil::Socket> m_sock;					//服务器Socket
    std::unique_ptr<Channel> m_acceptor;						//主reactor负责监听并分发任务
    std::unique_ptr<IOThreadPool> m_IOTP;                       //IO线程池

    std::map<int, TcpConnectionSP> m_conns;

    TcpCallback m_initConnCb;                                   //应用层初始化连接        
    TcpCallback m_clearConnCb;                                  //应用层连接清理    
    TcpCallback m_msgCb;
};

#endif