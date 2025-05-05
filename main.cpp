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
#include <optional>
#include <WinBase.h>
#include <filesystem>
#include <fstream>

SOCKET LISTEN_ON; 
struct sockaddr_in SERVER; 
hostent* localHost; 
char* localIP; 
using std::string; 
#define CONNECTED 1
#define DISCONNECTED 2
#define INVALID 3

#define GET 1
#define POST 2
#define _DELETE 3
#define PUT 4

class HTTP_CLIENT_ROUTES
{
    public:
        struct route {
            // Muss strikt so aussehen "/api/api/api/api/"
            string route;
            std::function<void(char*, SOCKET)> handler;
        };
        std::map<string, std::function<void(char* request, SOCKET Client)>> GET_ROUTES; 
        std::map<string, std::function<void(char* request, SOCKET Client)>> POST_ROUTES; 
        std::map<string, std::function<void(char* request, SOCKET Client)>> DELETE_ROUTES; 
        std::map<string, std::function<void(char* request, SOCKET Client)>> PUT_ROUTES; 
        std::function<void()> errorhandler; 


        HTTP_CLIENT_ROUTES(route _Get, route _Post, route _Delete, route _Put, std::function<void()> _errorhandler)
        {
            GET_ROUTES.insert(std::make_pair(_Get.route, _Get.handler));
            POST_ROUTES.insert(std::make_pair(_Post.route, _Post.handler));
            DELETE_ROUTES.insert(std::make_pair(_Delete.route, _Delete.handler));
            PUT_ROUTES.insert(std::make_pair(_Put.route, _Put.handler));
            errorhandler = _errorhandler;
            initClient();  
        }
                    // GET - PUT defined oben
        void registerRoute(int requestType, route newRoute)
        {
            switch(requestType)
            {
                case GET:
                    GET_ROUTES.insert(std::make_pair(newRoute.route, newRoute.handler));
                    break;
                case POST:
                    POST_ROUTES.insert(std::make_pair(newRoute.route, newRoute.handler));
                    break;
                case _DELETE:
                    DELETE_ROUTES.insert(std::make_pair(newRoute.route, newRoute.handler));
                    break;
                case PUT:
                    PUT_ROUTES.insert(std::make_pair(newRoute.route, newRoute.handler));
                    break;
                default:
                    errorhandler();
                    break; 
            }
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

        SOCKET LISTEN_ON;
        void recieveAndFallBack(SOCKET Client)
        {
            char buffer[1024];
            int bytesReceived = recv(Client, buffer, sizeof(buffer) - 1, 0); // Empfange Daten
            if (bytesReceived > 0) {
                buffer[bytesReceived] = '\0'; // Null-Terminierung hinzufügen
                std::string request(buffer); 
                std::cout << "Request:\n" << request << std::endl;

                // HTTP-Request parsen
                std::istringstream stream(request);
                std::string method, path, version;
                stream >> method >> path >> version;

                // Route extrahieren
                std::cout << "HTTP Method: " << method << std::endl;
                std::cout << "Requested Route: " << path << std::endl;
                std::cout << "HTTP Version: " << version << std::endl;

                // Weiterverarbeitung basierend auf der Methode
                if (method == "GET") {
                    callRegisteredRoute(GET, path, buffer, Client);
                } else if (method == "POST") {
                    callRegisteredRoute(POST, path, buffer, Client);
                } else if (method == "PUT") {
                    callRegisteredRoute(PUT, path, buffer, Client);
                } else if (method == "DELETE") {
                    callRegisteredRoute(_DELETE, path, buffer, Client);
                } else {
                    errorhandler();
                }
            }
            closesocket(Client);
        }

        void callRegisteredRoute(int method, const string& route, char* request, SOCKET Client)
        {
            switch (method)
            {
                case GET:
                {
                    auto it = GET_ROUTES.find(route);
                    if (it != GET_ROUTES.end()) {
                        it->second(request, Client);
                    } else {
                        std::cerr << "GET route not found: " << route << std::endl;
                        errorhandler();
                    }
                    break;
                }
                case POST:
                {
                    auto it = POST_ROUTES.find(route);
                    if (it != POST_ROUTES.end()) {
                        it->second(request, Client); 
                    } else {
                        std::cerr << "POST route not found: " << route << std::endl;
                        errorhandler();
                    }
                    break;
                }
                case _DELETE:
                {
                    auto it = DELETE_ROUTES.find(route);
                    if (it != DELETE_ROUTES.end()) {
                        it->second(request, Client);
                    } else {
                        std::cerr << "DELETE route not found: " << route << std::endl;
                        errorhandler();
                    }
                    break;
                }
                case PUT:
                {
                    auto it = PUT_ROUTES.find(route);
                    if (it != PUT_ROUTES.end()) {
                        it->second(request, Client);
                    } else {
                        std::cerr << "PUT route not found: " << route << std::endl;
                        errorhandler();
                    }
                    break;
                }
                default:
                    std::cerr << "Unsupported HTTP method: " << method << std::endl;
                    errorhandler();
                    break;
            }
        }
        
        void initClient(int _port = 4000)
        {
            WSADATA wsa ;
            WSAStartup(MAKEWORD(2,2), &wsa);
            soc_init(_port);
            soc_ready();
        }

        void soc_init(int port = 4000)
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

        void soc_ready()
        {
            int x = listen(LISTEN_ON,SOMAXCONN);
            if(x == SOCKET_ERROR)
            {
                exit(-1); 
            }

        }




};

class HTTP_RESPONSE
{
    #define OK "200 OK"
    #define NOTFOUND "404 Not Found"
    #define FORBIDDEN "403 Forbidden"
    #define ISERVERE "500 Internal Server Error"
    #define HTTP_VSTANDART "HTTP/1.1"

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




void sendResponse(SOCKET CLIENT, const string& httpV, const string& body, const string& contentType = "text/html; charset=utf-8")
{
    HTTP_RESPONSE http(CLIENT, contentType, "no-store", httpV, OK);
    http.resBuff = body;
    char* response = http.build_res(); 

    std::cout << response << std::endl;
    send(CLIENT, response , strlen(response), 0 );
    delete[] response; // korrekt: delete[] statt delete
}




class JSON_RAW
{
    public:
        string data ;
        JSON_RAW(std::vector<string> keys, std::vector<string> values)
        {
            this->data = buildJson(keys, values);
        }

