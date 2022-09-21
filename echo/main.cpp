/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-19 06:47
#
# Filename: main.cpp
#
# Description: 
#
=============================================================================*/

#include "EchoServer.h"
#include <signal.h>

int main()
{
    ::signal( SIGPIPE, SIG_IGN );

    EventLoop el;

    EchoServer es( &el, 2 );
    es.start();
    el.loop();

    return 0;
}