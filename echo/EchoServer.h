/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-19 06:33
#
# Filename: EchoServer.h
#
# Description: 
#
=============================================================================*/
#include "../net/TcpServer.h"
#include "../net/EventLoop.h"
#include "../net/TcpConnection.h"
#include <string>
#include <map>
#include <mutex>

class EchoServer
{
public:
     using TcpConnectionSP = TcpConnection::TcpConnectionSP;
    
    EchoServer( EventLoop*, const int threadNum = 2, const char* ip = "127.0.0.1", const uint16_t port = 8888);
    ~EchoServer();

    void start();

private:
/*= func*/
    void handleNewConn(const TcpConnectionSP& );
    void handleError(const TcpConnectionSP& );
    void handleClose( const TcpConnectionSP& );
    void handleMsg( const TcpConnectionSP& );
/*= data*/
    TcpServer* m_tcpServer;
};