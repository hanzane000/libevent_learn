#include <event2/event.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>

struct StdinContext {
    event_base* base;
};
/*
作用：传递事件循环对象给回调函数
libevent回调函数只能传一个void* 参数，所以用结构体打包需要的数据
*/

void on_stdin_readable(evutil_socket_t fd, short events, void* arg){    // 当 stdin 变得可读时，libevent 会自动调用它。
    auto* ctx = static_cast<StdinContext*>(arg);

    if(!(events & EV_READ)){
        return;
    }

    char buf[1024] = {0};
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if(n < 0){
        std::cerr << "[Stdin] read error " << std::endl;
        event_base_loopexit(ctx->base, nullptr);
        return;
    }

    if(n == 0){
        std::cout << "[stdin] EOF, exiting..." << std::endl;
        event_base_loopexit(ctx->base, nullptr);
        return;
    }

    std::string input(buf, static_cast<size_t>(n));

    while(!input.empty() && (input.back() == '\n' || input.back() == '\r')){
        input.pop_back();
    }

    std::cout << "[stdin] you entered " << input << std::endl;

    if(input == "quit"){
        std::cout << "[stdin] quit command received, exiting event loop..." << std::endl;
        event_base_loopexit(ctx->base, nullptr);
    }
}

int main(){
    event_base* base = event_base_new();
    if(!base) {
        std::cerr << "failed to create event_base" << std::endl;
        return 1;
    }

    std::cout << "event backend method: " << event_base_get_method(base) << std::endl;

    StdinContext ctx;
    ctx.base = base;

    event* stdin_event = event_new(
        base,
        STDIN_FILENO,
        EV_READ | EV_PERSIST,
        on_stdin_readable,
        &ctx
    );

    if(!stdin_event){
        std::cerr << "failed to create stdin event" << std::endl;
        event_base_free(base);
        return 1;
    }

    if(event_add(stdin_event, nullptr) < 0){
        std::cerr << "failed to add stdin event" << std::endl;
        event_free(stdin_event);
        event_base_free(base);
        return 1;
    }

    std::cout << "waiting for stdin input..." << std::endl;
    std::cout << "type something, or type 'quit' to exit." << std::endl;

    event_base_dispatch(base);

    event_free(stdin_event);
    event_base_free(base);
    return 0;
}

/*
功能：使用libevent事件驱动库，异步监听键盘输入（标准输入stdin），实现：
1、实时读取在控制台输入的文字
2、自动去掉换行符
3、打印输入的内容
4、输入quit时自动退出程序
5、遇到输入结束（EOF）或错误时安全退出

它是非阻塞、事件驱动的，而不是传统的 cin 阻塞等待。

传统写法std::getline(std::cin, s); 阻塞式读取

创建 event_base
→ 创建一个监听 stdin 可读的 event
→ 把这个 event 加入 base
→ 启动事件循环
→ 一旦 stdin 可读，就回调 on_stdin_readable()
→ 读取输入并处理
→ 输入 quit 或遇到 EOF 时退出循环
*/