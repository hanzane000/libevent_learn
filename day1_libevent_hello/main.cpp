#include <event2/event.h>
#include <iostream>

void on_timeout(evutil_socket_t, short, void* arg){
    auto* base = static_cast<event_base*>(arg);
    std::cout << "timer fired" << std::endl;
    event_base_loopexit(base, nullptr);     // 退出循环
}

int main(){
    event_base* base = event_base_new();    // 创建事件基座
    if(!base) {
        std::cerr << "failed to create event_base" << std::endl;
        return 1;
    }

    timeval tv{2, 0};       // Linux / 跨平台标准的时间结构体
    event* ev = evtimer_new(base, on_timeout, base);    // 创建定时器事件
    evtimer_add(ev, &tv);                   // 添加到事件基座

    std::cout << "dispatching...\n";
    event_base_dispatch(base);              // 开始事件循环

    event_free(ev);
    event_base_free(base);
    return 0;
}

/*
libevent规定死的回调函数格式：
void callback(
    evutil_socket_t fd,    // 参数1,文件描述符 /socket 句柄
    short events,          // 参数2,事件类型
    void* arg              // 参数3
)
*/

/*
evtimer_new(
    第一个base,   // 【作用1：事件属于哪个调度器】告诉 libevent：这个定时器事件 归谁管？
    on_timeout,   // 回调函数
    第二个base    // 【作用2：传给回调函数的参数 arg】这个值会原封不动传给回调函数的 void* arg
);
*/
