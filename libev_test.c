#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>
#include <ev.h>     //from lib.

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>

#include "global.h"
#include "RFID_handler.h"
#include "list.h"
#include "fd_list.h"

#define MAXLEN 1023

unsigned int topo_buffer[256*18+3];
char phone_buf[11]={0};
unsigned int sensor=0;
int sockfd;
struct timeval tv;
fd_set sds;
static int flag_bind=0;
int qt_socket_fd = 0;

bool Server_GetZigBeeNwkInfo(int fd);

bool Server_GetZigBeeNwkTopo(int fd);
bool Server_GetTempHum(int fd);

bool Server_SetSensorStatus(int fd,unsigned int addr,unsigned int state);
bool Server_ErrorFeedback(int ,unsigned int);//by @wei
int Server_GetRfidId(int fd);
int Server_GetGPRSSignal(int fd);
bool Server_SendGprsMessage(int fd,unsigned int *phone,unsigned int sensor);
int socket_init(char** argv);
void accept_callback(struct ev_loop *loop, ev_io *w, int revents);
void recv_callback(struct ev_loop *loop, ev_io *w, int revents);
void write_callback(struct ev_loop *loop, ev_io *w, int revents);
void gprs_send();

void init_int_queue(PUINT_QUEUE pQ)
{
    pQ->head = 0;
    pQ->tail = 0;
    //pQ->lock =  SPIN_LOCK_UNLOCKED;
    pthread_mutex_init(&pQ->lock, NULL);
}
void enqueue_int_queue(PUINT_QUEUE pQ, unsigned int c)
{
    pthread_mutex_lock(&pQ->lock);
    UPDBG("________________enqueue_int_queue: %d\n",c);
    if((pQ->head + 1) % UINT_QUEUE_SIZE == pQ->tail) // queue full
    {
        pQ->tail = (pQ->tail + 1 ) % UINT_QUEUE_SIZE;
    }
    pQ->data[pQ->head] = c;
    pQ->head = (pQ->head + 1 ) % UINT_QUEUE_SIZE;
    pthread_mutex_unlock(&pQ->lock);
    return;
}
int main(int argc ,char** argv)
{
    int listen;
    ev_io ev_io_watcher;


    argv[1]="7838";
    argv[2]="9";
    UPDBG("flag_bind%d",flag_bind);
    signal(SIGPIPE,SIG_IGN);
    UPDBG("start com monitor\n");  
    listen=socket_init(argv);
    UPDBG("flag_bind%d",flag_bind);
    if(flag_bind)
    {

        ComPthreadMonitorStart();

    }
    init_sqlite_RFID();
    list_fd_init();

    init_int_queue(&pQueue);

    //struct ev_loop *loop = ev_loop_new(EVBACKEND_EPOLL);
    struct ev_loop *loop = ev_default_loop(EVBACKEND_EPOLL);
    ev_io_init(&ev_io_watcher, accept_callback,listen, EV_READ);
    ev_io_start(loop,&ev_io_watcher); 
    ev_loop(loop,0);
        ev_loop_destroy(loop);
        if(flag_bind)
        {

            ComPthreadMonitorExit();

        }

        UPDBG("exit com monitor\n");

    return 0;

}

