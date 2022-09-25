/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-23 10:08
#
# Filename: main.cpp
#
# Description: 
#
=============================================================================*/

#include "HttpServer.h"
#include <signal.h>

int main()
{
    ::signal( SIGPIPE, SIG_IGN );

    EventLoop el;

    HttpServer es( &el, 2 );
    el.loop();

    return 0;
}