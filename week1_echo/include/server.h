#pragma once

#include <cstdint>
#include <cstddef>

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/bufferevent.h>

class EchoServer {
public:
    explicit EchoServer(uint16_t port, int timer_interval_sec = 5);
    ~EchoServer();

    EchoServer(const EchoServer&) = delete;
    EchoServer& operator=(const EchoServer&) = delete;

    bool init();
    void run();
    void stop();

    std::size_t current_connections() const;
    std::size_t total_connections() const;
    std::size_t total_closed_connections() const;

private:
    bool init_base();
    bool init_listener();
    bool init_timer();
    void cleanup();

    static void on_accept(struct evconnlistener* listener,
                            evutil_socket_t fd,
                            struct sockaddr* addr,
                            int socklen,
                            void* ctx);
    
    static void on_listener_error(struct evconnlistener* listener, void* ctx);
    static void on_read(struct bufferevent* bev, void* ctx);
    static void on_write(struct bufferevent* bev, void* ctx);
    static void on_event(struct bufferevent* bev, short events, void* ctx);
    static void on_timer(evutil_socket_t fd, short events, void* ctx);

    void handle_accept(evutil_socket_t fd, struct sockaddr* addr, int socklen);
    void handle_listener_error();
    void handle_read(struct bufferevent* bev);
    void handle_write(struct bufferevent* bev);
    void handle_event(struct bufferevent* bev, short events);
    void handle_timer();

    struct event_base* base_ = nullptr;
    struct evconnlistener* listener_ = nullptr;
    struct event* timer_event_ = nullptr;

    uint16_t port_ = 0;
    int timer_interval_sec_ = 5;
    bool running_ = false;

    std::size_t current_connections_ = 0;
    std::size_t total_connections_ = 0;
    std::size_t total_closed_connections_ = 0;
};