    private:
        
        string buildJson(std::vector<string> keys, std::vector<string> values)
        {
            if (std::size(keys) != std::size(values))
            {
                throw E_INVALIDARG;
            };
            std::string temp; 
            temp.append("{\n");
            for (int i = 0; i < std::size(values); i++)
            {
                temp.append("  \"" + keys[i] + "\": \"" + values[i] + "\"");
                if (i != keys.size() - 1) temp.append(",\n"); else temp.append("\n");
            }
            temp.append("}\n");


            return temp; 
        }
};

class WEBSERVE
{
    public:

        HTTP_CLIENT_ROUTES* server; 
        WEBSERVE(HTTP_CLIENT_ROUTES* _server)
        : server(_server)
        {
            SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, FALSE };
            if (GetFileAttributesA("static") == INVALID_FILE_ATTRIBUTES) {
                if (!CreateDirectoryA("static", &sa)) {
                    std::cerr << "Failed to create 'static' directory. Error: " << GetLastError() << std::endl;
                }
            }

            serveFiles();
        }
    
    private:

        void serveFiles()
        {
            std::string path = "./static";
            for (const auto & entry : std::filesystem::directory_iterator(path))
            {
                if(entry.path().extension() == ".html")
                {
                    string route = "/" + entry.path().filename().string();
                    string data = readHTMLFile(entry.path().string());
                    HTTP_CLIENT_ROUTES::route GetHTML = { route , [data](char* request, SOCKET Client) {
                        sendResponse(Client, HTTP_VSTANDART, data);
                    }};
                    server->registerRoute(GET, GetHTML);
                }
            }
        }

        std::string readHTMLFile(string path)
        {
            std::ifstream file(path);
            if (!file.is_open())
            {
            std::cerr << "Failed to open file: " << path << std::endl;
                return "Notfound";
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            file.close();

            std::string content = buffer.str();
            return content; 
        }
};


int main ()
{
    JSON_RAW data(std::vector<string>{"Name", "Alter"}, std::vector<string>{"Paul", "18"});
    HTTP_CLIENT_ROUTES::route Get = {"/api/hello", [](char* request, SOCKET Client)
    {
        sendResponse(Client, HTTP_VSTANDART, "Hello"); 
    }};
    HTTP_CLIENT_ROUTES::route Get2 = {"/api/moin", [](char* request, SOCKET Client)
    {
        sendResponse(Client, HTTP_VSTANDART, "Moin"); 
    }};
    HTTP_CLIENT_ROUTES::route Post = {"/", [](char* request, SOCKET Client){ }};
    HTTP_CLIENT_ROUTES::route Put = {"/", [](char* request, SOCKET Client){ }};
    HTTP_CLIENT_ROUTES::route Delete = {"/", [](char* request, SOCKET Client){ }};
    std::function<void()> Errorh = []() {}; 
    HTTP_CLIENT_ROUTES client(Get,Post,Delete,Put,Errorh); 
    client.registerRoute(GET, Get2);

    WEBSERVE wclient(&client);

    client.Listen();
    for (const auto & route : client.GET_ROUTES)
    {
        std::cout << route.first;
    }
    return 0 ;
}