int socket_init(char** argv)
{

    struct sockaddr_in my_addr;
    int listener;
    //socklen_t len;
    unsigned int myport, lisnum ;
    
    if (argv[1])
    {
        UPDBG("argv1%s\n",argv[1]);
        myport = atoi(argv[1]);
    }
    else
        myport = 7838;

    if (argv[2])
    {   
        UPDBG("argv2%s\n",argv[2]);
        lisnum = atoi(argv[2]);
    }
    else
        lisnum = 2;
    //int socket(int domain, int type, int protocol);creates  an endpoint for communication and returns a descrip-tor.
    /*The backlog argument provides a hint to the implementation which 
    the implementation shall use to limit the number of outstanding 
    connections in the socket's listen queue
    backlog²ÎÊýÌá¹©Ò»¸öimplementationµÄÏßË÷,implementationÓ¦¸ÃÓÃ(Ëü)À´ÏÞÖÆÎ´¾öµÄÁ¬½Ó
    ÊýÁ¿,(ÕâÐ©Á¬½Ó(ÇëÇó)ÔÚ listen¶ÓÁÐÀï)
    */
    if ((listener = socket(PF_INET, SOCK_STREAM, 0)) == -1) 
    {
                flag_bind=0;
                perror("socket");
        exit(1);
    } 
    else
    {
            UPDBG("flag_bind=1\n");
                flag_bind=1;
        UPDBG("SOCKET CREATE SUCCESS!\n");
    }
    //setnonblocking(listener);
    int so_reuseaddr=1;
    /* getsockopt()   and  setsockopt()  manipulate  options  for  the  socket
       referred to by the file descriptor sockfd.  Options may exist at multi-
       ple  protocol  levels;  they are always present at the uppermost socket
       level.int sockfd, int level, int optname, const void *optval, socklen_t optlen*/
    setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&so_reuseaddr,sizeof(so_reuseaddr));
    bzero(&my_addr, sizeof(my_addr));
    my_addr.sin_family = PF_INET;
    my_addr.sin_port = htons(myport);
    if(argv[3])
        my_addr.sin_addr.s_addr = inet_addr(argv[3]);
    else 
        my_addr.sin_addr.s_addr = INADDR_ANY;


    if (bind(listener, (struct sockaddr *) &my_addr, sizeof(struct sockaddr))== -1) 
    {
        perror("bind error!\n");
        exit(1);
    } 
    else
    {
        UPDBG("BIND SUCCESS\n");
        //UPDBG("IP BIND SUCCESS,IP:%s\n",my_addr.sin_addr.s_addr);
    }

    if (listen(listener, lisnum) == -1) 
    {
        perror("listen error!\n");
        exit(1);
    } 

    else
    {
        UPDBG("LISTEN SUCCESS,PORT:%d\n",myport);
    }
    return listener;
}

#define QT_CLIENT_ADDR "10.0.136.142"

//extern struct list_head head;
void accept_callback(struct ev_loop *loop, ev_io *w, int revents)
{
    int newfd;
    struct sockaddr_in their_addr;
    socklen_t addrlen = sizeof(struct sockaddr);
    ev_io* accept_watcher=malloc(sizeof(ev_io));
    ev_io* write_watcher = malloc(sizeof(ev_io));//@add by wei

    while ((newfd = accept(w->fd, (struct sockaddr *)&their_addr, &addrlen)) < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
        //these are transient, so don't log anything.
            continue; 
        }
        else
        {
            UPDBG("accept error.[%s]\n", strerror(errno));
            break;
        }
    }
    Udbg("Got connection from %s\n",inet_ntoa(their_addr.sin_addr));
    if(!(int)strcmp(inet_ntoa(their_addr.sin_addr),QT_CLIENT_ADDR)){
        qt_fd = w->fd;
        qt_socket_fd = newfd;
        Udbg("get fd from qt %d\n",qt_fd);
    }else{
        plat_other_fd = w->fd;
        add_fd(plat_other_fd,newfd);

        reg_fd_list_t *entry;
        list_for_each(iterator,&head)//遍历  
        {
            entry = list_entry(iterator,reg_fd_list_t,node);//读取某个值  
            printf("socketfd :%d\n",entry->socketfd);
            ULdbg();
            printf("head addr is %08x\n",iterator);
        }
        //skimfd();
    }

    //@wei If connection came from others expcept for qt client.
    /*if(strcmp(their_addr.sin_addr,QT_CLIENT_ADDR))
    {
        ev_io_init(accept_watcher,android_callback,newfd,EV_READ);
        ev_io_start(loop,accept_watcher);
        UPDBG("android callback : fd :%d\n",accept_watcher->fd);
    }else{*/
    ev_io_init(accept_watcher,recv_callback,newfd,EV_READ);
    //ev_io_start(loop,accept_watcher);
    ev_io_start(loop,accept_watcher);

    ev_io_init(write_watcher,write_callback,newfd,EV_WRITE);
    ev_io_start(loop,write_watcher);
    UPDBG("accept callback : fd :%d\n",accept_watcher->fd);        
    


}
/**create by wei
*
*/
void android_callback(struct ev_loop *loop, ev_io *w, int revents){
    ULdbg();
    //ecv_callback(loop, w, revents);
    unsigned int buffer[15]={0};
    unsigned int temp;
    int i=0;
    tv.tv_sec=60;
    tv.tv_usec=0;
    FD_ZERO(&sds);
    FD_SET(w->fd, &sds);
    /*int select(int nfds, fd_set *readfds, fd_set *writefds,
              fd_set *exceptfds, struct timeval *timeout);*/
    int ret = select((1+w->fd), &sds, NULL, NULL, &tv);
}



