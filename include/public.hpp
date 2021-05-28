#ifndef PUBLIC_H
#define PUBLIC_H

enum EnMsgType
{
    LOGIN_MSG=1,
    REG_MSG,
    LOGINOUT_MSG, // 注销消息
    REG_MSG_ACK, //注册响应消息
    LOGIN_MSG_ACK, //登录响应
    ONE_CHAT_MSG, //聊天消息
    ADD_FRIEND_MSG, //添加好友的消息

    CREATE_GROUP_MSG,
    ADD_GROUP_MSG,
    GROUP_CHAT_MSG
};


#endif