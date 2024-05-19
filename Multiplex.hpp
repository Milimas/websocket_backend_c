#ifndef MULTIPLEX_HPP
#define MULTIPLEX_HPP

#include <iostream>
#include <vector>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <cstring>
#include <string>
#include <netdb.h>
#include <fcntl.h>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include "./SocketManager.hpp"
#include <fstream>
#include <ctime>

#define RESETTEXT  "\x1B[0m" // Set all colors back to normal.
#define FOREBLK  "\x1B[30m" // Black
#define FORERED  "\x1B[31m" // Red
#define FOREGRN  "\x1B[32m" // Green
#define FOREYEL  "\x1B[33m" // Yellow
#define FOREBLU  "\x1B[34m" // Blue
#define FOREMAG  "\x1B[35m" // Magenta
#define FORECYN  "\x1B[36m" // Cyan
#define FOREWHT  "\x1B[37m" // White

#define R_SIZE 1024 // Read Buffer Size
#define TIMEOUT 30

class Multiplex
{
public:
	typedef struct epoll_event epoll_event_t ;
	
private:
	static SOCKET 			server ;
	static SOCKET 			epollFD ;
    static epoll_event_t 	events[SOMAXCONN] ;

	Multiplex( void ) ;
	Multiplex( const Multiplex& rhs ) ;
	Multiplex& operator=( const Multiplex& rhs ) ;
	~Multiplex( void ) ;
public:
    static void start( void ) ;
    static void setup( void ) ;
} ;

std::ostream& operator<<( std::ostream& os, const Multiplex& server ) ;

#endif