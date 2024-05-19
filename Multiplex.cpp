#include "./Multiplex.hpp"
#include "./Client.hpp"

SOCKET Multiplex::epollFD;
SOCKET Multiplex::server;
Multiplex::epoll_event_t Multiplex::events[SOMAXCONN] = {};

void Multiplex::setup( void )
{
    epollFD = SocketManager::createEpoll();
    server = SocketManager::createSocket("127.0.0.1", "8080", AF_INET, SOCK_STREAM, AI_PASSIVE);
    SocketManager::makeSocketNonBlocking(server);
    SocketManager::startListening(server);
    SocketManager::epollCtlSocket(server, EPOLL_CTL_ADD);
}

void Multiplex::start(void)
{
    std::map<SOCKET, Client*> clients;
    while (1)
    {
        int eventCount = 0;
        eventCount = epoll_wait(epollFD, events, SOMAXCONN, -1); // Waiting for new event to occur
        for (int i = 0; i < eventCount; i++)
        {
            int eFD = events[i].data.fd ;
            // if (events[i].events & EPOLLHUP)
            // {
            //     std::cout << "EPOLLHUP" << std::endl ;
            //     close(eFD) ;
            //     continue;
            // }
            if (server == eFD) // Check if socket belong to a server
            {
                struct sockaddr in_addr;
                socklen_t in_len;
                int infd;

                in_len = sizeof(in_addr);
                infd = accept(eFD, &in_addr, &in_len); // Accept connection
                if (!ISVALIDSOCKET(infd))
                {
                    if ((errno == EAGAIN) ||
                        (errno == EWOULDBLOCK))
                    {
                        /* We have processed all incoming
                            connections. */
                        continue;
                    }
                    else
                        continue;
                }
                SocketManager::makeSocketNonBlocking(infd);
                SocketManager::epollCtlSocket(infd, EPOLL_CTL_ADD);

                std::cout << "New client: " << infd << std::endl ;
                clients[infd] = new Client(infd);

                // find client in map that doesn't have peer and peer both
                for (std::map<SOCKET, Client*>::iterator it = clients.begin(); it != clients.end(); it++)
                {
                    if (it->second != clients[infd] && it->second->peer == NULL)
                    {
                        it->second->peer = clients[infd];
                        clients[infd]->peer = it->second;
                        break;
                    }
                }

                continue;
            }
            else if (events[i].events & EPOLLIN)   // check if we have EPOLLIN (connection socket ready to read)
            {
                try
                {
                    clients[eFD]->parseRequest();
                }
                catch (std::runtime_error& e)
                {
                    if (events[i].events & EPOLLOUT)
                    {
                        clients[eFD]->state = Client::ERROR;
                        clients[eFD]->sendError();
                    }
                    std::cout << e.what() << std::endl;
                    delete clients[eFD];
                    continue;
                }
            }
            if (events[i].events & EPOLLOUT)  // check if we have EPOLLOUT (connection socket ready to write)
            {
                try 
                {
                    if (clients[eFD]->state == Client::CHECK)
                        clients[eFD]->checkHeaders();
                }
                catch (std::runtime_error& e)
                {
                    std::cout << e.what() << std::endl;
                    delete clients[eFD];
                    continue;
                }
                if (clients[eFD]->state == Client::UPGRADE)
                    clients[eFD]->sendUpgrade();
                if (clients[eFD]->state == Client::DONE)
                {
                    delete clients[eFD];
                    continue;
                }
            }
        }
    }

    for (std::map<SOCKET, Client*>::iterator it = clients.begin(); it != clients.end(); it++)
    {
        delete it->second;
    }
}