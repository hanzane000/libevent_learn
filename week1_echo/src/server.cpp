#include "server.h"
#include "logger.h"

#include <arpa/inet.h>
#include <cstring>
#include <string>

#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

namespace{

std::string peer_to_string(struct sockaddr* addr){
    if(addr == nullptr){
        return "unknown";
    }

    if(addr->sa_family == AF_INET){
        char ip[64] = {0};
        auto* sin = reinterpret_cast<sockaddr_in*>(addr);

        const char* result = inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip));
        const uint16_t port = ntohs(sin->sin_port);

        if(result != nullptr){
            return std::string(ip) + ": " + std::to_string(port);
        }
    }

    return "unknown";
}
}   // namespace

EchoServer::EchoServer(uint16_t port, int timer_interval_sec) : 
    port_(port), timer_interval_sec_(timer_interval_sec) {
}

EchoServer::~EchoServer(){
    cleanup();
}

bool EchoServer::init(){
    if(!init_base()){
        return false;
    }

    if(!init_listener()){
        return false;
    }

    if (!init_timer()) {
    cleanup();
    return false;
    }

    logger::info("server init success");
    return true;
}

void EchoServer::run() {
    if(base_ == nullptr){
        logger::error("run failed: event_base is null");
        return;
    }

    running_ = true;
    logger::info("server running on port" + std::to_string(port_));

    event_base_dispatch(base_);

    running_ = false;
    logger::info("event loop exited");
}

void EchoServer::stop() {
    if(base_ != nullptr && running_){
        logger::info("stopping server");
        event_base_loopbreak(base_);
    }
}

std::size_t EchoServer::current_connections() const{
    return current_connections_;
}

std::size_t EchoServer::total_connections() const{
    return total_connections_;
}

std::size_t EchoServer::total_closed_connections() const{
    return total_closed_connections_;
}

bool EchoServer::init_base(){
    base_ = event_base_new();
    if(base_ == nullptr){
        logger::error("failed to create event_base");
        return false;
    }

    logger::info("event_base created");
    return true;
}

bool EchoServer::init_listener(){
    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(0);
    sin.sin_port = htons(port_);

    const unsigned flags = LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE;

    listener_ = evconnlistener_new_bind(base_, &EchoServer::on_accept, this, 
                                flags, -1, reinterpret_cast<sockaddr*>(&sin), sizeof(sin));
    
    if(listener_ == nullptr){
        logger::error("failed to create evconnlistener");
        return false;
    }

    evconnlistener_set_error_cb(listener_, &EchoServer::on_listener_error);

    logger::info("listener created on port" + std::to_string(port_));
    return true;
}

bool EchoServer::init_timer(){
    timer_event_ = event_new(base_, -1, EV_PERSIST, &EchoServer::on_timer, this);
    if(timer_event_ == nullptr){
        logger::error("failed to create timer event");
        return false;
    }

    timeval tv{};
    tv.tv_sec = timer_interval_sec_;
    tv.tv_usec = 0;

    if(event_add(timer_event_, &tv) < 0){
        logger::error("failed to add timer event");
        event_free(timer_event_);
        timer_event_ = nullptr;
        return false;
    }

    logger::info("timer event created, interval=" + std::to_string(timer_interval_sec_) + "s");
    
    return true;
}

void EchoServer::cleanup(){
    if(timer_event_){
        event_free(timer_event_);
        timer_event_ = nullptr;
    }

    if(listener_){
        evconnlistener_free(listener_);
        listener_ = nullptr;
    }

    if(base_){
        event_base_free(base_);
        base_ = nullptr;
    }
}

void EchoServer::on_accept(struct evconnlistener*, evutil_socket_t fd,
                            struct sockaddr* addr, int socklen, void* ctx){

    auto* server = static_cast<EchoServer*>(ctx);
    server->handle_accept(fd, addr, socklen);
}

