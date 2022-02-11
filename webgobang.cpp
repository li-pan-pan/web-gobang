#include"webgobang.hpp"


int main()
{
    WebGobang* wg=new WebGobang();
    if(wg==NULL)
    {
        printf("init web gobang failed\n");
        return 0;
    }

    if(wg->Init()<0)
    {
        return 0;
    }

    wg->StartWebGobang();
    delete wg;
    return 0;
}
