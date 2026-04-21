#include <event2/event.h>
#include <event2/listener.h>
#include <event2/util.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>

struct ListenerContext{
    event_base* base;
};

void on_accept(struct evconnlistener* listener,
                evutil_socket_t fd,
                struct sockaddr* addr,
                int socklen,
                void* arg){
    
    (void)listener;
    (void)socklen;
    (void)arg;      // 把没用到的参数显式标记掉

    char ip[INET6_ADDRSTRLEN] = {0};
    int port = 0;   // 准备两个变量，用来保存客户端地址信息：

    // 判断客户端到底是 IPv4 还是 IPv6，然后分别解析：
    if(addr->sa_family == AF_INET){
        // 把“通用地址指针” sockaddr*，当成“IPv4 专用地址指针” sockaddr_in* 来看
        // 为什么用reinterpret_cast 因为这是在做指针类型重解释。
        // 这不是普通的“父子类转换”，也不是数值转换，而是：把一块内存地址按另一种结构类型来解释。
        auto* sin = reinterpret_cast<sockaddr_in*>(addr);
        inet_ntop(AF_INET, &(sin->sin_addr), ip, sizeof(ip));   // 把二进制网络地址转成可读字符串
        port = ntohs(sin->sin_port);    // 把网络字节序端口转成主机字节序整数
    }
    else if(addr->sa_family == AF_INET6){
        auto* sin6 = reinterpret_cast<sockaddr_in6*>(addr);
        inet_ntop(AF_INET6, &(sin6->sin6_addr), ip, sizeof(ip));
        port = ntohs(sin6->sin6_port);
    }
    else{
        std::strncpy(ip, "unknown-family", sizeof(ip) - 1);
    }

    std::cout << "[accept] new client from " << ip << ":" << port << std::endl;
    
    evutil_closesocket(fd);
}

void on_listener_error(struct evconnlistener* listener, void* arg){
    auto* ctx = static_cast<ListenerContext*>(arg);
    event_base* base = ctx->base;

    int err = EVUTIL_SOCKET_ERROR();
    std::cerr << "[listener] error: " << evutil_socket_error_to_string(err) << std::endl;

    if(listener){
        evconnlistener_free(listener);
    }

    event_base_loopbreak(base);
}

int main() {
    event_base* base = event_base_new();
    if(!base){
        std::cerr << "failed to create event_base" << std::endl;
        return 1;
    }

    std::cout << "event backend method: " << event_base_get_method(base) << std::endl;

    ListenerContext ctx;
    ctx.base = base;

    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(0);
    sin.sin_port = htons(5555);

    evconnlistener* listener = evconnlistener_new_bind(
        base,
        on_accept,
        &ctx,
        LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
        -1,
        reinterpret_cast<sockaddr*>(&sin),
        sizeof(sin)
    );

    if(!listener){
        std::cerr << "failed to create evconnlistener" << std::endl;
        event_base_free(base);
        return 1;
    }
    
    evconnlistener_set_error_cb(listener, on_listener_error);

    std::cout << "listening on 0.0.0.0:5555 ..." << std::endl;
    std::cout << "use another terminal: nc 127.0.0.1 5555" << std::endl;

    event_base_dispatch(base);

    evconnlistener_free(listener);
    event_base_free(base);

    return 0;
}