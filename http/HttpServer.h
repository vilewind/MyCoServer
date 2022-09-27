/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-23 03:58
#
# Filename: HttpServer.h
#
# Description: 
#
=============================================================================*/
#ifndef __HTTPSERVER_H__
#define __HTTPSERVER_H__

#include "HttpSession.h"
#include "../net/TcpServer.h"
#include "../net/EventLoop.h"
#include "../net/TcpConnection.h"
#include "../coroutine/Coroutine.h"
#include <string>
#include <map>

using TcpConnectionSP = TcpConnection::TcpConnectionSP;
using HttpSessionMap = std::map<TcpConnectionSP, HttpSession*>;

class HttpServer
{
public:
    HttpServer( EventLoop*, const int threadNum = 2, const char* ip = "127.0.0.1", const uint16_t port = 8888 );
    ~HttpServer();

    void start();
    
private:
/*= func*/
    void initConn( const TcpConnectionSP& );
    void clearConn( const TcpConnectionSP& );
    void handleMsg( const TcpConnectionSP& );
/*= data*/
    TcpServer* m_tcpServer;
    HttpSessionMap m_hsm;
};

#endif
