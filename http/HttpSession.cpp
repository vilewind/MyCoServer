/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-20 10:36
#
# Filename: HttpSession.cpp
#
# Description: 
#
=============================================================================*/
#include "HttpSession.h"
#include "../net/EventLoop.h"
#include "../net/TcpConnection.h"
#include "../coroutine/Coroutine.h"
#include <string>
#include <unistd.h>
#include <fstream>
#include <iterator>

const std::string ok_200_title = "OK";
const std::string error_400_title = "Bad Request";
const std::string error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const std::string error_403_title = "Forbidden";
const std::string error_403_form = "You do not have permission to get file from this server.\n";
const std::string error_404_title = "Not Found";
const std::string error_404_form = "The requested file was not found on this server.\n";
const std::string error_500_title = "Internal Error";
const std::string error_500_form = "There was an unusual problem serving the requested file.\n";
const std::string doc_root = "/var/www/html";

const static std::string CRLF{"\r\n"};
const static std::string CR{"\r"};
const static std::string LF{"\n"};
const static std::string SP{" "};

HttpSession::HttpSession( TcpConnectionSP tcsp )
    : m_tcsp( tcsp ),
      m_inIdx( m_tcsp->getInIdx() ),
      m_endIdx( m_inIdx ),
      m_co( m_tcsp->getOwnerLoop()->getCoroutineInstanceInCurrentLoop() )
{
    m_co->setCallback( [=](){
        this->operator()();
    });
}

HttpSession::~HttpSession()
{

}

void HttpSession::operator()()
{
    HTTP_CODE ret = NO_REQUEST;
    while( ret == NO_REQUEST )
    {
        ret = parseProcess();
        if ( ret != NO_REQUEST)
        {
            requestProcess(ret);
            m_tcsp->getInput().clear();
            m_tcsp->setInIdx(0);
            // m_tcsp->addToOutput( std::move( m_str ) );
            m_tcsp->send( std::move( m_str ) );
            
            return;
        }
        Coroutine::Yield();
    }
    // std::cout << ret << std::endl;
}

HttpSession::LINE_STATUS HttpSession::parseLine()
{
    // if ( m_tcsp.expired() )
    // {
    //     return LINE_BAD;
    // }

    // TcpConnectionSP tmp_tcsp = m_tcsp.lock();

/* 长时间读入缓冲区为空或没有新的数据到达，通过时间轮定时关闭*/    
    if ( m_tcsp->getInput().empty() || m_endIdx + 1 >= m_tcsp->getInput().size() )
    {
        return LINE_OPEN;
    }
    std::string str = m_tcsp->getInput();
// std::cout << __func__ << " : " << str << std::endl;
    int idx = str.find( CRLF, m_inIdx );
    if ( idx != str.npos )
    {
    /* 下一行的首部*/    
        m_endIdx = idx + CRLF.size();                   
        return LINE_OK;
    }
    else 
    {
        int idxr = str.find( CR, m_inIdx );
        int idxf = str.find( LF, m_inIdx );
    /* "\n"在"\r"前或有"\n"没有"\r"说明格式错误；否则，行不完整*/
        if ( ( idxr == str.npos && idxf != str.npos ) || idxr > idxf )
        {
            return LINE_BAD;
        }
        return LINE_OPEN;
    }
}

/**
 * @brief 请求行： HTTP方法+"\t"+URL+"\t"+HTTP/1.1
 **/ 
HttpSession::HTTP_CODE HttpSession::parseRequestLine()
{
    // if ( m_tcsp.expired() )
    // {
    //     return BAD_REQUEST;
    // }

    // TcpConnectionSP tmp_tcsp = m_tcsp.lock();
    std::string str = m_tcsp->getInput();

/* 获取http方法*/
    int idx = str.find( SP, m_inIdx );
    // std::cout << __func__ << "idx : " <<  idx << " startidx: " << m_inIdx << " endidx : " << m_endIdx << std::endl;
    /* 请求行空格位置错误，格式错误*/
    if ( idx == str.npos || idx >= m_endIdx - 2 )
    {
        return BAD_REQUEST;
    }   
    /* 仅支持get请求*/
    std::string method = str.substr( m_inIdx, idx - m_inIdx );
    if ( method == "GET" )
    {
        m_method = GET;
        // std::cout << method << std::endl;
    }
    else 
    {
        return BAD_REQUEST;
    }  
    m_inIdx = idx + 1;

/* 获取url*/
    idx = str.find( SP, m_inIdx );
    // std::cout << __func__ << "idx : " <<  idx << " startidx: " << m_inIdx << " endidx : " << m_endIdx << std::endl;
    if ( idx == str.npos || idx >= m_endIdx - 2 )
    {
        return BAD_REQUEST;
    }   

    m_url = str.substr( m_inIdx, idx - m_inIdx );
/* http版本*/
    m_inIdx = idx + 1;
    m_version = str.substr( m_inIdx, m_endIdx-m_inIdx-2 );
    /* 仅支持HTTP/1.1*/
    if (m_url.substr( 0, 7 ) != "http://" || m_version != "HTTP/1.1" )
    {
        // std::cout << m_url << m_url.size() << " " << m_version << m_version.size() << std::endl;
        return BAD_REQUEST;
    }
    // std::cout << m_version << std::endl;

/* 更新主状态机状态*/
    m_checkState = CHECK_STATE_HEADER;
/* 更新解析点*/    
    m_inIdx = m_endIdx;

    return NO_REQUEST;
}

