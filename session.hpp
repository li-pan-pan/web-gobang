#pragma once
#include <openssl/md5.h>
#include<iostream>
#include<string>
#include<string.h>
#include<unordered_map>

#include <jsoncpp/json/json.h>
#include "httplib.h"
#include "tools.hpp"

using namespace std;
using namespace httplib;

class Session
{
    public:
        Session()
        {

        }

        Session(Json::Value& v, int user_id)
        {
            real_str_.clear();

            real_str_ += v["email"].asString();
            real_str_ += v["password"].asString();

            user_id_ = user_id;
        }

        ~Session()
        {

        }

        bool SumMd5()
        {
            MD5_CTX ctx;
            //1.初始化
            MD5_Init(&ctx);

            if(MD5_Update(&ctx, real_str_.c_str(), real_str_.size()) != 1)
            {
                return false;
            }

            unsigned char md5[16] = {0};
            if(MD5_Final(md5, &ctx) != 1)
            {
                return false;
            }

            char tmp[3] = {0};
            char buf[32] = {0};
            for(int i = 0; i < 16; i++)
            {
                snprintf(tmp, sizeof(tmp) - 1, "%02x", md5[i]);
                //printf("tem：%s", tem);
                strncat(buf, tmp, 2);
            }
            //printf("\n");

            session_id_ = buf;
            return true;
        }

        //获取会话id
        string& GetSessionId()
        {
            SumMd5();
            return session_id_;
        }
    public:
        //当前会话的会话id
        string session_id_;
        //生成会话id的原生字符串
        string real_str_;

        int user_id_;
};

class AllSessionInfo
{
    public:
        AllSessionInfo()
        {
            session_map_.clear();
            pthread_mutex_init(&map_lock_, NULL);
        }

        ~AllSessionInfo()
        {
            pthread_mutex_destroy(&map_lock_);
        }

        //设置会话信息
        void SetSessionInfo(string& session_id, Session& sess)
        {
            pthread_mutex_lock(&map_lock_);
            session_map_.insert(make_pair(session_id, sess));
            pthread_mutex_unlock(&map_lock_);
        }

        //获取查找会话信息的session id
        int GetSessionInfo(const string& session_id, Session* session_info)
        {

            pthread_mutex_lock(&map_lock_);
            auto iter = session_map_.find(session_id);
            if(iter == session_map_.end())
            {
                pthread_mutex_unlock(&map_lock_);
                return -1;
            }

            if(session_info != nullptr)
            {
                *session_info = iter->second;
            }
            pthread_mutex_unlock(&map_lock_);
            return 0;
        }
        //判断登录会话ID是正常的还是非法的
        int CheckSessionInfo(const Request& req)
        {
            string session_id = GetSessionId(req);
            if(session_id == "")
            {
                return -1;
            }

            Session sess;
            if(GetSessionInfo(session_id, &sess) < 0)
            {
                return -2;
            }

            //如何最终会话校验成功了， 则返回该用户的用户id
            return sess.user_id_;
        }


        //获取会话ID
        string GetSessionId(const Request& req)
        {
            string session_kv = req.get_header_value("Cookie");

            //JSESSIONID=xxxxx
            //JSESSIONID=
            vector<string> v;
            //切割字符串
            StringTools::Split(session_kv, "=", &v);
            return v[v.size() - 1];
        }

    private:
        unordered_map<string,Session> session_map_;
        pthread_mutex_t map_lock_;
};
