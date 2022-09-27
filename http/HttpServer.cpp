/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-23 03:58
#
# Filename: HttpServer.cpp
#
# Description: 
#
=============================================================================*/

#include "HttpServer.h"
#include "../coroutine/Coroutine.h"
#include "HttpSession.h"
#include <iostream>
#include <functional>

HttpServer::HttpServer( EventLoop* loop, const int threadNum, const char* ip, const uint16_t port )
    : m_tcpServer( new TcpServer( loop, threadNum, ip, port ) )
{
    std::cout << __func__ << std::endl;
    m_tcpServer->setInitConn( std::bind( &HttpServer::initConn, this, std::placeholders::_1 ) );
    m_tcpServer->setClearConn( std::bind( &HttpServer::clearConn, this, std::placeholders::_1 ) );
    m_tcpServer->setMsgCb( std::bind( &HttpServer::handleMsg, this, std::placeholders::_1 ) );
    
    m_tcpServer->start();
}

HttpServer::~HttpServer()
{
    // delete m_tcpServer;
    Util::Delete<TcpServer>( m_tcpServer );
    std::cout << __func__ << std::endl;
}

void HttpServer::start()
{
    m_tcpServer->start();
}

void HttpServer::initConn( const TcpConnectionSP& tcsp )
{    
    HttpSession* hs = new HttpSession( tcsp );
    m_hsm[tcsp ] = hs;
    Coroutine* co = tcsp->getCoroutine();
    co->setCallback( [=]()
    {
        hs->operator()();
    } );
}

void HttpServer::clearConn( const TcpConnectionSP& tcsp )
{
    HttpSession* hs = m_hsm[tcsp];
    m_hsm.erase(tcsp);
    Util::Delete<HttpSession>( hs );  
}


void HttpServer::handleMsg( const TcpConnectionSP& tcsp )
{
    // tcsp->setParseFin( false );
/// @note 新的http请求来临时，重置parse标志    
    if ( tcsp->isParseFin() == true )
    {   
        tcsp->setParseFin( false );
    }
    Coroutine::Resume(tcsp->getCoroutine());
}

#define HTTPTEST
#ifdef HTTPTEST

int main()
{
    EventLoop el;
    HttpServer hs( &el );
    hs.start();

    try
    {
        el.loop();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    return 0;   
}

#endif