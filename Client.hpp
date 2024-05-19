#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <openssl/sha.h>
#include "base64.h"
#include <map>

class Client
{
public:
    SOCKET sock ;
    std::map<std::string, std::string> headers ;
    enum State { REQUESTLINE = 0, HEADER, CHECK, UPGRADE, UPGRADED, DONE, ERROR } state ;
    std::string requestBuffer ;

    Client( SOCKET s ) : sock(s)
    {
        state = REQUESTLINE ;
        requestBuffer = "";
    }
    ~Client( void )
    {
        close(sock) ;
    }
    Client( const Client& rhs )
    {
        sock = rhs.sock ;
        headers = rhs.headers ;
        state = rhs.state ;
        requestBuffer = rhs.requestBuffer ;
    }
    const Client& operator=( const Client& rhs )
    {
        sock = rhs.sock ;
        headers = rhs.headers ;
        state = rhs.state ;
        requestBuffer = rhs.requestBuffer ;
        return *this ;
    }

    std::string takeHeader( std::string& buffer )
    {
        size_t pos = buffer.find("\r\n");

        if (pos == std::string::npos)
        {
            return "";
        }

        std::string header = buffer.substr(0, pos);

        buffer = buffer.substr(pos + 2);
        return header ;
    }

    std::string trim( std::string& str )
    {
        size_t first = str.find_first_not_of(" \t\r\n\v");
        if (first == std::string::npos)
        {
            return "";
        }
        size_t last = str.find_last_not_of(" \t\r\n\v");
        return str.substr(first, (last - first + 1));
    }

    const std::string getWord( std::string& buffer )
    {
        size_t pos = buffer.find_first_of(" \t\r\n\v");
        std::string word = buffer.substr(0, pos);
        buffer = buffer.substr(pos + 1);
        return trim(word) ;
    }


    std::string tolowercase( std::string& str )
    {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str ;
    }

    std::pair<std::string, std::string> splitHeader( std::string& buffer )
    {
        size_t pos = buffer.find(":");
        std::string header = buffer.substr(0, pos);
        std::string value = buffer.substr(pos + 2);
        buffer = buffer.substr(pos + 2);
        header = tolowercase(header);
        return std::make_pair(trim(header), trim(value)) ;
    }

    void parseRequestLine( void )
    {
        std::string line = takeHeader(requestBuffer);
        headers["method"] = getWord(line);
        headers["path"] = getWord(line);
        headers["version"] = getWord(line);
        state = HEADER ;
    }

    void parseHeaders( void )
    {
        std::string line ;
        if (requestBuffer.find("\r\n\r\n") != std::string::npos)
        {
            state = CHECK ;
            std::cout << "CHECK" << std::endl ;
        }
        while ((line = takeHeader(requestBuffer)) != "")
        {
            headers.insert(splitHeader(line));
        }
    }

    void printHeaders( void )
    {
        for (std::map<std::string, std::string>::iterator it = headers.begin(); it != headers.end(); it++)
        {
            std::cout << it->first << ": " << it->second << std::endl;
        }
    }

    void checkRequestLine( void )
    {
        if (headers["method"] != "GET")
        {
            throw std::runtime_error("Method not supported") ;
        }
        if (headers["path"] != "/")
        {
            throw std::runtime_error("Path not supported") ;
        }
        if (headers["version"] != "HTTP/1.1")
        {
            throw std::runtime_error("Version not supported") ;
        }
    }

    void checkHeaders( void )
    {
        checkRequestLine();
        if (headers["connection"] != "Upgrade")
        {
            throw std::runtime_error("Connection not supported") ;
        }
        if (headers["upgrade"] != "websocket")
        {
            throw std::runtime_error("Upgrade not supported") ;
        }
        if (headers["sec-websocket-key"] == "")
        {
            std::cout << "Sec-Websocket-Key: " << headers["sec-websocket-key"] << std::endl;
            throw std::runtime_error("Sec-WebSocket-Key not provided") ;
        }
        if (headers["sec-websocket-version"] != "13")
        {
            std::cout << "Sec-WebSocket-Version: " << headers["sec-websocket-version"] << std::endl;
            throw std::runtime_error("Sec-WebSocket-Version not supported") ;
        }
        state = UPGRADE ;
    }

