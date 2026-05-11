#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

struct ServerContext
{
    event_base* base;
};

double cal(double lhs, char op, double rhs){
    double result = 0.0;
    bool ok = true;

    switch(op){
        case '+':
            result = lhs + rhs;
            break;
        case '-':
            result = lhs - rhs;
            break;
        case '*':
            result = lhs * rhs;
            break;
        case '/':
            if (rhs == 0) {
                std::cout << "division by zero" << std::endl;
                ok = false;
            } else {
                result = lhs / rhs;
            }
            break;
        default:
            std::cout << "unsupported operator" << std::endl;
            ok = false;
            break;
    }

    if(!ok){
        return;
    }

    return result;
}

void on_bev_read(struct bufferevent* bev, void* ctx){
    (void)ctx;

    evbuffer* input = bufferevent_get_input(bev);
    size_t len = evbuffer_get_length(input);

    if(len == 0){
        return;
    }

    char* data = new char[len + 1];
    evbuffer_remove(input, data, len);
    data[len] = '\0';

    std::string expr(data);
    delete[] data;

    std::cout << "client expression " << expr << std::endl;

    double lhs = 0.0;
    double rhs = 0.0;
    char op = 0;

    std::stringstream ss(expr);
    ss >> lhs >> op >>rhs;

    evbuffer* output = bufferevent_get_output(bev);

    if(ss.fail()){
        std::cout << "parse failed" << std::endl;
        std::string response = "error: invalid expression\n";
        evbuffer_add(output, response.c_str(), response.size());
        return;
    }

    std::cout << "lhs = " << lhs << ", op = " << op << ", rhs = " << rhs << std::endl;

    auto result = cal(lhs, op, rhs);

    std::cout << "result = " << result << std::endl;

    std::string response = std::to_string(result);
    response += '\n';

    evbuffer_add(output, response.c_str(), response.size());
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

void on_accept(struct evconnlistener* listener,
            evutil_socket_t fd,
            struct sockaddr* addr,
            int socklen,
            void* arg){
    (void)listener;
    (void)addr;
    (void)socklen;

    std::cout << "a new client connected " << std::endl;

    auto* server_ctx = static_cast<ServerContext*>(arg);

    bufferevent* bev = bufferevent_socket_new(server_ctx->base, fd, BEV_OPT_CLOSE_ON_FREE);
    if(!bev){
        std::cerr << "failed to create bufferevent" << std::endl;
        evutil_closesocket(fd);
        return;
    }

    bufferevent_setcb(bev, on_bev_read, nullptr, on_bev_event, nullptr);

    if(bufferevent_enable(bev, EV_READ | EV_WRITE) < 0){
        std::cerr << "failed to enable bufferevent" << std::endl;
        bufferevent_free(bev);
        return;
    }
}



int main(){
    // 1、创建事件循环核心
    event_base* base = event_base_new();
    if(!base){
        std::cerr << "failed to create event_base" << std::endl;
        return 1;
    }

    // 2 准备上下文
    ServerContext server_ctx;
    server_ctx.base = base;

    // 3 准备监听地址
    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(0);
    sin.sin_port = htons(5555);

    // 4 创建监听器
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
        std::cerr << "failed to create listener" << std::endl;
        event_base_free(base);
        return 1;
    }

    std::cout << "caculator server listening on 0.0.0.0:5555" << std::endl;

    // 5 启动事件循环
    event_base_dispatch(base);
    
    // 6 释放资源
    evconnlistener_free(listener);
    event_base_free(base);

    return 0;

}