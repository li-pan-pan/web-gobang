#include<iostream>
#include<string>
#include<jsoncpp/json/json.h>
#include"httplib.h"
#include"database.hpp"
#include"session.hpp"
#include"room_player.hpp"

using namespace httplib;
using namespace std;

class WebGobang
{
    public:
        WebGobang()
        {
            db_svr_ = NULL;

            all_sess_ = NULL;

            pm_ = NULL;

            rm_ = NULL;

            match_pool_.clear();
            match_pool_num_ = 0;
            pthread_mutex_init(&vec_lock_,NULL);
            pthread_cond_init(&vec_cond_,NULL);
        }
        ~WebGobang()
        {
            if(db_svr_)
            {
                delete db_svr_;
                db_svr_ = NULL;
            }

            if(all_sess_)
            {
                delete all_sess_;
                all_sess_ = NULL;
            }

            if(pm_)
            {
                delete pm_;
                pm_ = NULL;
            }

            if(rm_)
            {
                delete rm_;
                rm_ = NULL;
            }
            pthread_mutex_destroy(&vec_lock_);
            pthread_cond_destroy(&vec_cond_);
        }
        //初始化用户管理模块的类指针
        //初始化数据库管理模块的类指针
        int Init()
        {
            db_svr_ = new DataBaseSvr();
            if(db_svr_ == NULL)
            {
                return -1;
            }
            if(db_svr_->Connect2Mysql() == false)
            {
                printf("connect to mysql falied\n");
                return -2;
            }

            all_sess_ = new AllSessionInfo();
            if(all_sess_ == NULL)
            {
                return -3;
            }

            pm_ = new PlayerManager();
            if(pm_ == NULL)
            {
                return -4;
            }

            rm_ = new RoomManager();
            if(rm_ == NULL)
            {
                return -5;
            }
            return 0;
        }
        //定义HTTP能接收的请求方法
        void StartWebGobang()
        {
            http_svr_.Post("/register",[this](const Request& res,Response& resp){
                    Json::Reader r;
                    Json::Value v;
                    r.parse(res.body,v);

                    Json::Value resp_json;
                    resp_json["status"] = this->db_svr_->AddUser(v);

                    Json::FastWriter w;
                    resp.body = w.write(resp_json);
                    resp.set_header("Content-Type","application/json");

                    });

            http_svr_.Post("/login",[this](const Request& res,Response& resp){
                    //1.校验用户提交的账号和密码(同数据库中的数据进行校验)
                    Json::Reader r;
                    Json::Value v;
                    r.parse(res.body,v);

                    int user_id = this->db_svr_->QueryUserExit(v);
                    //2.组织http响应进行恢复(json)

                    string tmp = "";
                    if(user_id > 0)
                    {
                    Session sess(v,user_id);
                    string session_id = sess.GetSessionId();
                    tmp = "JSESSIONID=" + session_id;

                    all_sess_->SetSessionInfo(session_id,sess);

                    //放到用户管理类当中，进行管理
                    this->pm_->InsertPlayer2Map(user_id);
                    }

                    Json::Value resp_json;
                    resp_json["status"] = user_id <= 0 ? false : true;
                    resp.body = this->Serializa(resp_json);
                    resp.set_header("Set-Cookie",tmp.c_str());
                    resp.set_header("Content-Type","application/json");
            });

            http_svr_.Get("/GetUserId",[this](const Request& res,Response& resp){
                    //1.会话校验
                    Json::Value resp_json;
                    resp_json["user_id"] = this->all_sess_->CheckSessionInfo(res);

                    resp.body = this->Serializa(resp_json);
                    resp.set_header("Content-Type","application/json");
                    });
            //将当前这个用户放到匹配池当中
            http_svr_.Get("/SetMatch", [this](const Request& res, Response& resp){
                    Json::Value resp_json;
                    int user_id = this->all_sess_->CheckSessionInfo(res); 
                    if(user_id <= 0)
                    {
                    resp_json["status"] = -1;
                    }
                    else
                    {
                    //将当前的用户放到匹配池当中
                    resp_json["status"] = this->PushPlayer2MatchPool(user_id);
                    }

                    resp.body = this->Serializa(resp_json);
                    resp.set_header("Content-Type", "application/json");
                    });

            http_svr_.Get("/Match", [this](const Request& res, Response& resp){
                    Json::Value resp_json;
                    int user_id = this->all_sess_->CheckSessionInfo(res); 
                    if(user_id <= 0)
                    {
                    resp_json["status"] = -1;
                    }
                    else
                    {
                    //使用用户id， 判断是否该用户有房间号， 
                    //       如果有房间号， 则配成功
                    //       如果没有， 则匹配失败

                    //1.获取当前这个用户， 
                    //       1.1 想要获取用户持有的棋子
                    //       1.2 想要获取用户所在的房间号

                    Player r = this->pm_->GetPlayerInfo(user_id);
                    if(r.room_id_ >= 1000 && r.player_status_ == PLAYING)
                    {
                    //匹配成功
                        resp_json["status"] = 1;
                        resp_json["room_id"] = r.room_id_;
                        resp_json["chess_name"] = r.chess_name_;
                    }
                    else
                    {
                        //没有匹配成功
                        resp_json["status"] = 0;
                    }

                    }
                    resp.body = this->Serializa(resp_json);
                    resp.set_header("Content-Type", "application/json");
            });

            http_svr_.Post("/IsMyTurn", [this](const Request& res, Response& resp)
                    {
                    Json::Value resp_json;
                    int user_id = this->all_sess_->CheckSessionInfo(res); 
                    if(user_id <= 0)
                    {
                    resp_json["status"] = -1;
                    }
                    else
                    {
                    Json::Reader r;
                    Json::Value v;
                    r.parse(res.body, v);

                    int room_id = v["room_id"].asInt();
                    int user_id = v["user_id"].asInt();

                    resp_json["status"] = this->rm_->IsMyTurn(room_id, user_id);
                    }

                    resp.body = this->Serializa(resp_json);
                    resp.set_header("Content-Type", "application/json");
                    });

            http_svr_.Post("/Step", [this](const Request& res, Response& resp)
                    {
                    Json::Value resp_json;
                    int user_id = this->all_sess_->CheckSessionInfo(res); 
                    if(user_id <= 0)
                    {
                    resp_json["status"] = -1;
                    }
                    else
                    {
                    resp_json["status"] = this->rm_->Step(res.body);
                    }
                    resp.body = this->Serializa(resp_json);
                    resp.set_header("Content-Type", "application/json");
                    });

            http_svr_.Post("/GetPeerStep", [this](const Request& res, Response& resp)
                    {
                        Json::Value resp_json;
                        int user_id = this->all_sess_->CheckSessionInfo(res); 
                    if(user_id <= 0)
                    {
                        resp_json["status"] = -1;
                    }
                    else
                    {
                        string peer_step;
                        if(this->rm_->GetRoomStep(res.body, peer_step) < 0)
                        {
                            resp_json["status"] = 0;
                        }
                        else
                        {
                            resp_json["status"] = 1;
                            resp_json["peer_step"] = peer_step;
                        }
                    }
                    resp.body = this->Serializa(resp_json);
                    resp.set_header("Content-Type", "application/json");
                    });

            http_svr_.Post("/Winner", [this](const Request& res, Response& resp)
                    {
                        Json::Value resp_json;
                        int user_id = this->all_sess_->CheckSessionInfo(res); 
                        if(user_id <= 0)
                        {
                            resp_json["status"] = -1;
                        }
                        else
                        {
                            Json::Reader r;
                            Json::Value v;
                            r.parse(res.body, v);

                            int room_id = v["room_id"].asInt();
                            int user_id = v["user_id"].asInt();

                            //1.设置对局的赢家到对应的房间
                            this->rm_->SetRoomWinner(room_id, user_id);
                            //2.将对局的步骤保存在数据库当中（持久化）
                                 //2.1 获取当前房间信息
                                 //2.2 将房间信息交给数据库模块继续持久化
                            resp_json["status"] = this->db_svr_->InsertRoomInfo(this->rm_->GetRoomInfo(room_id));
                        }
                        resp.body = this->Serializa(resp_json);
                        resp.set_header("Content-Type", "application/json");
                    });

            http_svr_.Post("/Restart", [this](const Request& res, Response& resp)
                    {
                        Json::Value resp_json;
                        int user_id = this->all_sess_->CheckSessionInfo(res); 
                        if(user_id <= 0)
                        {
                            resp_json["status"] = -1;
                        }
                        else
                        {
                            Json::Reader r;
                            Json::Value v;
                            r.parse(res.body, v);

                            int room_id = v["room_id"].asInt();
                            int user_id = v["user_id"].asInt();
                            //1.将用户信息进行重置， 用户状态， 游戏房间等信息
                            this->pm_->ResetUserGameInfo(user_id);
                            //2.将用户所在房间及进行移除
                            this->rm_->RemoveRoom(room_id);

                            resp_json["status"] = 0;
                        }
                        resp.body = this->Serializa(resp_json);
                        resp.set_header("Content-Type", "application/json");
                    });
            pthread_t tid;
            int ret = pthread_create(&tid, NULL, MatchServer, (void*)this);
            if(ret < 0)
            {
                printf("create match thread faield\n");
                exit(1);
            }

            http_svr_.set_mount_point("/","./www");
            http_svr_.listen("0.0.0.0",37878);
        }