/**
 * @brief HTTP请求头={ key : value \r\n}
*/
HttpSession::HTTP_CODE HttpSession::parseHeader()
{
    // if ( m_tcsp.expired() )
    // {
    //     return BAD_REQUEST;
    // }

    // TcpConnectionSP tmp_tcsp = m_tcsp.lock();
    std::string str = m_tcsp->getInput();

    int idx = m_inIdx;
/* 空行，请求头结束*/
    if ( str[idx] == '\0')
    {
        /* head方法与get类似，但服务器不会返回消息体，通常用来测试超链接的有效性、可用性和最近是否修改*/    
        if ( m_method == HEAD )
        {
            return GET_REQUEST;
        }
        
        if ( m_contentLen != 0)
        {
            m_checkState = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }

        m_inIdx = m_endIdx;
        return GET_REQUEST;
    }
    else if ( ( idx = str.find( "Connection:", idx ) ) != str.npos )
    {
        idx += 11;
        if ( str.substr( idx, 1) != SP )
        {
            return BAD_REQUEST;
        }

        idx += 1;
        if ( ( idx = str.find( "keep-alive", idx ) ) != str.npos
            && idx <= m_endIdx-12 )
        {
            m_linger = true;
        }
    }
    else if ( ( idx = str.find( "Content-length:", idx ) ) != str.npos
        && idx <= m_endIdx - 17 )
    {
        idx += 15;
        // if ( str.substr( idx, 1 ) != SP )
        // {
        //     return BAD_REQUEST;
        // }

        // idx += 1;

        m_contentLen = stoi( str.substr( idx, m_endIdx - idx - 2 ) );
    }
    else if  ( ( idx = str.find( "Host:", idx ) ) != str.npos
        && idx <= m_endIdx - 7 )
    {
        idx += 5;
        // if ( str.substr( idx, 1) != SP )
        // {
        //     return BAD_REQUEST;
        // }

        idx += 1; 
         m_host = str.substr( idx, m_endIdx - idx - 2 );
    }
    else 
    {
        std::cout << "unsupported header " << str.substr(m_inIdx, m_endIdx-m_inIdx-2 );
    }

    m_checkState = CHECK_STATE_CONTENT;
    m_inIdx = m_endIdx;

    return NO_REQUEST;
}

/**
 * @brief 请求体长度为contentlen
*/
HttpSession::HTTP_CODE HttpSession::parseContent()
{
    // if ( m_tcsp.expired() )
    // {
    //     return BAD_REQUEST;
    // }

    // TcpConnectionSP tmp_tcsp = m_tcsp.lock();
    std::string str = m_tcsp->getInput();

    if( str.size() - m_inIdx >= m_contentLen )
    {
        str.insert( m_inIdx + m_contentLen, "\0" );

        m_inIdx = m_inIdx + m_contentLen + 1;
        m_endIdx = m_inIdx;

        return GET_REQUEST;
    }

    return NO_REQUEST;
}

