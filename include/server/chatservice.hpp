#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <muduo/net/TcpConnection.h>
#include <unordered_map>
#include <functional>
#include <mutex>
#include "offlinemessmodel.hpp"
#include "usermodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include "json.hpp"
#include "redis.hpp"
using namespace std;
using namespace muduo;
using namespace muduo::net;
using json=nlohmann::json;
using MsgHandler=std::function<void(const TcpConnectionPtr &conn,json &js,Timestamp time)>;
// 聊天服务器业务类
class ChatService
{
public:
    //获取单例对象的接口函数
    static ChatService* instance();
    //登录
    void login(const TcpConnectionPtr &conn,json &js,Timestamp time);
    
    void logout(const TcpConnectionPtr &conn,json &js,Timestamp time);

    //注册

    void reg(const TcpConnectionPtr &conn,json &js,Timestamp time);

    void clientCloseException(const TcpConnectionPtr &conn);

    //一对一聊天业务
    void oneChat(const TcpConnectionPtr &conn,json &js,Timestamp time);

    //服务器异常退出时，重置方法
    void reset();

    void addFriend(const TcpConnectionPtr &conn,json &js,Timestamp time);


    void createGroup(const TcpConnectionPtr &conn,json &js,Timestamp time);
    void addGroup(const TcpConnectionPtr &conn,json &js,Timestamp time);
    void groupChat(const TcpConnectionPtr &conn,json &js,Timestamp time);


    void handleRedisSubscribeMessage(int userid,string msg);


    //获取消息对应的处理器
    MsgHandler getHandler(int msgid);
private:
    ChatService();
    unordered_map<int,MsgHandler> _handlerMap;

    //数据操作类对象
    UserModel _userModel;
    OfflineMsgModel _offlineMsgModel;
    FriendModel _friendModel;
    GroupModel _groupModel;

    //存储在线用户的通信连接,注意线程安全问题
    unordered_map<int,TcpConnectionPtr> _userConnMap;

    //定义互斥锁
    mutex _connMapMtx;

    Redis _redis;

};
#endif