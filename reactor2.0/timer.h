#ifndef TIMER
#define TIMER
#include <sys/epoll.h>
#include <functional> 
#include <chrono>
#include <set>
#include <memory>
#include <iostream>

using namespace std;

struct TimerNodeBase{
    time_t expire;//触发时间
    int64_t id;
};

struct TimerNode : public TimerNodeBase {
    using Callback = std::function<void(const TimerNode &node)>;//const & 只想读取
    Callback func;
};

bool operator < (const TimerNodeBase &lhd,const TimerNodeBase &rhd){
    if(lhd.expire < rhd.expire)
        return true;
    else if(lhd.expire > rhd.expire)
        return false;
    return lhd.id < rhd.id;
}

class Timer {
public:
    static time_t GetTick(){
        //chrono时间库
        auto sc = chrono::time_point_cast<chrono::milliseconds>(chrono::steady_clock::now());
        auto temp = chrono::duration_cast<chrono::milliseconds>(sc.time_since_epoch());
        return temp.count();
    }
    static int64_t GenID(){
        return ++gid;
    }
    TimerNodeBase AddTimer(time_t msec,TimerNode::Callback func){
        TimerNode tnode;
        tnode.expire = GetTick() + msec;
        tnode.func = func;
        tnode.id = GenID();
        timermap.insert(tnode);
        return static_cast<TimerNodeBase>(tnode);
    }
    bool DelTimer(TimerNodeBase &node){
        auto iter = timermap.find(node);
        if(iter != timermap.end()){
            timermap.erase(iter);
            return true;
        }
        return false;
    }

    bool CheckTimer(){
        auto iter = timermap.begin();
        if(iter->expire > GetTick()){
            timermap.erase(iter);
        }
        if(iter != timermap.end() && iter->expire <= GetTick()){
            iter->func(*iter);
            return true;
        }
        return false;
    }

    time_t TimeToSleep(){
        auto iter = timermap.begin();
        if(iter == timermap.end()){
            return -1;
        }
        time_t diss = iter->expire - GetTick();
        return diss > 0 ? diss : 0;
    }

private:
    static int64_t gid;
    set<TimerNode, std::less<>> timermap;
};

int64_t Timer::gid = 0;

// int main() {
//     int epfd = epoll_create(1);//可以采用IO时间和定时器事件在同一个线程执行的方案，利用epoll的epoll_wait()第四个参数做定时延时。
//     unique_ptr<Timer> timer = make_unique<Timer>();
//     int i = 0;
//     timer->AddTimer(1000,[&](const TimerNode &node){
//         cout << Timer::GetTick() << "node id" << node.id << "revoked times:" << ++i << endl;
//     });
//     timer->AddTimer(1000,[&](const TimerNode &node){
//         cout << Timer::GetTick() << "node id" << node.id << "revoked times:" << ++i << endl;
//     });
//     timer->AddTimer(3000, [&](const TimerNode &node) {
//         cout << Timer::GetTick() << "node id:" << node.id << " revoked times:" << ++i << endl;
//     });
//     auto node = timer->AddTimer(2100, [&](const TimerNode &node) {
//         cout << Timer::GetTick() << "node id:" << node.id << " revoked times:" << ++i << endl;
//     });
//     timer->DelTimer(node);
//     cout << "now time:" << Timer::GetTick() << endl;
//     epoll_event ev[64] = {0};
//     while (true){
//         /*
//             -1 永久阻塞
//             0 没有事件立刻返回，有事件就拷贝到 ev 数组当中
//             t > 0  阻塞等待 t ms， 
//             timeout  最近触发的定时任务离当前的时间
//         */        
//         int n = epoll_wait(epfd,ev,64,timer->TimeToSleep());
//         for (int i = 0; i < n; i++) {
//             /**/
//         }
//         /* 处理定时事件*/
//         while(timer->CheckTimer());
//     }
//     return 0;    
// }







#endif