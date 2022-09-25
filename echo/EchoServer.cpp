/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-19 06:37
#
# Filename: EchoServer.cpp
#
# Description: 
#
=============================================================================*/
#include "EchoServer.h"
#include <iostream>
#include <functional>

const std::string res = "HTTP/1.1 400 Bad Request\r\nContent-Length: 68\r\nConnection: close\r\n\r\nYour request has bad syntax or is inherently impossible to satisfy.";

EchoServer::EchoServer( EventLoop* loop, const int threadNum, const char* ip, const uint16_t port)
    : m_tcpServer( new TcpServer( loop, threadNum, ip, port ) )
{
    m_tcpServer->setAcceptNewConnCb( std::bind( &EchoServer::handleNewConn, this, std::placeholders::_1 ) );
    m_tcpServer->setCloseConnCb( std::bind( &EchoServer::handleClose, this, std::placeholders::_1 ) );
    m_tcpServer->setErrorCb( std::bind( &EchoServer::handleError, this, std::placeholders::_1 ) );
    m_tcpServer->setMsgCb( std::bind( &EchoServer::handleMsg, this, std::placeholders::_1 ) );
}

EchoServer::~EchoServer()
{
    // delete m_tcpServer;
    Util::Delete<TcpServer>( m_tcpServer );
    std::cout << __func__ << std::endl;
}

void EchoServer::start()
{
    m_tcpServer->start();
}

void EchoServer::handleNewConn( const TcpConnectionSP& tcsp )
{
    // std::cout << __func__ << std::endl;
}

void EchoServer::handleClose( const TcpConnectionSP& tcsp )
{
    // std::cout << __func__ << std::endl;
}

void EchoServer::handleError( const TcpConnectionSP& tcsp )
{
    // std::cout << __func__ << std::endl;
}

void EchoServer::handleMsg( const TcpConnectionSP& tcsp )
{
    std::string msg;
    msg.swap( tcsp->getInput() );
    tcsp->setParseFin( true );

    // tcsp->send( std::move( msg ) );
    tcsp->send( std::move( res ) );
}