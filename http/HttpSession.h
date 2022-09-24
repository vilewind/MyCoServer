/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-20 10:36
#
# Filename: HttpSession.h
#
# Description: 
#
=============================================================================*/
#ifndef __HTTPSESSION_H__
#define __HTTPSESSION_H__

#include <memory>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>

class TcpConnection;
class Coroutine;

class HttpSession
{
public:
    using TcpConnectionSP = std::shared_ptr<TcpConnection>;
    // using TcpConnectionWP = std::weak_ptr<TcpConnection>;
    static const int FILENAME_LEN = 200;
    enum METHOD { GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATCH };
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };
    enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };

     HttpSession( TcpConnectionSP );
    ~HttpSession();

    void operator()();

    Coroutine* getMyCo() const { return m_co; }

private:
/*= func*/
    HTTP_CODE parseProcess();
    void requestProcess( HTTP_CODE );

    HTTP_CODE parseRequestLine( );
    HTTP_CODE parseHeader( );
    HTTP_CODE parseContent( );
    LINE_STATUS parseLine();

    HTTP_CODE confirmResource();
    void addContent( const std::string& );
    void addStatusLine( int status, const std::string& title );
    void addHeader( int content_len );
    void addContentLen( int content_len );
    void addLinger( );
    void addBlankLine();

    void readFile( const std::string& filename );
/*= data*/
    TcpConnectionSP m_tcsp;
    CHECK_STATE m_checkState { CHECK_STATE_REQUESTLINE };
    METHOD m_method { GET };
    Coroutine* m_co;


    std::string m_file { };
    std::string m_url { };
    std::string m_version { };
    std::string m_host { };
    int m_contentLen { 0 };
    bool m_linger { false };

    /* http请求当前正在解析的行*/
    int m_inIdx { 0 };                                        //解析行的首部
    int m_endIdx { 0 };                                       //解析行的首部（未解析、行不完整）或下一行的首部

    /* 目标资源的文件属性：是否存在；读写权限；是否为文件夹等*/    
    struct  stat m_fileStat;
    std::string m_str;
};

#endif