#pragma once
#include <iostream>
#include <string>
#include <unordered_map>

#include <jsoncpp/json/json.h>

using namespace std;

//玩家状态
typedef enum PlayerStatus
{
    ONLINE = 0,
    MATCHING,
    PLAYING
}status_t;

class Player
{
    public:
        Player()
        {

        }
        Player(int user_id)
        {
            user_id_ = user_id;
            //刚登陆上来房间号不能确定
            room_id_ = -1;
            player_status_ = ONLINE;
            chess_name_.clear();
        }
        ~Player()
        {

        }
    public:
        //用户ID
        int user_id_;
        //游戏房间号
        int room_id_;
        //玩家状态
        status_t player_status_;
        //棋的颜色
        string chess_name_;
};

//玩家管理类
class PlayerManager
{
    public:
        PlayerManager()
        {
            player_map_.clear();
            pthread_mutex_init(&map_lock_,NULL);
        }

        ~PlayerManager()
        {
            pthread_mutex_destroy(&map_lock_);
        }

         //当用户登录之后， 将登录的用户保存在map当中         
        void InsertPlayer2Map(int user_id)
        {
            pthread_mutex_lock(&map_lock_);
            Player p(user_id);
            player_map_.insert({user_id, p});
            pthread_mutex_unlock(&map_lock_);
        }

        //设置当前用户状态
        void SetUserStatus(int user_id,status_t sta)
        {
            player_map_[user_id].player_status_ = sta;
        }

        //设置玩家的房间号
        void SetRoomId(int user_id, int room_id)
        {
            player_map_[user_id].room_id_ = room_id;
        }

        //获取用户ID
        Player& GetPlayerInfo(int user_id)
        {
            return player_map_[user_id];
        }
        //设置用户棋子颜色
        void SetUserChessName(int user_id, const string chess_name)
        {
            player_map_[user_id].chess_name_ = chess_name;
        }

        //重置用户信息
        void ResetUserGameInfo(int user_id)
        {
            player_map_[user_id].chess_name_ = "";
            player_map_[user_id].room_id_ = -1;
            player_map_[user_id].player_status_ = MATCHING;
        }
    private:
        unordered_map<int,Player> player_map_;
        pthread_mutex_t map_lock_;
};

class Room
{
    public:
        Room()
        {

        }

        Room(int p1, int p2, int room_id)
        {
            p1_= p1;
            p2_ = p2;

            //接下来是该谁走了， 黑棋先走
            who_turn_ = p1;
            room_id_ = room_id;

            step_num_ = 0;

            step_vec_.clear();

            winner_ = -1;
        }

        ~Room()
        {

        }
        //判断是否是自己的轮回
        int IsMyTurn(int user_id)
        {
            return user_id == who_turn_ ? 1 : 0;
        }

        int Step(int user_id, const string& body)
        {
            if(user_id != who_turn_)
            {
                return 0;
            }

            step_vec_.push_back(body);
            who_turn_ = (user_id == p1_ ? p2_ : p1_);
            step_num_++;

            return 1;
        }

        int GetPeerStep(int user_id, string& s)
        {
            if(step_num_ <= 0)
            {
                return -1;
            }

            if(user_id != who_turn_)
            {
                return -2;
            }

            s = step_vec_[step_num_ - 1];
            return 0;
        }
        void SetWinner(int user_id)
        {
            winner_ = user_id;
        }

        vector<string>&  GetRoomStepInfo()
        {
            return step_vec_;
        }
    public:
        int p1_;
        int p2_;

        //记录当前是谁的轮回
        int who_turn_;
        int step_num_;
        int room_id_;

        //保存用户走的每一个步骤
        vector<string> step_vec_;

        int winner_;
};

class RoomManager
{
     public:
        RoomManager()
        {
            map_room_.clear();
            pthread_mutex_init(&room_lock_, NULL);

            prepare_id_ = 10000;
        }

        ~RoomManager()
        {
            pthread_mutex_destroy(&room_lock_);
        }

        int CreateRoom(int p1, int p2)
        {
            pthread_mutex_lock(&room_lock_);
            int room_id = prepare_id_++;
            Room r(p1, p2, room_id);
            map_room_.insert(make_pair(room_id, r));
            pthread_mutex_unlock(&room_lock_);

            return room_id;
        }

        int IsMyTurn(const int room_id, const int user_id)
        {
            return map_room_[room_id].IsMyTurn(user_id);
        }

        int Step(const string& body)
        {
            Json::Reader r;
            Json::Value v;
            r.parse(body, v);

            int room_id = v["room_id"].asInt();
            int user_id = v["user_id"].asInt();
            return map_room_[room_id].Step(user_id, body);
        }

        int GetRoomStep(const string& body, string& s)
        {
            Json::Reader r;
            Json::Value v;
            r.parse(body, v);

            int room_id = v["room_id"].asInt();
            int user_id = v["user_id"].asInt();
            return map_room_[room_id].GetPeerStep(user_id, s);
        }

        void SetRoomWinner(const int room_id, const int user_id)
        {
            map_room_[room_id].SetWinner(user_id);
        }

        Room& GetRoomInfo(int room_id)
        {
            return map_room_[room_id];
        }

        //移除房间信息
        int RemoveRoom(int room_id)
        {
            pthread_mutex_lock(&room_lock_);
            auto iter = map_room_.find(room_id);
            if(iter == map_room_.end())
            {
                pthread_mutex_unlock(&room_lock_);
                return -1;
            }

            map_room_.erase(iter);
            pthread_mutex_unlock(&room_lock_);
            return 0;
        }
    private:
        unordered_map<int, Room> map_room_;
        pthread_mutex_t room_lock_;

        //预分配的房间ID
        int prepare_id_;
};
