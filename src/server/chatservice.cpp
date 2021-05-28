#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <vector>
#include <map>
using namespace muduo;

ChatService* ChatService::instance()
{
    static ChatService service;
    return &service;
}
ChatService::ChatService()
{
    _handlerMap.insert({LOGIN_MSG,std::bind(&ChatService::login,this,_1,_2,_3)});
    _handlerMap.insert({REG_MSG,std::bind(&ChatService::reg,this,_1,_2,_3)});
    
    
    _handlerMap.insert({LOGINOUT_MSG,std::bind(&ChatService::logout,this,_1,_2,_3)});

    _handlerMap.insert({ONE_CHAT_MSG,std::bind(&ChatService::oneChat,this,_1,_2,_3)});
    _handlerMap.insert({ADD_FRIEND_MSG,std::bind(&ChatService::addFriend,this,_1,_2,_3)});
    _handlerMap.insert({CREATE_GROUP_MSG,std::bind(&ChatService::createGroup,this,_1,_2,_3)});
    _handlerMap.insert({ADD_GROUP_MSG,std::bind(&ChatService::addGroup,this,_1,_2,_3)});
    _handlerMap.insert({GROUP_CHAT_MSG,std::bind(&ChatService::groupChat,this,_1,_2,_3)});

    if(_redis.connect())
    {
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage,this,_1,_2));
    }
}
void ChatService::login(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int id=js["id"].get<int>();
    string pwd=js["password"];

    User user=_userModel.query(id);
    LOG_INFO<<"账号为："<<user.getId()<<" "<<user.getPwd();
    if(user.getId()==id && user.getPwd()==pwd)
    {
        if(user.getState()=="online")
        {
            json response;
            response["msgid"]=LOGIN_MSG_ACK;
            response["errno"]=2;
            response["errmsg"]="该账号已经登录";
            conn->send(response.dump());
        }
        else
        {
            //记录用户连接信息
            {
                unique_lock<mutex> lock(_connMapMtx);
                _userConnMap.insert({id,conn});
            }

            //向redis订阅channel(id)
            _redis.subscribe(id);
            //更新state
            user.setState("online");
            _userModel.updateState(user);
            json response;
            response["msgid"]=LOGIN_MSG_ACK;
            response["errno"]=0;
            response["id"]=user.getId();
            response["name"]=user.getName();

            //检查用户是否有离线消息
            vector<string> vec=_offlineMsgModel.query(id);
            if(!vec.empty())
            {
                response["offlinemsg"]=vec;
                _offlineMsgModel.remove(id);
            }

            //查询该用户的好友信息
            vector<User> userVec=_friendModel.query(id);
            if(!userVec.empty())
            {
                vector<string> vec2;
                for(User &user:userVec)
                {
                    json js;
                    js["id"]=user.getId();
                    js["name"]=user.getName();
                    js["state"]=user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"]=vec2;
            }            

            conn->send(response.dump());

        }
    }
    else
    {
        json response;
        response["msgid"]=LOGIN_MSG_ACK;
        response["errno"]=1;
        response["errmsg"]="账号或密码错误";
        conn->send(response.dump());
    }
}
void ChatService::reg(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    string name=js["name"];
    string pwd=js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state=_userModel.insert(user);
    if(state)
    {
        json response;
        response["msgid"]=REG_MSG_ACK;
        response["errno"]=0;
        response["id"]=user.getId();
        conn->send(response.dump());
    }
    else
    {
        json response;
        response["msgid"]=REG_MSG_ACK;
        response["errno"]=1;
        conn->send(response.dump());      
    }
}
MsgHandler ChatService::getHandler(int msgid)
{
    auto it=_handlerMap.find(msgid);
    if(it==_handlerMap.end())
    {
        //返回一个空处理器
        return [=](const TcpConnectionPtr &conn,json &js,Timestamp time)
        {
            LOG_ERROR<<"msgid: "<<msgid<<" can not find handler";
        };
    }
    else
    {
        return _handlerMap[msgid];
    }
    return _handlerMap[msgid];
}

//处理客户端异常关闭
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    //1.在_userConnMap中删除该链接
    User user;
    {
        unique_lock<mutex> lock(_connMapMtx);
        for(auto it=_userConnMap.begin();it!=_userConnMap.end();it++)
        {
            if(it->second==conn)
            {
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }
    //2. 更改数据库，online改为offline
    if(user.getId()!=-1)
    {
        user.setState("offline");
        _userModel.updateState(user);
    }
    
}

void ChatService::oneChat(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int to_id=js["toid"].get<int>();
    
    //标识用户是否在线
    unique_lock<mutex> lock(_connMapMtx);
    auto it=_userConnMap.find(to_id);

    //两个用户在同一个服务器上
    if(it!=_userConnMap.end())
    {
        //toid在线,服务器主动推送消息给toid用户
        it->second->send(js.dump());
        return;
    }

    // 查询toid是否在线
    User user=_userModel.query(to_id);
    if(user.getState()=="online")
    {
        _redis.publish(to_id,js.dump());
        return ;
    }


    //如果不在线
    _offlineMsgModel.insert(to_id,js.dump());
}


void ChatService::reset()
{
    //把所有online的用户设置成offline
    _userModel.resetState();
}

//添加好友 msgid id friendid
void ChatService::addFriend(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int userid=js["id"].get<int>();
    int friendid=js["friendid"].get<int>();

    //存储好友信息 
    _friendModel.insert(userid,friendid);
    
}
void ChatService::createGroup(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int userid=js["id"].get<int>();
    string name=js["groupname"];
    string desc=js["groupdesc"];

    Group group(-1,name,desc);
    if(_groupModel.createGroup(group))
    {
        _groupModel.addGroup(userid,group.getId(),"creator");
    }
}
void ChatService::addGroup(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int userid=js["id"].get<int>();
    int groupid=js["groupid"].get<int>();
    _groupModel.addGroup(userid,groupid,"normal");
}
void ChatService::groupChat(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int userid=js["id"].get<int>();
    int groupid=js["groupid"].get<int>();
    vector<int> useridVec=_groupModel.queryGroupUsers(userid,groupid);
    
    unique_lock<mutex> lock(_connMapMtx);
    for(int id:useridVec)
    {
        auto it=_userConnMap.find(id);
        if(it!=_userConnMap.end())
        {
            it->second->send(js.dump());
        }
        else
        {

            User user=_userModel.query(id);
            if(user.getState()=="online")
            {
                _redis.publish(id,js.dump());
            }
            else
            {
                _offlineMsgModel.insert(id,js.dump());
            }
        }
    }
}

void ChatService::logout(const TcpConnectionPtr &conn,json &js,Timestamp time)
{
    int userid=js["id"].get<int>();
    {
        unique_lock<mutex> lock(_connMapMtx);
        auto it=_userConnMap.find(userid);
        if(it!=_userConnMap.end())
        {
            _userConnMap.erase(it);
        }
    }

    _redis.unsubscribe(userid);

    User user(userid,"","","offline");
    _userModel.updateState(user);
}
void ChatService::handleRedisSubscribeMessage(int userid,string msg)
{
    json js=json::parse(msg.c_str());

    unique_lock<mutex> lock(_connMapMtx);
    auto it=_userConnMap.find(userid);
    if(it!=_userConnMap.end())
    {
        it->second->send(js.dump());
        return;
    }

    _offlineMsgModel.insert(userid,msg);


}