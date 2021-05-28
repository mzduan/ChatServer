#ifndef USERMODEL_H
#define USERMODEL_H

#include "user.hpp"
class UserModel
{
public:
    bool insert(User &user);
    User query(int id);
    bool updateState(const User &user);
    void resetState();
};




#endif