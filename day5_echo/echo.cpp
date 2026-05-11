#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include <iostream>

struct ServerContext{
    event_base* base;
};

void on_bev_read(struct bufferevent* bev, void* ctx){
    (void)ctx;

    evbuffer* input = bufferevent_get_input(bev);
    evbuffer* output = bufferevent_get_output(bev);

    evbuffer_add_buffer(output, input);
}

void on_bev_event(struct bufferevent* bev, short events, void* ctx){
    (void)ctx;

    if(events & BEV_EVENT_EOF){
        std::cout << "client closed connection" << std::endl;
    }else if(events & BEV_EVENT_ERROR){
        std::cerr << "connection error" << std::endl;
    }

    bufferevent_free(bev);
}

void on_accept(struct evconnlistener* listener, evutil_socket_t fd, 
    struct sockaddr* addr, int socklen, void* arg)
{
    (void)listener;
    (void)fd;
    (void)addr;
    (void)socklen;
    (void)arg;

    std::cout << "a new client connected" << std::endl;

    auto* server_ctx = static_cast<ServerContext*>(arg);

    bufferevent* bev = bufferevent_socket_new(server_ctx->base, fd, BEV_OPT_CLOSE_ON_FREE);

    /*在 server_ctx->base 这个事件循环上，基于刚刚 accept 到的这个客户端 socket fd，
    创建一个 socket 类型的 bufferevent；
    并且设置为“以后释放 bev 时，顺便把这个 socket 也关掉”*/

    if(!bev){
        std::cerr << "failed to create bufferevent" << std::endl;
        evutil_closesocket(fd);
        return;
    }

    bufferevent_setcb(bev, on_bev_read, nullptr, on_bev_event, nullptr);

    if(bufferevent_enable(bev, EV_READ | EV_WRITE) < 0 ){
        std::cerr << "failed to enable bufferevent" << std::endl;
        bufferevent_free(bev);
        return;
    }
}


int main(){
    event_base* base = event_base_new();

    if(!base){
        std::cerr << "failed to create event_base" << std::endl;
        return 1;
    }

    ServerContext server_ctx;
    server_ctx.base = base;

    // 准备监听地址：
    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(0);
    sin.sin_port = htons(5555);

    // 创建监听器：
    evconnlistener* listener = evconnlistener_new_bind(
        base,
        on_accept,
        &server_ctx,
        LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
        -1,
        reinterpret_cast<sockaddr*>(&sin),
        sizeof(sin)
    );
    /*
    “在 base 这个事件循环上，创建一个监听器；监听 sin 指定的地址；
    当有新连接到来时调用 on_accept；把 &server_ctx 这个上下文一起传给回调；
    监听器行为受这些 LEV_OPT_* 标志控制；未完成连接队列长度由 backlog 决定。”
    */

    if(!listener){
        std::cerr << "failed to create listener" << std::endl;
        event_base_free(base);
        return 1;
    }

    std::cout << "lsitening on 0.0.0.0:5555" << std::endl;

    event_base_dispatch(base);

    evconnlistener_free(listener);
    event_base_free(base);

    return 0;
}