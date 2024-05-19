#include "./SocketManager.hpp"

int SocketManager::epollFD = -1 ;

#include <iostream>

int SocketManager::createSocket( std::string host, std::string port, int ai_family, int ai_socktype, int ai_flags)
{
    struct addrinfo hints ;
    struct addrinfo *result, *rp ;
    SOCKET  sfd ;
    int     s ;
    int     yes = 1 ;

    bzero(&hints, sizeof(struct addrinfo)) ;
    hints.ai_family = ai_family ;
    hints.ai_socktype = ai_socktype ;
    hints.ai_flags = ai_flags ;

    s = getaddrinfo(host.c_str(), port.c_str(), &hints, &result) ;
    if (s != 0)
    {
        throw std::runtime_error("getaddrinfo") ;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol) ;
        if (!ISVALIDSOCKET(sfd))
            continue ;
        if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &yes, sizeof(int)) == -1)
        {
            perror("setsockopt()") ;
        }
        s = bind(sfd, rp->ai_addr, rp->ai_addrlen) ;
        if (s == 0)
        {
            /* We managed to bind successfully! */
            break ;
        }
        close(sfd) ;
    }
    freeaddrinfo(result) ;
    if (rp == NULL)
        throw std::runtime_error("Could not bind " + host + ":" + port) ;
    return sfd;
}

int SocketManager::makeSocketNonBlocking( int sfd )
{
    int s ;

    s = fcntl(sfd, F_SETFL, O_NONBLOCK, FD_CLOEXEC);
    if (s == -1)
    {
        perror("fcntl") ;
        throw std::runtime_error("Could not set socket flags") ;
        return -1 ;
    }
    return 0 ;
}

int SocketManager::startListening( int sfd )
{
    int s ;
    s = listen (sfd, SOMAXCONN);
    if (s == -1)
    {
        perror ("listen");
        throw std::runtime_error("Could not listen on socket") ;
        return -1 ;
    }
    return 0 ;
}

int SocketManager::createEpoll()
{
    epollFD = epoll_create1 (0);
    if (epollFD == -1)
    {
        perror ("epoll_create");
        throw std::runtime_error("Could not create epoll") ;
        return -1 ;
    }

    return epollFD ;
}

int SocketManager::epollCtlSocket( int sfd, int op, uint32_t _events )
{
    int s ;
    struct epoll_event event;

    event.data.fd = sfd;
    event.events = _events ;
    signal(SIGPIPE, SIG_IGN) ;
    s = epoll_ctl (epollFD, op, sfd, &event);
    if (s == -1)
    {
        perror ("epoll_ctl");
        throw std::runtime_error("Could not ctl epoll") ;
        return -1 ;
    }
    return 0 ;
}