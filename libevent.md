4.18
1、libevent源码编译，make -j"$(nproc)"，-j"$(nproc)"：使用服务器所有CPU核心，加速编译

2、编译后的源码，需要执行make install，自动复制编译产物，
头文件.h→复制到 /usr/local/include/event2/
库文件.so→ 复制到 /usr/local/lib
Ldconfig，作用：让系统立刻识别新安装的库。

3、CMakeLists.txt——cmake_minimum_required(VERSION 3.10)
project(day1_libevent_hello)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON) 必须用C++17
add_executable(day1_libevent_hello main.cpp) 把 main.cpp 编译成一个可执行程序，名字叫 day1_libevent_hello
target_link_libraries(day1_libevent_hello event) 给可执行程序 链接 libevent 库（libevent.so），不加这行，代码用不了libevent

4、timeval 是 Linux / 跨平台标准的时间结构体
struct timeval {
    time_t tv_sec;  // 秒
    suseconds_t tv_usec;  // 微秒（1秒 = 1000000微秒）
};
timeval tv{2, 0};意思就是：2 秒 + 0 微秒 = 2 秒

5、一些函数原型
(1) int evtimer_add(struct event *ev, const struct timeval *tv);
作用：把一个定时器事件，加入到事件循环中，并设置超时时间
(2) libevent规定死的回调函数格式：
void callback(
    evutil_socket_t fd,    // 参数1,文件描述符 /socket 句柄
    short events,          // 参数2,事件类型
    void* arg              // 参数3
)
(3) event* evtimer_new(
    event_base* base,       // 事件所属的调度器
    event_callback_fn cb,   // 回调函数
    void* arg               // 要传给回调的参数
);

6、定时器在 libevent 里，本质就是一种 “超时回调事件”。它和网络可读、可写事件地位完全平等，只是触发条件不一样：
网络事件：socket 可读 / 可写 → 触发回调
信号事件：收到信号 → 触发回调
定时器：时间到了 → 触发回调
底层：libevent 内部会维护一棵时间堆（最小堆），它不断检查：有没有定时器时间到了？时间到了就调用你绑定的回调函数。