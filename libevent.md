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

4.20
1、libevent事件类型
分成两类，一类是“触发原因”类事件：EV_TIMEOUT EV_READ EV_WRITE EV_SIGNAL
另一类是“行为修饰”类标志：EV_PERSTST EV_ET
1）EV_TIMEOUT：超时事件，表示“在指定时间到了以后触发”，用于定时器。
回调触发原因之一可以是EV_TIMEOUT，而timeout事件通常通过event_add(ev, &tv)指定等待时间。

2）EV_READ可读事件，表示“某个fd现在就可以读，而不会阻塞”
event_new(base, STDIN_FILENO, EV_READ | EV_PERSIST, on_stdin_readable, &ctx);
当标准输入可读时，调用回调。
在服务器中，监听 socket 上有新连接，对监听 fd 来说，“可读”通常意味着有新的连接可以 accept()；已连接 socket 上有数据到来，对连接 fd 来说，“可读”通常意味着对端发来了数据，可以 recv()/read()

3）EV_WRITE可写事件 表示“某个 fd 现在可以写，而不会阻塞”。
“内核现在允许你继续往这个 fd 里塞数据了。”
应用：非阻塞 socket 发送大块数据；connect() 进行中的套接字，等待连接完成；写缓冲区之前满了，现在恢复可写；上游代理、转发器、网关中做异步回写

4）EV_SIGNAL信号事件 表示“收到某个信号时触发回调”。
常用于做程序的优雅退出或运行期控制：收到 SIGINT（Ctrl+C）时退出；收到 SIGTERM 时清理资源后关闭服务；收到 SIGHUP 时重载配置；收到某个信号时打印状态信息

5）EV_PERSTST持久事件标志
不是单独的触发原因，而是一个“行为修饰标志”
官方文档明确说：非持久事件一旦触发，就会变成 non-pending；而持久事件在变 active 后仍然保持 pending，除非你手动 event_del()。如果持久事件还带 timeout，那么每次回调后 timeout 会被重置，因此可以实现周期定时。

6）EV_ET边沿触发标志
“行为修饰标志”。官方头文件说明可以把 EV_ET 和读写事件一起使用，请求 edge-triggered（边沿触发）行为；但并不是所有后端都支持。
应用：主要用于高性能网络程序，尤其在 Linux epoll 场景里常见。
相比“水平触发”，边沿触发更强调：
状态从“不可读”变“可读”时通知一次；之后你要尽量把数据一次性读空；否则可能不会再次提醒你


2、“持久事件”和“非持久事件”的区别
本质区别：非持久事件一旦被触发，在执行回调之前就会自动变成 non-pending；持久事件在被触发后仍然保持 pending，不会因为执行了一次回调就自动失效。
这就是 EV_PERSIST 的意义。头文件和官方文档都把它定义为：事件激活时不会被自动移除。

3、为什么事件触发一次后默认会变成non-pending
默认情况下，一个 pending 事件一旦因为 fd 可读、可写，或者 timeout 到期而变成 active，它会在回调执行前变成 non-pending。这样如果你想让它再次等待，只要明确地再调用一次 event_add() 就行。
1）最核心的设计思想：默认按“一次性任务”处理
libevent 把默认事件理解为：“我帮你等一次条件成立；条件一旦成立并交付给回调，这次等待任务就完成了。”
所以默认行为是：event_add()：把事件放入等待状态；条件满足：事件变 active；执行回调前：自动从 pending 里摘掉；这一轮任务结束
2）这样设计更安全，避免“无意识地一直挂着”
如果默认事件触发后还一直保持 pending，会带来一个问题：很多一次性任务会在你没注意时反复继续存在。
3）这样让“是否重复等待”变成显式逻辑，程序的控制权更清楚。
因为回调执行完后，你可以明确决定：
重新 event_add()：继续等下一次
不再 event_add()：本事件到此结束
改 timeout 后再 event_add()：换一种等待策略
干脆 event_free()：彻底不用了
这比“默认一直挂着，然后你还得记得手动删掉”更适合多数一次性场景。

4、pending 和 active状态的关系
libevent 官方文档就是这样描述生命周期的：事件先被 add 成为 pending；当触发条件发生时，事件变成 active，随后运行回调；如果事件是 persistent，它会继续保持 pending；如果不是 persistent，它在回调执行前会变成 non-pending。
1）non-pending 和 active 可以同时存在吗？
可以。而且对于默认的非持久事件，这正是最典型的情况。
非持久事件，触发后会出现这样的组合状态：active + non-pending
意思是：已经被激活了，准备执行回调，但它已经不再“继续等待下一次触发”
持久事件，触发后会出现这样的组合状态：active + pending
意思是：已经被激活了，准备执行回调，但因为它是持久事件，所以它仍然保留“继续等待下一次触发”的资格
pending / non-pending 是“是否继续注册等待”这一维
active / non-active 是“是否已经被触发并进入活跃队列”这一维

4.21
1、哪些代码可以看做是libevent开发的“固定套路”
第一类是 事件循环核心的创建与释放
event_base* base = event_base_new();
event_base_dispatch(base);
event_base_free(base);
这几句是最基础的模版：先创建事件循环对象，后进入循环，最后释放资源。
以后不管是监听socket、定时器，还是信号事件，基本都绕不开event_base