        static void* MatchServer(void* arg)
        {

            pthread_detach(pthread_self());
            WebGobang* wg = (WebGobang*)arg;

            while(1)
            {
                pthread_mutex_lock(&wg->vec_lock_);
                while(wg->match_pool_num_ < 2)
                {
                    pthread_cond_wait(&wg->vec_cond_, &wg->vec_lock_);
                    printf("i am MatchServer, i working\n");
                }

                //匹配池当中的人数一定为大于等于2
                //  奇数： 一定由一个玩家要轮空
                //  偶数：两两进行匹配
                int last_id = -1;
                auto& vec = wg->match_pool_;
                size_t vec_size = vec.size();
                if(vec_size % 2)
                {
                    //当前人数是奇数
                    last_id = vec[vec_size - 1];
                    vec_size -= 1;
                }

                for(int i = vec_size - 1; i >= 0; i -= 2)
                {
                    int player_one = vec[i];
                    int player_two = vec[i - 1];

                    //假设匹配成功
                    int room_id = wg->rm_->CreateRoom(player_one, player_two);

                    //当前匹配上的这两个用户的状态设置成为PLAYING
                    wg->pm_->SetUserStatus(player_one, PLAYING);
                    wg->pm_->SetUserStatus(player_two, PLAYING);

                    wg->pm_->SetRoomId(player_one, room_id);
                    wg->pm_->SetRoomId(player_two, room_id);

                    /*
                     * 给用户分配黑棋还是白棋
                     * */
                    wg->pm_->SetUserChessName(player_one, "黑棋");
                    wg->pm_->SetUserChessName(player_two, "白棋");

                    cout << "player_one: " << player_one << ", player_two:" << player_two << ", room_id: "<< room_id <<  endl;
                }

                wg->MatchPoolClear();
                pthread_mutex_unlock(&wg->vec_lock_);

                if(last_id > 0)
                {
                    wg->PushPlayer2MatchPool(last_id);
                }
            }
            return NULL;
        }

