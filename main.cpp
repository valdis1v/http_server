#include <iostream>
#include <winsock2.h>
#include <Windows.h>
#include <thread>
#include <vector>
#include <WS2tcpip.h>
#include <string>
#include <sstream>
#include <map>
#include <functional>

SOCKET LISTEN_ON; 
struct sockaddr_in SERVER; 
hostent* localHost; 
char* localIP; 
using std::string; 
#define CONNECTED 1
#define DISCONNECTED 2
#define INVALID 3


class HTTP_CLIENT
{

    public:
        std::function<void(char* request, SOCKET Client)> GetFallback   ; 
        std::function<void(char* request, SOCKET Client)> PostFallback  ; 
        std::function<void(char* request, SOCKET Client)> DeleteFallback; 
        HTTP_CLIENT(int _port, std::function<void( char* request, SOCKET Client)> _GetFallback, std::function<void(char* request, SOCKET Client)> _PostFallback, std::function<void(char* request, SOCKET Client)> _DeleteFallback)
        {
            WSADATA wsa ;
            WSAStartup(MAKEWORD(2,2), &wsa);

            GetFallback = _GetFallback;
            PostFallback = _PostFallback;
            DeleteFallback = _DeleteFallback; 

            port = _port; 
          
            soc_init();
            soc_ready();
        }

        void Listen()
        {
            while(true)
            {
                while (true)
                {
                    SOCKET CLIENT = accept(LISTEN_ON, NULL, NULL);
                    std::thread connection([this, CLIENT]() { recieveAndFallBack(CLIENT); });
                    connection.detach();
                }
            }
        }



    private:

        SOCKET TCPSOC; 
        int port; 

        void recieveAndFallBack(SOCKET ClientSocket)
        {
            char buffer[1024];
            int bytesReceived = recv(ClientSocket, buffer, sizeof(buffer) - 1, 0); // Empfange Daten
            if (bytesReceived > 0) {
                buffer[bytesReceived] = '\0'; // wichtig
                std::string request(buffer); 
                std::cout << request << std::endl;

                std::istringstream stream(request);
                std::string method, path, version;
                stream >> method >> path >> version;

                // request.c_str() direkt in char* kopieren
                char* temp = new char[request.size() + 1];
                strcpy(temp, request.c_str());

                if (method == "GET") {
                    GetFallback(temp, ClientSocket);
                } else if (method == "POST") {
                    PostFallback(temp, ClientSocket);
                } else if (method == "DELETE") {
                    DeleteFallback(temp, ClientSocket);
                }

                delete[] temp;
            }
            closesocket(ClientSocket);   
        }


        void soc_ready()
        {
            int x = listen(LISTEN_ON,SOMAXCONN);
            if(x == SOCKET_ERROR)
            {
                exit(-1); 
            }

        }

        void soc_init()
        {
            LISTEN_ON = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); 
            if (LISTEN_ON == INVALID_SOCKET) { std::cerr << "socket failed\n"; exit(1); }
            addrinfo hints = {}, *res;
            hints.ai_family = AF_INET; // IPv4
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;
            std::cout << "SOCKET ERSTELLT" << std::endl;
            if (getaddrinfo("localhost", nullptr, &hints, &res) != 0) {
                std::cerr << "Failed to resolve localhost" << std::endl;
                exit(-1);
            }
                    sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(res->ai_addr);
            localIP = inet_ntoa(addr->sin_addr);
            freeaddrinfo(res);
            SERVER.sin_addr.s_addr = inet_addr(localIP);
            SERVER.sin_family = AF_INET; //TCP
            SERVER.sin_port = htons(port); // localhost:4200
            bind(LISTEN_ON, (SOCKADDR*)&SERVER, sizeof(SERVER));
        }
};

class HTTP_RESPONSE
{
    #define OK "200 OK"
    #define NOTFOUND "404 Not Found"
    #define FORBIDDEN "403 Forbidden"
    #define ISERVERE "500 Internal Server Error"
    #define HTTP_VSTANDART "HTTP/ 1.1"

    public:
        string CT;
        string CC;
        string method; 
        string HTTPV;
        string CODE;  
        string resBuff; 

        HTTP_RESPONSE(SOCKET RESPONDER, string _Ct, string _Cc, string HttpVersion, string STATUS_CODE)
        {
            CT = _Ct;
            CC = _Cc; 
            HTTPV = HttpVersion;
            CODE = STATUS_CODE; 
        }

        char* build_res()
        {
            int CL = resBuff.size(); 
            string head = HTTPV + " " + CODE + "\r\n";
            string server = std::string("Server: Test") + "\r\n";
            string cl = std::string("Content-Length: ") + std::to_string(CL) + "\r\n";
            string ct = "Content-Type: " + CT + "\r\n";
            string cc = "Cache-Control: " + CC + "\r\n";
            string co = std::string("Connection: close") + "\r\n";
            string body = "\r\n" + std::string(resBuff);

            std::string ready = head + server + cl + ct + cc + co + body;

            char* response = new char[ready.size() + 1];
            strcpy(response, ready.c_str()); 

            return response; // funkt nicht bei chrome => test curl http:localhost:XXXX ;
        }

        // Socket Abhören lassen

};


void sendResponse(SOCKET CLIENT, string httpV)
{
    HTTP_RESPONSE http(CLIENT, "text/html", "no-store", httpV, OK);
    http.resBuff = "<html><body><h1>Hello, World!</h1></body></html>"; 
    char* response = http.build_res(); 

    std::cout << response << std::endl;
    send(CLIENT, response , strlen(response), 0 );
    delete response; 
}

void verarbeitungHTTP(std::string buffer, SOCKET responseTo)
{
    int loc = buffer.find("HTTP/");
    std::string httpVersion = buffer.substr(loc, 8); // wichtig für antwort
    sendResponse(responseTo, httpVersion);
}


void recieve(SOCKET CLIENT)
{
    char buffer[1024];
    int bytesReceived = recv(CLIENT, buffer, sizeof(buffer) - 1, 0); // Empfange Daten
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0'; // wichtig
        std::string request(buffer); 
        std::cout << request << std::endl;
        verarbeitungHTTP(request, CLIENT);   
    }
    closesocket(CLIENT);
}


int main ()
{
    HTTP_CLIENT client( 4200,
        [](char* request, SOCKET Client) { sendResponse(Client, HTTP_VSTANDART); },
        [](char* request, SOCKET Client) { std::cout << "POST request received: " << request << std::endl; },
        [](char* request, SOCKET Client) { std::cout << "DELETE request received: " << request << std::endl; }
    );
    client.Listen();
    return 0 ;
}