    std::string decodeWebsocketDataFrame( std::string bytes )
    {
        bool fin = (bytes[0] & 0b10000000) != 0 ;
        bool mask = (bytes[1] & 0b10000000) != 0 ;
        int opcode = bytes[0] & 0b00001111, offset = 2;

        if (opcode != 1)
        {
            throw std::runtime_error("Opcode not supported") ;
        }

        if (!fin)
        {
            throw std::runtime_error("Continuation frames not supported") ;
        }

        unsigned long msglen = bytes[1] & 0b01111111;
        if (msglen == 126)
        {
            msglen = bytes[2] << 8 | bytes[3];
            offset = 4;
        }
        else if (msglen == 127)
        {
            msglen = (bytes[2] << 24) | (bytes[3] << 16) | (bytes[4] << 8) | bytes[5];
            offset = 6;
        }

        if (mask)
        {
            std::string decoded(msglen, 0);
            std::string masks = bytes.substr(offset, 4);
            offset += 4;
            for (unsigned long i = 0; i < msglen; i++)
            {
                decoded[i] = bytes[offset + i] ^ masks[i % 4];
            }
            std::cout << "Decoded: " << decoded << std::endl;
            requestBuffer = "" ;
            return decoded;
        }
        else
        {
            std::string decoded(msglen, 0);
            for (unsigned long i = 0; i < msglen; i++)
            {
                decoded[i] = bytes[offset + i];
            }
            requestBuffer = "" ;
            return decoded;
        }
        state = UPGRADED ;
    }

    void encodeWebsocketDataFrame( std::string data )
    {
        std::string encoded = "";
        
        encoded += "\x81";
        if (data.size() <= 125)
            encoded += (char) data.size();
        else if (data.size() <= 65535)
        {
            encoded += "\x82";
            encoded += (char) ((data.size() >> 8) & 0xFF);
            encoded += (char) (data.size() & 0xFF);
        }

        encoded += data;

        int bytesSent = send(sock, encoded.c_str(), encoded.size(), 0);
        if (bytesSent < 0)
            throw std::runtime_error("Error sending to socket") ;
    }

    void parseRequest( void )
    {
        char buffer[1024];
        int bytesReceived;
        bytesReceived = recv(sock, buffer, 1024, 0);
        if (bytesReceived == 0)
        {
            return ;
        }
        if (bytesReceived < 0)
            throw std::runtime_error("Error receiving from socket") ;

        requestBuffer += std::string(buffer, bytesReceived);
        switch (state)
        {
            case REQUESTLINE:
                parseRequestLine();
            case HEADER:
                parseHeaders();
                break ;
            case UPGRADED:
                decodeWebsocketDataFrame(requestBuffer) ;
                encodeWebsocketDataFrame("Hello World!") ;
                break ;
            default:
                break ;
        }
    }

    std::string generateResponse( void )
    {
        std::string key = headers["sec-websocket-key"];
        std::string key_plus = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        unsigned char digest[SHA_DIGEST_LENGTH];
        SHA1((const unsigned char*)key_plus.c_str(), key_plus.size(), digest);
        std::string accept = base64_encode(digest, SHA_DIGEST_LENGTH);
        return accept;
    }

    void sendUpgrade( void )
    {
        requestBuffer = "";
        std::string response = "HTTP/1.1 101 Switching Protocols\r\n";
        response += "Connection: Upgrade\r\n";
        response += "Upgrade: websocket\r\n";
        response += "Sec-WebSocket-Accept: " + generateResponse() + "\r\n\r\n";
        send(sock, response.c_str(), response.size(), 0);
        std::cout << "Sent response" << std::endl;
        state = UPGRADED ;
    }

    void sendError( void )
    {
        std::string response = "HTTP/1.1 400 Bad Request\r\n";
        response += "Connection: Upgrade\r\n";
        response += "Upgrade: websocket\r\n";
        response += "Sec-WebSocket-Accept: " + generateResponse() + "\r\n\r\n";
        send(sock, response.c_str(), response.size(), 0);
        state = DONE ;
    }

};


#endif