        //清空匹配池，若匹配人数为奇数，保留未匹配人的id
        void MatchPoolClear()
        {
            match_pool_.clear();
            match_pool_num_ = 0;

        }

        string Serializa(Json::Value& v)
        {
            Json::FastWriter w;
            return w.write(v);
        }

        int PushPlayer2MatchPool(int user_id)
        {
            //1.设置用户的状态为MATCHING状态
            pm_->SetUserStatus(user_id, MATCHING);
            //2.放到匹配池当中
            pthread_mutex_lock(&vec_lock_);
            match_pool_.push_back(user_id);
            match_pool_num_++;
            printf("match_pool_num_: %d\n", match_pool_num_);
            pthread_mutex_unlock(&vec_lock_);

            //通知匹配线程工作
            pthread_cond_broadcast(&vec_cond_);
            return 0;
        }
    private:
        Server http_svr_;

        DataBaseSvr* db_svr_;

        AllSessionInfo* all_sess_;

        PlayerManager* pm_;

        RoomManager* rm_;

        //匹配池：vector  需要匹配之后开始游戏的用户，匹配线程就是通过该匹配池进行匹配
        vector<int> match_pool_;
        int match_pool_num_;
        pthread_mutex_t vec_lock_;
        pthread_cond_t vec_cond_;
};
