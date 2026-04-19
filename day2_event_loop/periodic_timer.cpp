#include <event2/event.h>
#include <iostream>
#include <cstdlib>

struct TimerContext {
    event_base* base;
    int count;
    int max_count;
};

void on_periodic_timeout(evutil_socket_t, short, void* arg) {
    auto* ctx = static_cast<TimerContext*>(arg);
    ++ctx->count;

    std::cout << "[timer] fired " << ctx->count
            << " / " << ctx->max_count << std::endl;

    if(ctx->count >= ctx->max_count){
        std::cout << "[timer] reached max count, exiting event loop..." << std::endl;
        event_base_loopexit(ctx->base, nullptr);        // 回调里数到 5 次后，调用 event_base_loopexit() 请求退出循环
    }
}

int main(){
    event_base* base = event_base_new();    // 创建事件循环容器
    if(!base){
        std::cerr << "failed to create event_base" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "event backend method:" << event_base_get_method(base) << std::endl;

    TimerContext ctx;
    ctx.base = base;
    ctx.count = 0;
    ctx.max_count = 5;

    timeval interval;
    interval.tv_sec = 1;
    interval.tv_usec = 0;

    event* timer_event = event_new(
        base,
        -1,
        EV_PERSIST,
        on_periodic_timeout,
        &ctx
    );              // 创建一个“每 1 秒触发一次”的定时事件对象
    // event_new()创建的是一个struct event事件对象。

    if(!timer_event){
        std::cerr << "failed to create timer event" << std::endl;
        event_base_free(base);
        return EXIT_FAILURE;
    }

    if(event_add(timer_event, &interval) < 0){      // 把这个事件注册到 base 里，让它进入等待触发状态
        std::cerr << "failed to add timer event" << std::endl;
        event_free(timer_event);
        event_base_free(base);
        return EXIT_FAILURE;
    }

    std::cout << "starting event loop..." << std::endl;
    event_base_dispatch(base);          // 开始循环，等待事件发生并执行回调
    std::cout << "event loop exited." << std::endl;

    event_free(timer_event);
    event_base_free(base);
    return EXIT_SUCCESS;
}

/*
1、event_base_new() 创建事件循环容器
2、event_new() 创建一个“每1秒触发一次”的定时事件对象
3、event_add() 把这个事件注册到base里，让它进入等待触发状态
4、event_base_dispatch()开始循环，等待事件发生并执行回调
5、回调里数到5次后，调用event_base_loopexit()请求退出循环
*/

/**
event_new()
   ↓
已初始化（initialized）
   ↓   event_add()
等待中（pending）
   ↓   条件满足：超时 / 可读 / 可写 / 信号
已激活（active）
   ↓   event_base_dispatch() 调度执行回调
执行 callback
   ↓
[分两种]
1. 非持久事件：回调前后会变成 non-pending
2. 持久事件(EV_PERSIST)：回调执行后仍然保持 pending
 */