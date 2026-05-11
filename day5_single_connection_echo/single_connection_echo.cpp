#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/util.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>

struct ServerContext {
    event_base* base;
};

void on_bev_read(struct bufferevent* bev, void* ctx){
    (void)ctx;
    // 1. 获取输入缓冲区（存放客户端发来的数据）
    evbuffer* input = bufferevent_get_input(bev);

    // 2. 获取输出缓冲区（存放要发送给客户端的数据）
    evbuffer* output = bufferevent_get_output(bev);

    // 3. 获取收到的数据长度
    const size_t input_len = evbuffer_get_length(input);

    // 4. 打印日志：收到多少字节，准备回显
    std::cout << "[read_cb] received " << input_len << " bytes, echo back" << std::endl;

    // 5. 核心功能：把输入缓冲区的所有数据 搬运到输出缓冲区
    // libevent 会自动把输出缓冲区的数据发送给客户端
    evbuffer_add_buffer(output, input);
}

void on_bev_write(struct bufferevent* bev, void* ctx){
    (void)bev;
    (void)ctx;

    std::cout << "[write_cb] output buffer drained" << std::endl;
}


/*
监听并处理socket连接的所有状态变化和异常，打印日志提示原因
最后自动释放连接资源，避免内存泄漏
*/
// bufferevent 事件回调：连接建立、断开、出错、超时都会触发
void on_bev_event(struct bufferevent* bev, short events, void* ctx){
    (void)ctx;

    if(events & BEV_EVENT_CONNECTED){   // 连接成功事件（客户端 / 主动连接用）
        std::cout << "[event_cb] connection established" << std::endl;
        return;
    }

    if(events & BEV_EVENT_EOF){         // 连接断开（客户端正常关闭）
        std::cout << "[event_cb] client closed connection" << std::endl;
    }else if(events & BEV_EVENT_ERROR){ // 捕获网络异常：连接被重置、端口不可达、绑定失败等
        int err = EVUTIL_SOCKET_ERROR();
        std::cerr << "[event_cb] socket error: "
                  << evutil_socket_error_to_string(err) << std::endl;
    }else if(events & BEV_EVENT_TIMEOUT){   // 连接超时
        std::cerr << "[event_cb] connection timeout" << std::endl;
    }else{                                  // 其他未知事件
        std::cout << "[event_cb] other event: " << events << std::endl;
    }

    // 核心收尾动作：释放资源
    bufferevent_free(bev);
}

void on_accept(struct evconnlistener* listener, evutil_socket_t fd, struct sockaddr* addr,
                int socklen, void* arg){
    (void)listener;
    (void)socklen;

    auto* server_ctx = static_cast<ServerContext*>(arg);
    event_base* base = server_ctx->base;

    char ip[INET6_ADDRSTRLEN] = {0};
    int port = 0;

    if (addr->sa_family == AF_INET) {
    auto* sin = reinterpret_cast<sockaddr_in*>(addr);
    inet_ntop(AF_INET, &(sin->sin_addr), ip, sizeof(ip));
    port = ntohs(sin->sin_port);
    } else if (addr->sa_family == AF_INET6) {
        auto* sin6 = reinterpret_cast<sockaddr_in6*>(addr);
        inet_ntop(AF_INET6, &(sin6->sin6_addr), ip, sizeof(ip));
        port = ntohs(sin6->sin6_port);
    } else {
        std::strncpy(ip, "unknown-family", sizeof(ip) - 1);
    }
    
    std::cout << "[accept] new client from " << ip << ": " << port << std::endl;

    bufferevent* bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);

    if(!bev){
        std::cerr << "[accept] failed to create bufferevent" << std::endl;
        evutil_closesocket(fd);
        return;
    }

    bufferevent_setcb(bev, on_bev_read, on_bev_write, on_bev_event, nullptr);

    if(bufferevent_enable(bev, EV_READ | EV_WRITE) < 0){
        std::cerr << "[accept] failed to enable bufferevent" << std::endl;
        bufferevent_free(bev);
        return;
    }
}

void on_listen_error(struct evconnlistener* listener, void* arg){
    auto* server_ctx = static_cast<ServerContext*>(arg);
    event_base* base = server_ctx->base;

    int err = EVUTIL_SOCKET_ERROR();
    std::cerr << "[listener] error: " << evutil_socket_error_to_string(err) << std::endl;

    if(listener){
        evconnlistener_free(listener);
    }

    event_base_loopbreak(base);
}

int main(){
    event_base* base = event_base_new();
    if(!base){
        std::cerr << "failed to create enevt_base" << std::endl;
        return 1;
    }

    std::cout << "event backend method: "
              << event_base_get_method(base) << std::endl;
    
    ServerContext server_ctx;
    server_ctx.base = base;

    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(0);
    sin.sin_port = htons(5555);

    evconnlistener* listener = evconnlistener_new_bind(
        base,
        on_accept,
        &server_ctx,
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

    evconnlistener_set_error_cb(listener, on_listen_error);

    std::cout << "listening on 0.0.0.0:5555 ..." << std::endl;
    std::cout << "test with: nc 127.0.0.1 5555" << std::endl;
    
    event_base_dispatch(base);

    evconnlistener_free(listener);
    event_base_free(base);
    return 0;
}
