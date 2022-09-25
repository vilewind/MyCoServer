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
    m_tcpServer->setAcceptNewConnCb( std::bind( &HttpServer::handleNewConn, this, std::placeholders::_1 ) );
    m_tcpServer->setCloseConnCb( std::bind( &HttpServer::handleClose, this, std::placeholders::_1 ) );
    m_tcpServer->setErrorCb( std::bind( &HttpServer::handleError, this, std::placeholders::_1 ) );
    m_tcpServer->setMsgCb( std::bind( &HttpServer::handleMsg, this, std::placeholders::_1 ) );
    
    m_tcpServer->start();
}

HttpServer::~HttpServer()
{
    // delete m_tcpServer;
    Util::Delete<TcpServer>( m_tcpServer );
    std::cout << __func__ << std::endl;
}

void HttpServer::handleNewConn( const TcpConnectionSP& tcsp )
{
    Coroutine* co = tcsp->getMyCo();
    HttpSession* hs = new HttpSession( tcsp );
    m_hsm[tcsp ] = hs;
    co->setCallback([=]()
    {
       hs->operator()();
    });
}

void HttpServer::handleClose( const TcpConnectionSP& tcsp )
{
    HttpSession* hs = m_hsm[tcsp];
    m_hsm.erase(tcsp);
    // if (  hs )
    // {
    //     delete hs;
    // } 
    Util::Delete<HttpSession>( hs );  
}

void HttpServer::handleError( const TcpConnectionSP& tcsp )
{
    // std::cout << __func__ << std::endl;
}


void HttpServer::handleMsg( const TCSP& tcsp )
{
    tcsp->setParseFin( false );
    Coroutine::Resume(tcsp->getMyCo());
}