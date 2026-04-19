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

4.19
1、event_base 和 event 的关系
event_base：事件循环的“总控中心”或“调度器”  event：一个具体的“监听任务”
event_base 负责维护事件集合、等待内核通知、管理活跃队列、调度回调
event 只是描述“当什么条件发生时，执行哪个回调”

2、event_new()
创建的是一个struct event事件对象

官方文档写得很明确：event_new() 会“allocate and construct a new event for use with base”，也就是“分配并构造一个新的事件对象”；新建出来的事件是 initialized but non-pending，也就是“已初始化，但还没有进入等待状态”。

所以event_new() 不是：不是立刻创建一个线程；不是立刻开始计时；不是立刻把事件放进循环里运行，只是先造出一个“事件描述对象”

3、event_add() 为什么叫“加入 pending”
这是 libevent 里非常重要的状态概念。
一个事件大致会经历：initialized：刚创建好；pending：已经注册到 base，正在等待触发；active：条件已满足，准备执行或正在等待调度执行回调
官方文档原话就是：调用 event_add() 会让一个 non-pending 的事件变成 pending；当触发条件发生时，它会变成 active，然后执行回调。
if(event_add(timer_event, &interval) < 0)
这一步的含义是：把 timer_event 正式注册到 base；给它设置 1 秒的超时时间；从现在开始，libevent 会在事件循环里关注它

4、event_base_dispatch() 什么时候退出
event_base_dispatch(base) 等价于：event_base_loop(base, 0);也就是“默认模式下跑事件循环”。
退出的情况：
没有更多的事件可处理了
有人调用了event_base_loopexit()
有人调用了event_base_loopbreak()
底层后端出现错误

5、event_base_loopexit() 和 event_base_loopbreak() 的区别
event_base_loopexit(bsas, tv)含义是：让事件循环在一段时间后退出；如果tv == NULL，就是尽快退出，但不是立刻中断当前这一批活跃回调
event_base_loopbreak(base)含义是：让事件循环尽快中断。
loopexit(NULL)：这轮已经活跃起来的回调，能跑完的都跑完
loopbreak()：只保证当前正在执行的这个回调跑完，其他已经活跃但还没执行的，先不跑了

6、一次性定时器和周期定时器，本质区别在哪
一次性定时器的特点是：触发一次；回调执行完后，不在pending；如果还想再触发，需要重新event_add()
这正是官方文档说的默认行为：事件一旦因为 timeout 或 IO 变 active，在回调执行前就会变成 non-pending；如果想继续用，需要再次 event_add()。
周期定时器的特点是：触发后不会自动消失；回调执行完成后仍然保持pending；超时时间会重新开始计算；
这就是EV_PERSTST的作用，官方文档明确说：
如果设置了 EV_PERSIST，事件在回调被激活后仍保持 pending；并且持久事件的 timeout 会在每次回调运行后重置。

今日必背：
event_base 是事件循环和调度中心，event 是具体监听任务。
event_new() 只是创建事件对象，不会自动开始监听。
event_add() 让事件进入 pending，表示已注册并开始等待触发。
event_base_dispatch() 会一直跑，直到没有事件了，或者被 loopexit/loopbreak 停掉。
loopexit(NULL) 更温和，会让当前已激活的回调跑完；loopbreak() 更直接，当前回调结束就停。