第二类是 监听器的创建套路
evconnlistener_new_bind(...)
这是libevent写TCP服务器时常见的固定入口。
把“创建监听socket、bind、listen、注册接受连接逻辑”这一套包装起来了。
函数原型：
struct evconnlistener *evconnlistener_new_bind(struct event_base *base,
    evconnlistener_cb cb, void *ptr, unsigned flags, int backlog,
    const struct sockaddr *sa, int socklen);
参数：
base：挂到哪个 event_base 事件循环上。
cb：当有新连接到来时调用的回调函数。
ptr：用户自定义参数，会原样传给回调里的void* arg
flags：监听器选项，比如LEV_OPT_CLOSE_ON_FREE、LEV_OPT_REUSEABLE。
backlog：传给listen()的监听队列长度；设为-1时，libevent会选一个合理默认值
sa：要绑定监听的地址
socklen：地址结构体长度

evconnlistener* listener = evconnlistener_new_bind(
    base,
    on_accept,
    &ctx,
    LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
    -1,
    reinterpret_cast<sockaddr*>(&sin),
    sizeof(sin)
);
意思就是：在 base 这个事件循环上，创建一个监听 0.0.0.0:5555 的 TCP 监听器；有新连接时调用 on_accept；把 &ctx 传给回调；释放监听器时顺便关闭底层 fd，并允许地址复用。 这正是 evconnlistener_new_bind() 的典型用法。

第三类是 上下文传参套路
struct ListenerContext{
    event_base* base;
};
再把&ctx传给监听器，回调里再从void* arg转回来。
这是C风格事件库非常常见的写法：库只给你一个void* ，需要自己用结构体打包需要的数据。以后不只是 listener，很多 event/bufferevent 回调都会这么干。

第四类是 错误回调注册
evconnlistener_set_error_cb(listener, on_listener_error);
监听器除了“接收新连接”，通常还会单独配一个“出错怎么办”的回调。这也是比较标准的写法。实际开发里，错误处理不能只靠 if(!listener) 这种初始化检查，运行时错误也要兜住。

第五类是 资源释放套路
evconnlistener_free(listener);
event_base_free(base);
以及连接来了以后：evutil_closesocket(fd);
libevent 编程里，一个很重要的习惯就是：谁创建，谁释放；进入事件循环前后的资源生命周期要明确。监听器、event、bufferevent、socket 都要有对应释放动作。

2、static_cast 和 reinterpret_cast
auto* ctx = static_cast<ListenerContext*>(arg);
把无类型指针恢复成原来的真实类型
auto* sin = reinterpret_cast<sockaddr_in*>(addr);
把同一块地址按另一种结构布局来解释
static_cast 更像“在已有类型体系内做合理、受约束的转换”；reinterpret_cast 更像“把同一块内存地址按另一种类型强行解释”。

4.22
1、evconnlistener_new_bind()的功能
“让 libevent 帮我创建一个监听器，并绑定到某个地址；以后只要有新连接，就回调我指定的函数。”
struct evconnlistener* evconnlistener_new_bind(
    struct event_base* base,
    evconnlistener_cb cb,
    void* ptr,
    unsigned flags,
    int backlog,
    const struct sockaddr* sa,
    int socklen
)
官方定义：在给定的地址上创建一个evconnlistener，用于监听传入的TCP连接。和evconnlistener_new()的区别是：new_bind()由libevent自己创建并绑定监听socket；new()则要求自己先准备好一个已经绑定好的socket fd再交给它。
evconnlistener_new_bind() 用来在指定地址上创建 libevent 监听器。base 指定归属的事件循环，cb 指定新连接到来时的回调函数，ptr 是透传给回调的用户上下文，flags 控制监听器行为，backlog 控制待 accept 连接队列长度，sa 和 socklen 指定监听地址及其长度。常见入门写法是 LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE 加 backlog = -1。

2、bufferevent_socket_new
bufferevent* bev = bufferevent_socket_new(server_ctx->base, fd, BEV_OPT_CLOSE_ON_FREE); 
把一个“裸的 socket fd”包装成一个 libevent 能管理的“连接对象”。
在 server_ctx->base 这个事件循环上，基于刚刚 accept 到的这个客户端 socket fd，创建一个 socket 类型的 bufferevent；并且设置为“以后释放 bev 时，顺便把这个 socket 也关掉”。

bufferevent的意义就是：libevent帮你把“socket + 读缓冲 + 写缓冲 + 读写事件管理”打包成一个对象

3、void bufferevent_setcb
void bufferevent_setcb(struct bufferevent *bufev,
                       bufferevent_data_cb readcb,
                       bufferevent_data_cb writecb,
                       bufferevent_event_cb eventcb,
                       void *cbarg);


3.23
1、event bufferevent buffer这几个概念有什么区别
event 管“发生了什么事”，bufferevent 管“一个连接怎么收发数据”，evbuffer 管“数据本身放在哪里”。
event_base
   └── event          // 基础事件
   └── bufferevent    // 更高级的连接事件封装
           ├── input  -> evbuffer
           └── output -> evbuffer

event 是基础事件机制
bufferevent 是建立在事件机制上的高级封装
evbuffer 是 bufferevent 里面真正装数据的缓冲区

event 是事件通知机制，负责监视“事情是否发生”；bufferevent 是面向连接的高级 I/O 封装，负责管理 socket 的读写和回调；evbuffer 是数据缓冲区，负责保存收到的数据和待发送的数据。