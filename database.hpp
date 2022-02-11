#pragma once
#include<iostream>
#include<string>
#include<jsoncpp/json/json.h>
#include<mysql/mysql.h>
#include"room_player.hpp"
using namespace std;

#define JUDGE_VALUE(p) ((p != NULL) ? p : "")

class DataBaseSvr
{
    public:
        DataBaseSvr()
        {
            //初始化mysql操作句柄
            mysql_init(&mysql_);
        }
        ~DataBaseSvr()
        {
            //关闭连接
            mysql_close(&mysql_);
        }

        bool Connect2Mysql()
        {
            //连接mysql服务端
            //MYSQL *     STDCALL mysql_real_connect(MYSQL *mysql, const char *host,                  //                        const char *user,             
            //                        const char *passwd,                 
            //                        const char *db,                            
            //                        unsigned int port,              
            //                        const char *unix_socket,                                       //                        unsigned long clientflag); 
            //if(!mysql_real_connect(&mysql_,"121.4.219.195","HanMeng","949198901","web-gobang",3306,NULL,CLIENT_FOUND_ROWS))
            if(!mysql_real_connect(&mysql_, "121.5.41.216", "lipan", "123456", "webgobang", 3306, NULL, CLIENT_FOUND_ROWS))
            {
                return false;
            }
            return true;
        }

        //注册校验的接口
        //email和password
        //返回值：成功：返回用户id  失败：负数
        int QueryUserExit(Json::Value& v)
        {
            //1.组织sql语句
            string email = v["email"].asString();
            string password = v["password"].asString();
#define GET_USER_INFO "select * from sys_user where email=\'%s\';"
            char sql[1024] = {0};
            snprintf(sql, sizeof(sql) - 1, GET_USER_INFO, email.c_str());
            cout <<"sql: " << sql << endl;
            //2.查询
            MYSQL_RES* res = NULL;
            if(ExecuteSql(sql, &res) == false)
            {
                return -2;
            }

            //3.针对查询的结果进行处理
            //判断结果集当中的行数：my_ulonglong STDCALL mysql_num_rows(MYSQL_RES *res);
            if(mysql_num_rows(res) != 1)
            {
                printf("No Data: sql is %s\n", sql);
                return -3;
            }

            //获取结果集当中的一行数据， 通过返回值返回：MYSQL_ROW   STDCALL mysql_fetch_row(MYSQL_RES *result);
            MYSQL_ROW row = mysql_fetch_row(res);
            //row[4] : 对应表记录当中的password
            string passwd_db = JUDGE_VALUE(row[4]);

            if(password != passwd_db)
            {
                return -4;
            }

            //4.释放结果集
            mysql_free_result(res); 
            //返回用户ID
            return atoi(row[0]);
        }


        int InsertRoomInfo(const Room& r)
        {
            
             // 想要插入对局当中所有落子信息
             // 需要使用到事务

            if(ExecuteSql("start transaction;") == false)
            {
                return -1;
            }

            if(ExecuteSql("savepoint start_insert;") == false)
            {
                return -2;
            }

#define INSERT_ROOM_INFO "insert into game_info values(%d, %d, %d, %d, %d, \'%s\');"
            for(size_t i = 0; i < r.step_vec_.size(); i++)
            {
                char sql[1024] = {0};
                snprintf(sql, sizeof(sql) - 1, INSERT_ROOM_INFO, r.room_id_, r.p1_,
                        r.p2_, r.winner_, r.p1_, r.step_vec_[i].c_str());

                if(ExecuteSql(sql) == false)
                {
                    ExecuteSql("rollback to start_insert;");
                    ExecuteSql("commit;");
                    return -3;
                }
            }

            ExecuteSql("commit;");
            return 0;
        }
        //执行sql语句，并将结果放到结果集当中
        bool ExecuteSql(const string& sql, MYSQL_RES** res)
        {
            //1.设置当前客户端的字符集
            mysql_query(&mysql_, "set names utf8");

            //等于0， 则表示调用成功， 非0，则表示错误
            if(mysql_query(&mysql_, sql.c_str()) != 0)
            {
                return false;
            }

            //mysql执行的结果都是放在结果集当中  mysql_store_result
            //MYSQL_RES * STDCALL mysql_store_result(MYSQL *mysql);
            *res = mysql_store_result(&mysql_);
            if(res == NULL)
            {
                return false;
            }
            return true;
        }


        bool ExecuteSql(const string& sql)
        {
            //1.设置当前客户端的字符集
            mysql_query(&mysql_, "set names utf8");

            //等于0， 则表示调用成功， 非0，则表示错误
            if(mysql_query(&mysql_, sql.c_str()) != 0)
            {
                return false;
            }
            return true;
        }

        //向数据库中添加用户注册的信息
        bool AddUser(Json::Value& v)
        {
            string name = v["name"].asString();
            string password = v["passwd"].asString();
            string email = v["email"].asString();
            string phone_num = v["phonenum"].asString(); 
#define ADD_USER "insert into sys_user(name, email, phone_num, passwd) values(\'%s\', \'%s\', \'%s\', \'%s\');"
            char sql[1024] = {0};
            snprintf(sql, sizeof(sql) - 1, ADD_USER, name.c_str(),
                    email.c_str(), phone_num.c_str(), password.c_str());
            return ExecuteSql(sql);
        }
    private:
        MYSQL mysql_;
};