void recv_callback(struct ev_loop *loop, ev_io *w, int revents)
{
    //UPDBG("\nrecv_callback:\n");
    //@wei indentify of sending message successfully.
    if(gSend==1)
    {
        gSend=0;
        unsigned int buffer[4]={0};
        buffer[0]=0x26;
        buffer[1]=0x06;

        buffer[3]='\n';

        buffer[2]=1;
        write(w->fd, (unsigned int*)(&buffer), sizeof(buffer));

    }
    unsigned int buffer[15]={0};
    unsigned int temp;
    int i=0,j;
    tv.tv_sec=60;
    tv.tv_usec=0;
    FD_ZERO(&sds);
    FD_SET(w->fd, &sds);
    /*int select(int nfds, fd_set *readfds, fd_set *writefds,
              fd_set *exceptfds, struct timeval *timeout);*/
    int ret = select((1+w->fd), &sds, NULL, NULL, &tv);
    if(ret >0)
    {
        UPDBG("server select wait...\n");
        if (FD_ISSET(w->fd, &sds))
        {
            UPDBG("server FD_ISSET...\n");
            int len=0;
            len=recv(w->fd,&buffer[i],sizeof(unsigned int),0);
            if(len>0)
            {
                //UPDBG("len :%d && server buffer[i]=%d\n",len,buffer[i],i);
            //@wei
            //buffer[i] is not equaled with 0x0a.it  specify the data didn't reach END.
            while(buffer[i]!=0x0a)//original
            //while(buffer[len-2]!=0x0A)
            {
               i++ ;//
               len=recv(w->fd,&buffer[i],sizeof(unsigned int),0);
               //UPDBG("len :%d && server buffer[i]=%d i = %d\n",len,buffer[i],i);
            }
            if(buffer[0]==0x15)
            {
                //UPDBG("temp: %d\n",buffer[1]);
                temp=buffer[1];
                switch(temp)
                {
                case 0x01:

                        UPDBG("COMMAND:-------TOPOINFO--------:\n");
                        Server_GetZigBeeNwkTopo(w->fd);
                        break;
                case 0x02:

                        //UPDBG("COMMAND:-------GetZigBeeNwkInfo--------:\n");
                        Server_GetZigBeeNwkInfo(w->fd);
                        break;
                case 0x03:

                        UPDBG("CMOMAND:-------SET_SENSOR_STATUS:--------:\n");//
                        Server_SetSensorStatus(w->fd,buffer[2],buffer[3]);
                        break;
                case 0x04:

                        UPDBG("COMMAND:-------GetRfidId--------:\n");
                        Server_GetRfidId(w->fd);
                        break;
                case 0x05:

                        UPDBG("COMMAND:-------GetTempHum--------:\n");
                        Server_GetTempHum(w->fd);
                        break;
                case 0x08:

                        Udbg("COMMAND:-------clear intrupt--------:\n");
                        gIntLock=0;
                        break;
                case 0x09:
                        Udbg("Command : get real time picture\n");
                        ClientGetRealPic(w->fd);
                        break;
                case 0x10://0x0a is end of frame
                        Udbg("Command:register UID \n");
                        Udbgv(buffer[3]);
                        switch(buffer[2]){
                            case 0x01://registing
                                gRegister_ready = 1;
                                registering_fd = w->fd;
                                Udbg("ready to register\n");
                            break;
                            case 0x02:
                                //removing
                                ret = delete_ID(buffer[3]);
                                if(!ret)
                                    Udbg("delete successfully\n");
                                Server_ErrorFeedback(w->fd,0);
                                break;
                            case 0x03:
                                //if(w->fd != qt_socket_fd)
                                Udbgv(w->fd);
                                Udbgv(qt_socket_fd);
                                if(w->fd == qt_socket_fd){
                                    Server_SetSensorStatus(w->fd,rf_nwk,1);
                                    break;
                                }else{
                                    ret = RFID_verify(buffer[3]);
                                    if(ret){
                                         //open door:
                                        Server_SetSensorStatus(w->fd,rf_nwk,1);
                                    }else{
                                        Server_ErrorFeedback(w->fd,ERROR_ID_NOT_EXIST);
                                        Udbg("not exist ID!\n");
                                    }
                                }
                               
                        }
                        break;
                case 0x11:

                        break;
                default:
                        UPDBG("error COMMAND\n");
                        break;

                }
            }
            else
            {
                UPDBG("other protrol.\n");
            }
        }
            else if(len ==0)
            {
                    UPDBG("remote socket closed!socket fd: %d\n",w->fd);
                    close(w->fd);
                    ev_io_stop(loop, w);
                    free(w);
                    return;
            }
            else
            {
                    if(errno == EAGAIN ||errno == EWOULDBLOCK)
                    {
                        UPDBG("errno == EAGAIN ||errno == EWOULDBLOCK\n");
                            //goto loop;
                    }
                    else
                    {
                            UPDBG("ret :%d ,close socket fd : %d\n",ret,w->fd);
                            close(w->fd);
                            ev_io_stop(loop, w);
                            free(w);
                            return;
                    }
            }
        }
        else{
                           UPDBG("not  server fd.\n");
                }
    }
    else if(ret == 0){
            UPDBG("server read wait timeout!!!\n");

    }
    else{// ret <0
            UPDBG("server select error.\n");
            //perror(ret);
    }



}