void EchoServer::on_listener_error(struct evconnlistener* listener, void* ctx){
    auto* server = static_cast<EchoServer*>(ctx);
    server->handle_listener_error();
}

void EchoServer::on_read(struct bufferevent* bev, void* ctx){
    auto* server = static_cast<EchoServer*>(ctx);
    server->handle_read(bev);
}

void EchoServer::on_write(struct bufferevent* bev, void* ctx){
    auto* server = static_cast<EchoServer*>(ctx);
    server->handle_write(bev);
}

void EchoServer::on_event(struct bufferevent* bev, short events, void* ctx){
    auto* server = static_cast<EchoServer*>(ctx);
    server->handle_event(bev, events);
}

void EchoServer::on_timer(evutil_socket_t,
                          short,
                          void* ctx) {
    auto* server = static_cast<EchoServer*>(ctx);
    server->handle_timer();
}



void EchoServer::handle_accept(evutil_socket_t fd, struct sockaddr* addr, int){
    logger::info("new connection from " + peer_to_string(addr));

    bufferevent* bev = bufferevent_socket_new(base_, fd, BEV_OPT_CLOSE_ON_FREE);

    if(bev == nullptr){
        logger::error("failed to create bufferevent");
        evutil_closesocket(fd);
        return;
    }

    bufferevent_setcb(bev, &EchoServer::on_read, &EchoServer::on_write, &EchoServer::on_event, this);
    
    if(bufferevent_enable(bev, EV_READ | EV_WRITE) < 0 ){
        logger::error("failed to enable bufferevent");
        bufferevent_free(bev);
        return;
    }

    ++current_connections_;
    ++total_connections_;

    logger::info("connection attached, current=" + std::to_string(current_connections_));
}

void EchoServer::handle_listener_error(){
    const int err = EVUTIL_SOCKET_ERROR();
    const char* msg = evutil_socket_error_to_string(err);

    logger::error("listener error: " + std::string(msg));

    if(listener_ != nullptr){
        evconnlistener_free(listener_);
        listener_ = nullptr;
    }

    stop();
}

void EchoServer::handle_read(struct bufferevent* bev){
    evbuffer* input = bufferevent_get_input(bev);
    evbuffer* output = bufferevent_get_output(bev);

    if(input == nullptr || output == nullptr){
        logger::error("input/output buffer is null");
        return;
    }

    const std::size_t input_len = evbuffer_get_length(input);
    logger::info("read callback triggered, bytes=" + std::to_string(input_len));

    evbuffer_add_buffer(output, input);
}

void EchoServer::handle_write(struct bufferevent* bev){
    evbuffer* output = bufferevent_get_output(bev);
    if(output == nullptr){
        logger::error("output buffer is null");
        return;
    }

    const std::size_t remaining = evbuffer_get_length(output);
    logger::info("write callback triggered, remaining output bytes=" + std::to_string(remaining));
}

void EchoServer::handle_event(struct bufferevent* bev, short events){
    if(events & BEV_EVENT_EOF){
        logger::info("connection closed by peer");
    }else if(events & BEV_EVENT_ERROR){
        const int err = EVUTIL_SOCKET_ERROR();
        logger::error("connection error: " + std::string(evutil_socket_error_to_string(err)));
    }else if(events & BEV_EVENT_TIMEOUT){
        logger::warn("connection timeout");
    }else if(events & BEV_EVENT_CONNECTED){
        logger::info("connection established");
    }

    if(current_connections_ > 0){
        --current_connections_;
    }

    ++total_closed_connections_;

    logger::info("connection released, current=" + std::to_string(current_connections_));

    bufferevent_free(bev);
}

void EchoServer::handle_timer(){
    logger::info(
        "stats: current=" + std::to_string(current_connections_) + 
        ", total_accepted=" + std::to_string(total_connections_) +
        ", total_closed=" + std::to_string(total_closed_connections_)
    );
}