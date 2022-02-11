#pragma once
#include <iostream>
#include <string.h>
#include <vector>

#include <boost/algorithm/string.hpp>

using namespace std;



class StringTools
{
    public:
        static void Split(const string& input, const string& split_char, vector<string>* output)
        {
              //存放切割完毕的字符串
              //待要切割的字符串
              //boost::is_any_of : 设定切割的分隔符（不仅仅支持按照字符切割， 也支持按照字符串切割）
              //boost::token_compress_off : 将多个分隔符按照多个计算  eg:abc=1----->abc = 1    
              //boost::token_compress_on : 将多个分隔符按照一个计算  eg: abc==1---->abc = "" = 1
             
            boost::split(*output, input, boost::is_any_of(split_char), boost::token_compress_off);
        }
};