void write_callback(struct ev_loop *loop, ev_io *w, int revents)
{


    int fd=w->fd;

    ev_io_stop(loop,  w);

}

bool Server_GetZigBeeNwkInfo(int fd){
    UPDBG("Server:___________GetZigBeeNwkInfo\n");
/*int bufer [9]
    0x26 0x02 panid chnnal  maxchild  maxdepth  maxrouter
    */
        unsigned int buffer[9]={0};
        buffer[0]=0x26;
        buffer[1]=0x02;

        buffer[8]='\n';

        struct NwkDesp *pNwkDesp2;
        //bool netinfo   @wei update by server.
        //pNwkDesp2 =GetZigBeeNwkDesp();
        buffer[2]=pNwkDesp2->panid;
        buffer[3]=pNwkDesp2->channel;

        buffer[4]=pNwkDesp2->channel>>16;
        //UPDBG("pNwkDesp2->channel=%ld %d %d %ld\n",pNwkDesp2->channel,pNwkDesp2->channel>>16,pNwkDesp2->channel,pNwkDesp2->channel>>16+pNwkDesp2->channel);
        buffer[5]=pNwkDesp2->maxchild;
        buffer[6]=pNwkDesp2->maxdepth;
        buffer[7]=pNwkDesp2->maxrouter;

        int len=write(fd, (unsigned int*)(&buffer), sizeof(buffer));
        if(len<=0)
        {

            return FALSE;
        }


        return TRUE;

}


bool Server_SetSensorStatus(int fd,unsigned int addr,unsigned int state)
{
    Udbg(" SetSensorStatus nwk  = %08x ,state = %08x",addr,state);
    SetSensorStatus(addr,state);
    usleep(500000);
    CMD_GetZigBeeNwkTopo();
    UPDBG("Server:Server_SetSensorStatus end");
    return TRUE;

}

bool Server_ErrorFeedback(int fd,unsigned int err){
    unsigned int error[4] = {0x26,0xff,err,0x0a};
    int len=write(fd, (unsigned int*)(&error), sizeof(error));
        if(len<=0)
        {
            return FALSE;
        }
        return TRUE;
}