HttpSession::HTTP_CODE HttpSession::parseProcess()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    // if ( m_tcsp.expired() )
    // {
    //     return BAD_REQUEST;
    // }

    // TcpConnectionSP tmp_tcsp = m_tcsp.lock();
    std::string str = m_tcsp->getInput();
    if ( m_inIdx + 1 >= str.size() )
    {
        return NO_REQUEST;
    }
    while( ( ( m_checkState == CHECK_STATE_CONTENT ) && ( line_status == LINE_OK ) )
        || ( ( line_status = parseLine() ) == LINE_OK ) )
    {
        // std::cout << "line_status : " << line_status << std::endl;
        switch (m_checkState)
        {
        case CHECK_STATE_REQUESTLINE :
            {
                ret = parseRequestLine();
                if ( ret == BAD_REQUEST )
                {
                    // std::cout << "checkstatu : " << m_checkState << std::endl;
                    return ret;
                }
                break;
            }
        case CHECK_STATE_HEADER :
            {
                ret = parseHeader();
                if ( ret == BAD_REQUEST )
                {
                    // std::cout << "checkstatu : " << m_checkState << std::endl;
                    return ret;
                }
                if ( ret == GET_REQUEST )
                {
                    return confirmResource();
                }
                break;
            }
        case CHECK_STATE_CONTENT :
            {
                ret = parseContent();
                if ( ret == GET_REQUEST )
                {
                    return confirmResource();
                }
                line_status = LINE_OPEN;
                break;
            }
        default:
            {
                return INTERNAL_ERROR;
                break;
            }
        }
    }
    if ( line_status == LINE_BAD)
    {
        return BAD_REQUEST;
    }
    return NO_REQUEST;
}

HttpSession::HTTP_CODE HttpSession::confirmResource()
{
    m_file = doc_root + m_url;

    if ( stat( m_file.c_str(), &m_fileStat ) < 0 )
    {
        return NO_RESOURCE;
    }
/* 客户权限不足*/
    if ( !( m_fileStat.st_mode & S_IROTH ) )
    {
        return FORBIDDEN_REQUEST;
    }
/* 目标为一个文件*/
    if ( S_ISDIR( m_fileStat.st_mode ) )
    {
        return BAD_REQUEST;
    }

    return FILE_REQUEST;
}

void HttpSession::addStatusLine( int status, const std::string& title )
{
    m_str += "HTTP/1.1 " + std::to_string(status) + " " + title + CRLF;
}

void HttpSession::addHeader( int content_len )
{
    addContentLen( content_len );
    addLinger();
    addBlankLine();
}

void HttpSession::addContentLen( int content_len )
{
    m_str += "Content-Length: " + std::to_string( content_len ) + CRLF;
}

void HttpSession::addLinger()
{
    std::string label = m_linger == true ? "keep-alive" : "close";
    m_str += "Connection: " + label + CRLF;
}

void HttpSession::addBlankLine()
{
    m_str += CRLF;
}

void HttpSession::addContent( const std::string& content )
{
    m_str += content;
}

void HttpSession::requestProcess( HTTP_CODE ret )
{
    switch( ret )
    {
        case INTERNAL_ERROR :
        {
            addStatusLine( 500, error_500_title );
            addHeader( error_500_form.size() );
            addContent( error_500_form );

            break;
        }
        case BAD_REQUEST : 
        {
// std::cout << "bad reuqe" << std::endl;
            addStatusLine( 400, error_400_title );
            addHeader( error_400_form.size() );
            addContent( error_400_form );
            break;
        }
        case NO_RESOURCE :
        {
            addStatusLine( 404, error_404_title );
            addHeader( error_404_form.size() );
            addContent( error_404_form );
            break;
        }
        case FORBIDDEN_REQUEST :
        {
            addStatusLine( 403, error_403_title );
            addHeader( error_403_form.size() );
            addContent( error_403_form );
            break;
        }
        case FILE_REQUEST :
        {
            addStatusLine( 200, ok_200_title );
            if ( m_fileStat.st_size != 0 )
            {
                addHeader( m_fileStat.st_size );
                readFile( m_file );
            }
            else 
            {
                std::string ok_str = "<html><body></body></html>";
                addHeader( ok_str.size() );
                addContent( ok_str );
            }
        }
    }
}

void HttpSession::readFile( const std::string& filename )
{
    std::ifstream in( filename.c_str(), std::ios::in );
/* c++17支持*/    
    std::istreambuf_iterator<char> head(in), back;
    std::string res( head, back );
    in.close();
    addContent( res );
}


#define HSTEST
#ifdef HSTEST

int main()
{   
    EventLoop el;
    std::shared_ptr<TcpConnection> tcsp = std::make_shared<TcpConnection>(&el, 1);
    std::string str = "GET http://root/Mine/MyCoServer/index.html HTTP/1.1" +  CRLF;
    tcsp->addToInput( std::move( str ) );
    HttpSession hs( tcsp );
    Coroutine::Resume( hs.getMyCo() );
    tcsp->addToInput( "Host: www.wrox.com" + CRLF + "Content-Length: 10" + CRLF + "Connection: Keep-Alive" + CRLF + CRLF + "name=Professional%20Ajax&publisher=Wiley" );
    Coroutine::Resume( hs.getMyCo() );
    std::cout << tcsp->getOutput() << std::endl;
    tcsp->setDisconn( true );
    return 0;
}
#endif