bool Server_GetZigBeeNwkTopo(int fd){
    struct NodeInfo* pNodeNwkTopoInfo ,*q;
    //modify by wei 
    pNodeNwkTopoInfo = GetZigBeeNwkTopoHead();//pNodeNwkTopoInfo=GetZigBeeNwkTopo();
    int ret ;
    //UPDBG("gNwkStatusFlag=%d\n",gNwkStatusFlag);
    topo_buffer[0]=0x26;
    topo_buffer[1]=0x01;
    if(gNwkStatusFlag)
    {
        UPDBG("fun :%s get alarm LINE :%d  \n",__FUNCTION__,__LINE__);
        if(pNodeNwkTopoInfo==NULL)
        {
            topo_buffer[2]=0x02;//for online,but NULL
            topo_buffer[3]=0x0A;
        UPDBG("fun :%s get alarm LINE :%d  \n",__FUNCTION__,__LINE__);
            write(fd, (unsigned int*)(&topo_buffer), sizeof(unsigned int)*4);
        }
        else{
        UPDBG("fun :%s get alarm LINE :%d  \n",__FUNCTION__,__LINE__);
            int i=0,j=0;
            while(pNodeNwkTopoInfo!=NULL)
            {
               topo_buffer[2+18*i]=pNodeNwkTopoInfo->devinfo->nwkaddr;
               UPDBG("cliect topo_buffer[%d]=%d\n",2+18*i,topo_buffer[2+18*i]);
               topo_buffer[3+18*i]=pNodeNwkTopoInfo->devinfo->macaddr[0];
               //UPDBG("cliect topo_buffer[%d]=%d\n",3+18*i,topo_buffer[3+18*i]);
               topo_buffer[4+18*i]=pNodeNwkTopoInfo->devinfo->macaddr[1];
               //UPDBG("cliect topo_buffer[%d]=%d\n",4+18*i,topo_buffer[4+18*i]);
               topo_buffer[5+18*i]=pNodeNwkTopoInfo->devinfo->macaddr[2];
               //UPDBG("cliect topo_buffer[%d]=%d\n",5+18*i,topo_buffer[5+18*i]);
               topo_buffer[6+18*i]=pNodeNwkTopoInfo->devinfo->macaddr[3];
               //UPDBG("cliect topo_buffer[%d]=%d\n",6+18*i,topo_buffer[6+18*i]);
               topo_buffer[7+18*i]=pNodeNwkTopoInfo->devinfo->macaddr[4];
               //UPDBG("cliect topo_buffer[%d]=%d\n",7+18*i,topo_buffer[7+18*i]);
               topo_buffer[8+18*i]=pNodeNwkTopoInfo->devinfo->macaddr[5];
               //UPDBG("cliect topo_buffer[%d]=%d\n",8+18*i,topo_buffer[8+18*i]);
               topo_buffer[9+18*i]=pNodeNwkTopoInfo->devinfo->macaddr[6];
               //UPDBG("cliect topo_buffer[%d]=%d\n",9+18*i,topo_buffer[9+18*i]);
               topo_buffer[10+18*i]=pNodeNwkTopoInfo->devinfo->macaddr[7];
               //UPDBG("cliect topo_buffer[%d]=%d\n",10+18*i,topo_buffer[10+18*i]);
               topo_buffer[11+18*i]=pNodeNwkTopoInfo->devinfo->depth;
               //UPDBG("cliect topo_buffer[%d]=%d\n",11+18*i,topo_buffer[11+18*i]);
               topo_buffer[12+18*i]=pNodeNwkTopoInfo->devinfo->devtype;
               //UPDBG("cliect topo_buffer[%d]=%d\n",12+18*i,topo_buffer[12+18*i]);
               topo_buffer[13+18*i]=pNodeNwkTopoInfo->devinfo->parentnwkaddr;
               //UPDBG("cliect topo_buffer[%d]=%d\n",13+18*i,topo_buffer[13+18*i]);
               topo_buffer[14+18*i]=pNodeNwkTopoInfo->devinfo->sensortype;
               //UPDBG("cliect topo_buffer[%d]=%d\n",14+18*i,topo_buffer[14+18*i]);
               topo_buffer[15+18*i]=pNodeNwkTopoInfo->devinfo->sensorvalue;
               //UPDBG("cliect topo_buffer[%d]=%d\n",15+18*i,topo_buffer[15+18*i]);
               topo_buffer[16+18*i]=pNodeNwkTopoInfo->devinfo->sensorvalue>>16;
               //UPDBG("cliect topo_buffer[%d]=%d\n",16+18*i,topo_buffer[16+18*i]);
               topo_buffer[17+18*i]=pNodeNwkTopoInfo->devinfo->status;
               //UPDBG("cliect topo_buffer[%d]=%d\n",17+18*i,topo_buffer[17+18*i]);
               topo_buffer[18+18*i]=pNodeNwkTopoInfo->row;
               //UPDBG("cliect topo_buffer[%d]=%d\n",18+18*i,topo_buffer[18+18*i]);
               topo_buffer[19+18*i]=pNodeNwkTopoInfo->num;
               //UPDBG("cliect topo_buffer[%d]=%d\n",19+18*i,topo_buffer[19+18*i]);
               pNodeNwkTopoInfo=pNodeNwkTopoInfo->next;
               j=i;
               i++;
            }
            topo_buffer[20+18*j]='\n';
            //UPDBG("cliect topo_buffer[%d]=%d\n",20+18*j,topo_buffer[20+18*j]);
            //write(fd, (unsigned int*)(&topo_buffer), sizeof(unsigned int)*(20+18*j+1));

            ret = write_securely(fd, (unsigned int*)(&topo_buffer), sizeof(unsigned int)*(20+18*j+1));
            if(ret < 0){
                Udbgln("fd is not exist");
                return FALSE;
            }
        }
    }
    else{

        topo_buffer[2]=0x01;//for offline
        topo_buffer[3]='\n';
        //write(fd, (unsigned int*)(&topo_buffer), sizeof(unsigned int)*4);
        if(fd == qt_socket_fd){
            write(fd, (unsigned int*)(&topo_buffer), sizeof(unsigned int)*4);
        }else{
            ret = write_securely(fd, (unsigned int*)(&topo_buffer), sizeof(unsigned int)*4);
            if(ret<0){
                Udbgln("write error");
                return FALSE;
            }
        }
    }


    return TRUE;
}
bool Server_GetTempHum(int fd){
    UPDBG("Server:___________GetTempHum\n");

        unsigned int buffer[5]={0};
        buffer[0]=0x26;
        buffer[1]=0x05;

        buffer[4]='\n';

        unsigned long int temp=GetBlueToothData();
        buffer[2]=temp;
        buffer[3]=temp>>16;

        int len=write(fd, (unsigned int*)(&buffer), sizeof(buffer));
        if(len<=0)
        {

            return FALSE;
        }


        return TRUE;
}
int Server_GetRfidId(int fd){
    UPDBG("Server:___________GetRfidId\n");

    unsigned int buffer[5]={0};
    buffer[0]=0x26;
    buffer[1]=0x04;

    buffer[4]='\n';

    unsigned long int id= GetRFIDData();
    buffer[2]=id;
    buffer[3]=id>>16;

    int len=write(fd, (unsigned int*)(&buffer), sizeof(buffer));
    if(len<=0)
    {

        return 0;
    }


    return 1;

}
int Server_GetGPRSSignal(int fd){
    UPDBG("Server:___________GetGPRSSignal\n");
    unsigned int buffer[4]={0};
    buffer[0]=0x26;
    buffer[1]=0x07;

    buffer[3]='\n';

    buffer[2]=gSim;

    int len=write(fd, (unsigned int*)(&buffer), sizeof(buffer));
    if(len<=0)
    {

        return 0;
    }


    return 1;
}
bool Server_SendGprsMessage(int fd,unsigned int *phone,unsigned int sensor){
    //UPDBG("@@@@@@@@@@@@@@@@@@@@@Server:___________SendGprsMessage@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
    int i=0;
    for(i=0;i<11;i++)
    {
    phone_buf[i]=*(phone+i);
    UPDBG("%c",*(phone+i));
    }
    //gprs_msg(phone_buf,sensor);
    enqueue_int_queue(&pQueue,sensor);
    return TRUE;
}
void gprs_send()
{

}

int ClientGetRealPic(int qt_fd){
    Udbg("Server:get qt Client real time Picture\n");
    unsigned int buffer[4]={0};
    buffer[0]=0x26;
    buffer[1]=0x08;

    buffer[3]='\n';

    buffer[2]= 0 ;

    int len=write(qt_fd, (unsigned int*)(&buffer), sizeof(buffer));
    if(len<=0)
    {

        return 0;
    }


    return 1;
}

int Server_GetRealPic(int fd){
    unsigned int buffer[4]={0};
    buffer[0]=0x26;
    buffer[1]=0x08;

    buffer[3]='\n';

    buffer[2]= 0 ;
    if(qt_socket_fd > 0){

    }
}