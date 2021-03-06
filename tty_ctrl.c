#include "tty.h"
#include "global.h"
#include "zigbee_ctrl.h"
#include "gprs.h"
#include "RFID_handler.h"

#include "list.h"
#include "fd_list.h"

pthread_mutex_t mutex;
pthread_mutex_t mutex_trans;

volatile int STOP=FALSE;
volatile int SEND_PAUSE = FALSE;
int zb_fd;
int bt_fd;
int rf_fd;
int gprs_fd;


#define ZB_BAUDRATE B115200
#define BT_BAUDRATE B9600
#define RF_BAUDRATE B9600
#define GPRS_BAUDRATE B9600

struct sigaction sa;
pthread_t th_kb, th_rev, th_send,bt_rev,rf_rev,msg_rev;
void * retval;


unsigned char gNwkStatusFlag;
unsigned char gFrameValidFlag;
unsigned char gCmdValidFlag;
unsigned char gIntLock;
unsigned char gRegister_ready;
NwkDesp gNwkDesp;
DeviceInfo gDeviceInfo;
SensorDesp gSensorDesp;

void ProcessNwkConnectRSP( unsigned char  *_pData , unsigned char _len );
void ProcessGetNwkDespRSP( unsigned char  *_pData , unsigned char _len );
void ProcessGetNwkTopoRSP( unsigned char  *_pData , unsigned char _len  );
void ProcesssSetSensorModeRSP( unsigned char  *_pData , unsigned char _len );
void ProcessGetSensorStatusRSP( unsigned char  *_pData , unsigned char _len );
void ProcessSetSensorStatusRSP( unsigned char  *_pData , unsigned char _len );
void ProcessGetDevInfoRSP( unsigned char  *_pData , unsigned char _len );
void ProcessRptNodeOutRSP( unsigned char  *_pData , unsigned char _len );

unsigned int gBTStatusFlag;
unsigned long int gBTDatas;
unsigned long int gRFDatas;
//int msgid;
//struct msgbuf msg_rbuf;

unsigned char Data_CalcFCS( unsigned char  *msg_ptr, unsigned char  len )
{
      unsigned char  x;
      unsigned char  xorResult;

      xorResult = 0;

      for ( x = 0; x < len; x++, msg_ptr++ )
        xorResult = xorResult ^ *msg_ptr;

      return ( xorResult );
}

int CMD_CalcFCS( unsigned char  *msg_ptr, unsigned char  len )
{
	unsigned int  cmd;
	unsigned int flag = 0;

	cmd = BUILD_UINT16( msg_ptr[1], msg_ptr[0] );
	//UPDBG("cmd=0x%x\n",cmd);
	UPDBG("cmd calcfcs.\n");
	
	//Process the contents of the message
	switch ( cmd )
	{	  
	  case SPI_CMD_GET_NWK_TOPO_RSP:
	    flag = 1;
	    break;

	  case SPI_CMD_NWK_CONNECT_RSP:
	    flag = 1;
	    break;

	  case SPI_CMD_GET_NWK_DESP_RSP:
	    flag = 1;
	    break;

	  case SPI_CMD_SET_SENSOR_MODE_RSP:
	    flag = 1;
	    break;

	  case SPI_CMD_GET_SENSOR_STATUS_RSP:
	    flag = 1;
	    break;

	  case SPI_CMD_SET_SENSOR_STATUS_RSP:
	    flag = 1;
	    break;

	  case SPI_CMD_GET_DEVINFO_RSP:
	    flag = 1;
	    break;

	  case SPI_CMD_RPT_NODEOUT_RSP:
	    flag = 1;
	    break;	 
	    //@wei add by wei 	
	  case CMD_ALARM_ASYNC:
	  	flag = 1;
		
	  default:
	    break;
	}

	return flag;
}

int Data_PackageParser( unsigned char  *_pData, unsigned char  len )
{
	unsigned int  cmd;

	cmd = BUILD_UINT16( _pData[2], _pData[1] );
	UPDBG("cmd=0x%x\n",cmd);
	
	//Process the contents of the message
	switch ( cmd )
	{	  
	  case SPI_CMD_NWK_CONNECT_RSP:
	    ProcessNwkConnectRSP( _pData, len );
	    break;

	  case SPI_CMD_GET_NWK_DESP_RSP:
	    ProcessGetNwkDespRSP( _pData, len );
	    break;

	  case SPI_CMD_GET_NWK_TOPO_RSP:	  	
	    ProcessGetNwkTopoRSP( _pData, len );				
	    break;

	  case SPI_CMD_SET_SENSOR_MODE_RSP:
	    ProcesssSetSensorModeRSP( _pData, len );
	    break;

	  case SPI_CMD_GET_SENSOR_STATUS_RSP:
	    ProcessGetSensorStatusRSP( _pData, len );
	    break;

	  case SPI_CMD_SET_SENSOR_STATUS_RSP:
	    ProcessSetSensorStatusRSP( _pData, len );
	    break;

	  case SPI_CMD_GET_DEVINFO_RSP:
	    ProcessGetDevInfoRSP( _pData, len );
	    break;

	  case SPI_CMD_RPT_NODEOUT_RSP:
	    ProcessRptNodeOutRSP( _pData, len );
	    break;	  	
		
	  default:
	    break;
	}

	return TRUE;

}


void CMD_NwkDetect(void)
{

	pthread_mutex_lock(&mutex);	

	unsigned char txbuff[5]={0,};
	//unsigned char rxbuff[11];

	//int i=0,j=0;

	txbuff[0]=SOP_VALUE;// SOP
	txbuff[1]=0x00;// CMDH
	txbuff[2]=0x50;// CMDL
	txbuff[3]=0x00;// LEN

	txbuff[4]=Data_CalcFCS(txbuff+1, (unsigned char)3);// FCS
	UPDBG("\ncmd nwk deteck\n");
	tty_write(zb_fd,txbuff,5);


	pthread_mutex_unlock(&mutex);

#if 0
    //·¢ËÍÃüÁî
    while ((i++)< ITIMEOUT) {
            tty_write(txbuff,10);
            //Ô¤½ÓÊÕ11¸ö×Ö½ÚµÄ·µ»ØÊý¾ÝÐÅÏ¢
            while(tty_read(rxbuff,10) < 0 && (j++) < JTIMEOUT);
            //¶Ô½ÓÊÕµ½µÄÊý¾Ý½øÐÐÔ¤´¦Àí
            if((rxbuff[0]!= 0xBB) || (rxbuff[3] != 0x00 || rxbuff[4] != cmd)){ //ÅÐ¶ÏÎÄ¼þÍ·£¬ÒÔ¼°»úºÅ
                    //ÑéÖ¤´íÎó
                    continue;
            }else{
                    //ÊäÈë³¬Ê±´¦Àí
                    *rx_param = rxbuff[5];//´«µÝ²ÎÊýµ½ÉÏ²ã
                    break;
            }
    }

    return (i == ITIMEOUT+1)?0:1;
#endif
}

void CMD_GetNwkDesp(void)
{
	pthread_mutex_lock(&mutex);	

	unsigned char txbuff[5]={0,};

	txbuff[0]=SOP_VALUE;// SOP
	txbuff[1]=0x00;// CMDH
	txbuff[2]=0x51;// CMDL
	txbuff[3]=0x00;// LEN

	txbuff[4]=Data_CalcFCS(txbuff+1, (unsigned char)3);// FCS
        //UPDBG("\ncmd nwk detect\n");//by gxj
	tty_write(zb_fd,txbuff,5);

	pthread_mutex_unlock(&mutex);	
}

void CMD_SetSensorWorkMode(/*unsigned int nwkaddr,*/unsigned char Mode)
{

	pthread_mutex_lock(&mutex);	

	unsigned char txbuff[6]={0,};

	txbuff[0]=SOP_VALUE;// SOP
	txbuff[1]=0x00;// CMDH
	txbuff[2]=0x53;// CMDL
	txbuff[3]=0x01;// LEN
	txbuff[4]=Mode;// LEN

	txbuff[5]=Data_CalcFCS(txbuff+1, (unsigned char)4);// FCS
        //UPDBG("\ncmd set sensor mode\n");//by gxj
	tty_write(zb_fd,txbuff,6);

	pthread_mutex_unlock(&mutex);	
}

void CMD_SetSensorStatus(unsigned int nwkaddr, unsigned  int status)
{
	pthread_mutex_lock(&mutex);	
	
	unsigned char txbuff[8]={0,};
	unsigned char addr[2];

	addr[0]=(unsigned char)HI_UINT16(nwkaddr);
	addr[1]=(unsigned char)LO_UINT16(nwkaddr);

	txbuff[0]=SOP_VALUE;// SOP
	txbuff[1]=0x00;// CMDH
	txbuff[2]=0x55;// CMDL
	txbuff[3]=0x03;// LEN
	txbuff[4]=addr[0];// nwkaddr H
	txbuff[5]=addr[1];// nwkaddr L
	txbuff[6]= status;
	printf("txbuf 5 is %02x\n",txbuff[6]);

	txbuff[7]=Data_CalcFCS(txbuff+1, (unsigned char)6);// FCS
        //UPDBG("\ncmd set sensor status\n");//by gxj
	tty_write(zb_fd,txbuff,8);
	
	pthread_mutex_unlock(&mutex);
}

void CMD_GetSensorStatus(unsigned int nwkaddr)
{

	pthread_mutex_lock(&mutex);	

	unsigned char txbuff[7]={0,};
	unsigned char addr[2];

	addr[0]=(unsigned char)HI_UINT16(nwkaddr);
	addr[1]=(unsigned char)LO_UINT16(nwkaddr);

	txbuff[0]=SOP_VALUE;// SOP
	txbuff[1]=0x00;// CMDH
	txbuff[2]=0x54;// CMDL
	txbuff[3]=0x02;// LEN
	txbuff[4]=addr[0];// nwkaddr H
	txbuff[5]=addr[1];// nwkaddr L


	txbuff[6]=Data_CalcFCS(txbuff+1, (unsigned char)5);// FCS
        //UPDBG("\ncmd get sensor status\n");//by gxj
	tty_write(zb_fd,txbuff,7);

	pthread_mutex_unlock(&mutex);	
}

void CMD_GetZigBeeDevInfo(unsigned int nwkaddr)
{

	pthread_mutex_lock(&mutex);	

	unsigned char txbuff[7]={0,};
	unsigned char addr[2];

	addr[0]=(unsigned char)HI_UINT16(nwkaddr);
	addr[1]=(unsigned char)LO_UINT16(nwkaddr);

	txbuff[0]=SOP_VALUE;// SOP
	txbuff[1]=0x00;// CMDH
	txbuff[2]=0x56;// CMDL
	txbuff[3]=0x02;// LEN
	txbuff[4]=addr[0];// nwkaddr H
	txbuff[5]=addr[1];// nwkaddr L


	txbuff[6]=Data_CalcFCS(txbuff+1, (unsigned char)5);// FCS
        //UPDBG("\ncmd get dev info\n");//by gxj
	tty_write(zb_fd,txbuff,7);

	pthread_mutex_unlock(&mutex);	
}
// ffffffff9f06bb1a
void CMD_GetZigBeeNwkTopo(void)
{

	pthread_mutex_lock(&mutex);	

	unsigned char txbuff[5]={0,};

	txbuff[0]=SOP_VALUE;// SOP
	txbuff[1]=0x00;// CMDH
	txbuff[2]=0x52;// CMDL
	txbuff[3]=0x00;// LEN

	txbuff[4]=Data_CalcFCS(txbuff+1, (unsigned char)3);// FCS
    //UPDBG("\ncmd get nwk topologic  txbuff[4] = %02x\n",txbuff[4]);//by gxj
	tty_write(zb_fd,txbuff,5);

	pthread_mutex_unlock(&mutex);
}
/**
* add by @wei
*/
void CMD_GETZigebeePins(void)
{
	pthread_mutex_lock(&mutex);	

	unsigned char txbuff[5]={0,};

	txbuff[0]=SOP_VALUE;// SOP
	txbuff[1]=0x00;// CMDH
	txbuff[2]=0x58;// CMDL
	txbuff[3]=0x00;// LEN
	txbuff[4]=Data_CalcFCS(txbuff+1, (unsigned char)3);// FCS
    //UPDBG("\ncmd get nwk topologic  txbuff[4] = %02x\n",txbuff[4]);//by gxj
	tty_write(zb_fd,txbuff,5);

	pthread_mutex_unlock(&mutex);
}

void CMD_ZIGBEE_RESTART(void)
{
	pthread_mutex_lock(&mutex);	
	unsigned char txbuff[6]={0x02,0,0x05,0x01,0x0,0x04};
	tty_write(zb_fd,txbuff,6);
	Udbg("restart zigbee coodrator\n");
	pthread_mutex_unlock(&mutex);
}
/*end add*/
void ProcessNwkConnectRSP( unsigned char  *_pData , unsigned char _len)
{
	UPDBG("cmd:nwk connect rsp\n");
	//gNwkStatusFlag = _pData[3+_len];
	pthread_mutex_lock(&mutex);	
	gNwkStatusFlag = 0x01;
        //UPDBG("gNwkStatusFlag=0x%x\n", gNwkStatusFlag);//by gxj
	pthread_mutex_unlock(&mutex);

	return;		
}

void ProcessGetNwkDespRSP( unsigned char  *_pData , unsigned char _len )
{
        //UPDBG("cmd:get nwk desp rsp\n");//by gxj
	unsigned int panid;
	unsigned long int channel;

	pthread_mutex_lock(&mutex);	

	panid = BUILD_UINT16(_pData[5], _pData[4]);
	channel = BUILD_UINT32(_pData[9], _pData[8], _pData[7], _pData[6]);
	
	gNwkDesp.panid = panid;
	gNwkDesp.channel = channel;
	gNwkDesp.maxchild = _pData[10];
	gNwkDesp.maxdepth = _pData[11];	
	gNwkDesp.maxrouter =_pData[12];

        /*UPDBG("gNwkDesp.panid=0x%x\n", gNwkDesp.panid);
	UPDBG("gNwkDesp.channel=0x%lx\n", gNwkDesp.channel);
	UPDBG("gNwkDesp.maxchild=0x%x\n", gNwkDesp.maxchild);
	UPDBG("gNwkDesp.maxdepth=0x%x\n", gNwkDesp.maxdepth);
        UPDBG("gNwkDesp.maxrouter=0x%x\n", gNwkDesp.maxrouter);*///by gxj

	pthread_mutex_unlock(&mutex);	

	return;	
}

void ProcessGetNwkTopoRSP( unsigned char  *_pData , unsigned char _len  )
{
        //UPDBG("cmd:get nwk topo rsp\n");//by gxj
	int i;
	unsigned int  nwkaddr;
	unsigned char  macaddr[8];
	unsigned char  depth;
	unsigned char  devtype;
	unsigned int  parentnwkaddr;
	unsigned char  sensortype;
	unsigned long int  sensorvalue;
	unsigned char  status;

	pthread_mutex_lock(&mutex);

	if((_pData[3])==0x01){// cmd ack pakage. not real datapakage.
		pthread_mutex_unlock(&mutex);		
		return;
	}	

	nwkaddr = BUILD_UINT16(_pData[5], _pData[4]);
	for(i=0; i<8; i++){
		macaddr[i] = _pData[i+6];
	}
	depth = _pData[14];
	devtype = _pData[15];
	parentnwkaddr = BUILD_UINT16(_pData[17], _pData[16]);
	sensortype = _pData[18];
	sensorvalue = BUILD_UINT32(_pData[22], _pData[21], _pData[20], _pData[19]);	
	//printf("devtype is %02x\n",sensortype);
	gDeviceInfo.nwkaddr = nwkaddr;
	for(i=0; i<8; i++){
		gDeviceInfo.macaddr[i] = macaddr[i];
	}
	gDeviceInfo.depth = depth;
	gDeviceInfo.devtype = devtype;
	gDeviceInfo.parentnwkaddr = parentnwkaddr;
	gDeviceInfo.sensortype = sensortype;
	gDeviceInfo.sensorvalue = sensorvalue;
	gDeviceInfo.status = 0x01;// node status  ffffffff9f06bb1a
	
    Udbgln("gDeviceInfo.nwkaddr=0x%x", gDeviceInfo.nwkaddr);
    UPDBG("gDeviceInfo.macaddr=0x");//by gxj
	for(i=0; i<8; i++){
		UPDBG("%lx ", gDeviceInfo.macaddr[i]);
	}
        UPDBG("\n");
	
	UPDBG("gDeviceInfo.depth=0x%lx\n", gDeviceInfo.depth);
	UPDBG("gDeviceInfo.devtype=0x%x\n", gDeviceInfo.devtype);
	UPDBG("gDeviceInfo.parentnwkaddr=0x%x\n", gDeviceInfo.parentnwkaddr);
	Udbg("gDeviceInfo.sensortype=0x%x\n", gDeviceInfo.sensortype);
	Udbgln("gDeviceInfo.sensorvalue0x%x", gDeviceInfo.sensorvalue);
    UPDBG("gDeviceInfo.status=0x%x", gDeviceInfo.status);//by gxj

	pNodeInfo p = DeviceNodeSearch(nwkaddr);
	if(NULL != p){// exist node nwkaddr
		Udbg("nwkaddr :%x exist update info.\n",nwkaddr);
		//p->devinfo = &gDeviceInfo;// update datas
		{
			p->devinfo->nwkaddr = gDeviceInfo.nwkaddr;
			
			for(i=0;i<8;i++){
				p->devinfo->macaddr[i] = gDeviceInfo.macaddr[i];
			}
			p->devinfo->depth = gDeviceInfo.depth;
			p->devinfo->devtype = gDeviceInfo.devtype;
			p->devinfo->parentnwkaddr = gDeviceInfo.parentnwkaddr;
			p->devinfo->sensortype = gDeviceInfo.sensortype;
			/*
	public final static String[] DEVTYPESTR 	= {"无","蓝色","green"};
	public final static String[] SENSORTYPESTR 	= {"温湿度","人体红外","可然气体","引脚信息"};
	public final static String[] SENSORSTATUS 	= {"正常","警告"};*/
			if((p->devinfo->sensortype) ==0x07){
				rf_nwk = gDeviceInfo.nwkaddr;
				//goto IGNORE_RFID_UPDATE;
			}
			if((p->devinfo->sensortype) !=0 &&(gIntLock==0)){
				p->devinfo->sensorvalue = gDeviceInfo.sensorvalue;
			}
			else if((p->devinfo->sensortype) ==0){//sht11 sensor
				p->devinfo->sensorvalue = gDeviceInfo.sensorvalue;

			}
			if(sensorvalue ==1){
				UPDBG("get int \n");
				//sleep(5);	
			}
IGNORE_RFID_UPDATE:
			p->devinfo->status = gDeviceInfo.status;// recover status		
		}
	}
	else if(NULL == p){// new node nwkaddr
                //UPDBG("new nwkaddr:%x make node.\n",nwkaddr);//by gxj
		pNodeInfo pp = DeviceNodeCreate(&gDeviceInfo);
		if((pp->devinfo->sensortype) ==0x07){
			pp->devinfo->sensorvalue = 0;
		}
		DeviceNodeAdd(pp);		
	}

    Udbg("node num:%x\n",DeviceNodeNum(NodeInfoHead));//by gxj
	pthread_mutex_unlock(&mutex);

}

void ProcesssSetSensorModeRSP( unsigned char  *_pData , unsigned char _len )
{
	UPDBG("cmd:set sensor mode rsp\n");
}
/**
*@wei
*the function called when message been sent from zigbee asynchronously.
*/

void debug_fun(unsigned char type,unsigned int value){
	int ret;
	Udbg("fun :%s get alarm \n",__FUNCTION__);
	Udbgv(value);
	gRegister_ready = 1;
	if(gRegister_ready){
		Udbgv(value);
		register_ID(value);
		gRegister_ready = 0;
	}else{
		//The state is registering ether verifying.
		ret = RFID_verify(value);
		Udbgv(ret);
	}	
}

void ProcessGetSensorStatusRSP( unsigned char  *_pData , unsigned char _len )
{	
    UPDBG("cmd:get sensor status rsp\n");//by gxj
	unsigned int nwkaddr;
	unsigned char type;
	unsigned long int value;
	int ret,index,err;
	pthread_mutex_lock(&mutex_trans);	
	//SEND_PAUSE = true;
	gIntLock = 0x01;
	nwkaddr = BUILD_UINT16(_pData[5], _pData[4]);
	type = _pData[6];
	value = BUILD_UINT32(_pData[10], _pData[9], _pData[8], _pData[7]);

	pNodeInfo p = DeviceNodeSearch(nwkaddr);
	//debug_fun(type,value);
	if(NULL != p){// exist node nwkaddr
        //UPDBG("nwkaddr :%x is interrupt.\n",nwkaddr);//by gxj
		p->devinfo->sensorvalue = value;
		Udbg("fun :%s get alarm \n",__FUNCTION__);
		//@wei Update nwk info before tty data come in next time.
		//Actually,only the some sinsor need to update android client;
		if(type == 0x07){
			//return;}else{//type == 0x07
//@wei below code is for RFIDcard system.
//server try to handle all RFID message for all clients.should handle in recv thread.  注意gintlock  需要客户端清除中断才能下一次更新
			if(gRegister_ready && value!=0){
				//register ready. register to server,
				//judge the id has regiestered .
				Udbgv(value);
				ret = register_ID(value);
				gRegister_ready = 0;
				//to client (with the registered ID)
				if(ret < 0){
					Udbg("register fail\n");
				}else{
					p->devinfo->sensorvalue = value;
					if(registering_fd > 0){
						Server_GetZigBeeNwkTopo(registering_fd);
						Udbg("register successfully\n");
					}
				}
			}else{
				//The state is registering ether verifying. just verfying dont tell clinet.
				ret = RFID_verify(value);
				Udbgv(value);
				if(ret){
					Udbg("verify successfully.ready to open door  && rf_nwk : %08x\n",rf_nwk);
					p->devinfo->sensorvalue = value;///Open the door and tell to clients.		
					CMD_SetSensorStatus(rf_nwk,1);//in test dont need to tell clients.
					//Server_ErrorFeedback()
				}else{
					p->devinfo->sensorvalue = 0x1;//warning!
					Udbgln("THE ID IS NOT REGISTERED!");
					CMD_SetSensorStatus(rf_nwk,0);//alarm
					err = ERROR_ID_NOT_EXIST;
				}	
			}
			gIntLock = 0;//clear auto 
		}

        reg_fd_list_t *entry;
        bool ret_b;
        list_for_each(iterator,&head)//遍历  
       {
            entry = list_entry(iterator,struct reg_fd_list,node);//读取某个值  
            Udbgv(ret);
            //printf("socketfd :%d\n",entry->socketfd);
            if((entry->socketfd > 0)){// && (entry->socketfd!=qt_socket_fd))
		        if(!ret){
	            	Udbgln("feedback!!!!!!");
	            	Server_ErrorFeedback(entry->socketfd,err);

	            }
	            ret_b = Server_GetZigBeeNwkTopo(entry->socketfd);
	            if(ret_b==FALSE)
	            	break;
	        }
        }
        ULdbg();
		if(qt_socket_fd > 0){
			Server_GetZigBeeNwkTopo(qt_socket_fd);
			if(ret){
	            Server_ErrorFeedback(qt_socket_fd,0);
	        }else{
				Server_ErrorFeedback(qt_socket_fd,err);
	        }
		}
		//UPDBG("error plat_other_fd \n");
		p->devinfo->sensorvalue = 0;
	}
	else if(NULL == p){// new node nwkaddr
		Udbg("new nwkaddr:%x is not find.\n",nwkaddr);
	}
	
	pthread_mutex_unlock(&mutex_trans);	

	return;	
}

void ProcessSetSensorStatusRSP( unsigned char  *_pData , unsigned char _len )
{
	UPDBG("cmd:set sensor status rsp\n");
		printf("cmd:set sensor status rsp\n");
	unsigned int nwkaddr;
	unsigned char type;
	unsigned char value;

	pthread_mutex_lock(&mutex);	
	/*======_pData=0x02,0x10,0x55,0x04,0x00,0x01,0x05,0x0c*/

	printf("======_pData=%0x,%0x,%0x,%0x,%0x,%0x,%0x,%0x\n",
		_pData[0],_pData[1],_pData[2],_pData[3],_pData[4],_pData[5],_pData[6],_pData[7] );
	gIntLock = 0x01;

	nwkaddr = BUILD_UINT16(_pData[5], _pData[4]);
	printf("====================nwkaddr=%0x\n",nwkaddr );
	type = _pData[6];
	printf("====================type=%0x\n",type );
	value = _pData[7];
	printf("====================value=%0x\n",value );
	
	pNodeInfo p = DeviceNodeSearch(nwkaddr);

	if(NULL != p && p->devinfo->sensortype == 0x05){// exist node nwkaddr
                //printf("nwkaddr :%x is interrupt.\n",nwkaddr);//by gxj
		//p->devinfo->sensortype;
		p->devinfo->sensorvalue = value;
	}
	else if(NULL == p){// new node nwkaddr
		printf("new nwkaddr:%x is not find.\n",nwkaddr);	
	}
	
	pthread_mutex_unlock(&mutex);	


}

void ProcessGetDevInfoRSP( unsigned char  *_pData , unsigned char _len )
{	
        //UPDBG("cmd:get dev info rsp\n");//by gxj
	int i;
	unsigned int  nwkaddr;
	unsigned char  macaddr[8];
	unsigned char  depth;
	unsigned char  devtype;
	unsigned int  parentnwkaddr;
	unsigned char  sensortype;
	unsigned long int  sensorvalue;
	unsigned char  status;

	pthread_mutex_lock(&mutex);

	nwkaddr = BUILD_UINT16(_pData[5], _pData[4]);
	for(i=0; i<8; i++){
		macaddr[i] = _pData[i+6];
	}
	depth = _pData[14];
	devtype = _pData[15];
	parentnwkaddr = BUILD_UINT16(_pData[17], _pData[16]);
	sensortype = _pData[18];
	sensorvalue = BUILD_UINT32(_pData[22], _pData[21], _pData[20], _pData[19]);	

	gDeviceInfo.nwkaddr = nwkaddr;
	for(i=0; i<8; i++){
		gDeviceInfo.macaddr[i] = macaddr[i];
	}
	gDeviceInfo.depth = depth;
	gDeviceInfo.devtype = devtype;
	gDeviceInfo.parentnwkaddr = parentnwkaddr;
	gDeviceInfo.sensortype = sensortype;
	gDeviceInfo.sensorvalue = sensorvalue;
	gDeviceInfo.status = 0x01;// node status 
	
        UPDBG("gDeviceInfo.nwkaddr=0x%x\n", gDeviceInfo.nwkaddr);

	UPDBG("gDeviceInfo.macaddr=0x");
	for(i=0; i<8; i++){
		UPDBG("%lx ", gDeviceInfo.macaddr[i]);
	}
        UPDBG("\n");
	
	UPDBG("gDeviceInfo.depth=0x%lx\n", gDeviceInfo.depth);
	UPDBG("gDeviceInfo.devtype=0x%x\n", gDeviceInfo.devtype);
	UPDBG("gDeviceInfo.parentnwkaddr=0x%x\n", gDeviceInfo.parentnwkaddr);
	UPDBG("gDeviceInfo.sensortype=0x%x\n", gDeviceInfo.sensortype);
	UPDBG("gDeviceInfo.sensorvalue0x%x\n", gDeviceInfo.sensorvalue);
        UPDBG("gDeviceInfo.status=0x%x\n", gDeviceInfo.status);//by gxj

	pNodeInfo p = DeviceNodeSearch(nwkaddr);
	if(NULL != p){// exist node nwkaddr
                UPDBG("nwkaddr :%x exist update info.\n",nwkaddr);//by gxj
		//p->devinfo = &gDeviceInfo;// update datas
		{
			p->devinfo->nwkaddr = gDeviceInfo.nwkaddr;
			
			for(i=0;i<8;i++){
				p->devinfo->macaddr[i] = gDeviceInfo.macaddr[i];
			}
			p->devinfo->depth = gDeviceInfo.depth;
			p->devinfo->devtype = gDeviceInfo.devtype;
			p->devinfo->parentnwkaddr = gDeviceInfo.parentnwkaddr;
			p->devinfo->sensortype = gDeviceInfo.sensortype;
			if((p->devinfo->sensortype) !=0 &&(gIntLock==0)){
				p->devinfo->sensorvalue = gDeviceInfo.sensorvalue;
			}
			else if((p->devinfo->sensortype) ==0){
				p->devinfo->sensorvalue = gDeviceInfo.sensorvalue;
			}
			p->devinfo->status = gDeviceInfo.status;		
		}
	}
	else if(NULL == p){// new node nwkaddr
                UPDBG("new nwkaddr:%x make node.\n",nwkaddr);//by gxj
		pNodeInfo p = DeviceNodeCreate(&gDeviceInfo);
		DeviceNodeAdd(p);		
	}

        Udbg("node num:%x\n",DeviceNodeNum(NodeInfoHead));//by gxj

	pthread_mutex_unlock(&mutex);		
}

void	ProcessRptNodeOutRSP( unsigned char  *_pData , unsigned char _len )
{
	Udbg("cmd:rpt node out rsp\n");
	int i;
	static int count ;
	unsigned int  nwkaddr;
	//CMD_ZIGBEE_RESTART();
	//notice to deadlock
	pthread_mutex_lock(&mutex);

	
	nwkaddr = BUILD_UINT16(_pData[5], _pData[4]);

	pNodeInfo p = DeviceNodeSearch(nwkaddr);
	if(NULL != p){// fint out node nwkaddr
		p->devinfo->status = 0x0;

		//DeviceNodeDel(p);
		//DeviceNodeFree(p);				
		UPDBG("find a out node and nwkaddr=%x\n",p->devinfo->nwkaddr);
	}
	
	pthread_mutex_unlock(&mutex);		

}
void ProcessRptGETZigebeePins( unsigned char  *_pData , unsigned char _len )
{
	UPDBG("cmd:get ZigebeePins\n");

	return;	
}
int ZigBeeNwkDetect(void)
{
	//unsigned char nwkstatusflag;
	CMD_NwkDetect();
	//nwkstatusflag = gNwkStatusFlag;		
	//return nwkstatusflag;
	return gNwkStatusFlag;
}


NwkDesp *GetZigBeeNwkDesp(void)
{
	CMD_GetNwkDesp();
	pNwkDesp nwkdesp = &gNwkDesp;
	
	return nwkdesp;
}

int SetSensorWorkMode(/*unsigned int nwkaddr,*/unsigned char Mode)
{
	CMD_SetSensorWorkMode(Mode);
	return 0;
}

int SetSensorStatus(unsigned int nwkaddr, unsigned  int status)
{
	pthread_mutex_lock(&mutex_trans); // by @wei
	CMD_SetSensorStatus(nwkaddr, status);
	pthread_mutex_unlock(&mutex_trans); // by @wei
	return 0;
}

// do not forget free SensorDesp buffer!!!
SensorDesp *GetSensorStatus(unsigned int nwkaddr)
{
	//CMD_GetSensorStatus(nwkaddr);

	pNodeInfo p = DeviceNodeSearch(nwkaddr);
	pSensorDesp sensordesp = malloc(sizeof(*sensordesp));
	if(NULL != p){// exist node nwkaddr
                UPDBG("nwkaddr :%x is find.\n",nwkaddr);//by gxj
		sensordesp->nwkaddr = p->devinfo->nwkaddr;
		sensordesp->sensortype = p->devinfo->sensortype;
		sensordesp->sensorvalue = p->devinfo->sensorvalue;
		return sensordesp;
	}
	else if(NULL == p){// new node nwkaddr
                UPDBG("new nwkaddr:%x is not find.\n",nwkaddr);//by gxj
		//free(sensordesp);
		return NULL;
	}
	
}

DeviceInfo* GetZigBeeDevInfo(unsigned int nwkaddr)
{
	//CMD_GetZigBeeDevInfo(nwkaddr);
	int i;

	pNodeInfo p = DeviceNodeSearch(nwkaddr);
	if(NULL != p){// exist node nwkaddr
                UPDBG("nwkaddr :%x is find.\n",nwkaddr);//by gxj
		{
			return p->devinfo;
		}
	}
	else if(NULL == p){// new node nwkaddr
                UPDBG("new nwkaddr:%x is not find.\n",nwkaddr);//by gxj
		return NULL;
	}

}

NodeInfo *GetZigBeeNwkTopo(void)
{
	CMD_GetZigBeeNwkTopo();

	return NodeInfoHead;
}

NodeInfo *GetZigBeeNwkTopoHead(void)
{
	return NodeInfoHead;
}

void ShowNodeListInfo(pNodeInfo pNodeHead)
{
        int len,i;
	pNodeInfo p;

	len = DeviceNodeNum(NodeInfoHead);
        //UPDBG("\r\nnodelist len=%x\n",len);//by gxj
	
	for(p=NodeInfoHead; p;p=p->next){
		
                UPDBG("\r\np->DeviceInfo.nwkaddr=0x%x\n", p->devinfo->nwkaddr);
                UPDBG("p->DeviceInfo.macaddr=0x");
		for(i=0; i<8; i++){
			UPDBG("%lx ", p->devinfo->macaddr[i]);
		}
		UPDBG("\n");
		
		UPDBG("p->DeviceInfo.depth=0x%lx\n", p->devinfo->depth);
		UPDBG("p->DeviceInfo.devtype=0x%x\n", p->devinfo->devtype);
		UPDBG("p->DeviceInfo.parentnwkaddr=0x%x\n", p->devinfo->parentnwkaddr);
		UPDBG("p->DeviceInfo.sensortype=0x%x\n", p->devinfo->sensortype);
		UPDBG("p->DeviceInfo.sensorvalue0x%x\n", p->devinfo->sensorvalue);
		UPDBG("p->DeviceInfo.status=0x%x\n", p->devinfo->status);	
				
        }//by gxj
	
}

void SigChild_Handler(int s)
{
        UPDBG("stop!!!\n");
        STOP=TRUE;
}

unsigned long int GetBlueToothData(void)
{
	return gBTDatas;
}

unsigned int GetBlueToothStatus(void)
{
	return gBTStatusFlag;
}

void HandleBlueToothData(unsigned char  *_pData , unsigned char _len)
{
	UPDBG("handle bluetooth data\n");
	int i;
	//unsigned long int  data;

	pthread_mutex_lock(&mutex);
	
	gBTDatas = BUILD_UINT32(_pData[3], _pData[2], _pData[1], _pData[0]);	

	pthread_mutex_unlock(&mutex);	
}

unsigned long int GetRFIDData(void)
{
	return gRFDatas;
}

void HandleRFIDData(unsigned char  *_pData , unsigned char _len)
{
	UPDBG("handle rfid data\n");
	int i;
	//unsigned long int  data;

	pthread_mutex_lock(&mutex);
	
	gRFDatas= BUILD_UINT32(_pData[3], _pData[2], _pData[1], _pData[0]);	

	pthread_mutex_unlock(&mutex);	
}

/*--------------------------------------------------------*/
void* KeyBoardPthread(void * data)
{
        int c;
	 int i,j,len;

	//pthread_detach(pthread_self());
	 
        for (;;){		 	
                c=getchar();
          	 //UPDBG("getchar is :%d\n",c);
                if( (c== ENDMINITERM1) | (c==ENDMINITERM2)){
                        STOP=TRUE;
                        break ;
                }
		 if(c==0x30){
		 	pNodeInfo p = DeviceNodeSearch(0x796F);
			if(NULL != p){
				UPDBG("Find it and Del it!!!\n");
				DeviceNodeDel(p);
				DeviceNodeFree(p);
			}
			continue;
		 }
		 if(c==0x31){
			pNwkDesp nwkdesp;

			nwkdesp  = GetZigBeeNwkDesp();
			
                        UPDBG("nwkdesp->panid=0x%x\n", nwkdesp->panid);
			UPDBG("nwkdesp->channel=0x%lx\n", nwkdesp->channel);
			UPDBG("nwkdesp->maxchild=0x%x\n", nwkdesp->maxchild);
			UPDBG("nwkdesp->maxdepth=0x%x\n", nwkdesp->maxdepth);
                        UPDBG("nwkdesp->maxrouter=0x%x\n", nwkdesp->maxrouter);//by gxj
			
			continue;
		 }
		 if(c==0x32){
			pNodeInfo nodehead;
			
			nodehead  = GetZigBeeNwkTopo();
			ShowNodeListInfo(nodehead);			
			continue;
		 }
		 if(c==0x33){
			pDeviceInfo devinfo;
			
			devinfo  = GetZigBeeDevInfo(0x796F);
			if(NULL != devinfo){			
                                UPDBG("devinfo->nwkaddr=0x%x\n", devinfo->nwkaddr);

				UPDBG("devinfo->macaddr=0x");
				for(i=0; i<8; i++){
					UPDBG("%lx ", devinfo->macaddr[i]);
				}
				UPDBG("\n");
				
				UPDBG("devinfo->depth=0x%lx\n", devinfo->depth);
				UPDBG("devinfo->devtype=0x%x\n", devinfo->devtype);
				UPDBG("devinfo->parentnwkaddr=0x%x\n", devinfo->parentnwkaddr);
				UPDBG("devinfo->sensortype=0x%x\n", devinfo->sensortype);
				UPDBG("devinfo->sensorvalue0x%x\n", devinfo->sensorvalue);
                                UPDBG("devinfo->status=0x%x\n", devinfo->status);//by gxj
			}
			continue;
		 }		 
		 
/*				
                // test tty_write cmd.
                CMD_NwkDetect();
                CMD_GetSensorStatus(0x796F);
                CMD_GetZigBeeNwkTopo();
*/		
	
        }

        return NULL;
}
/*--------------------------------------------------------*/
/* modem input handler */
void* ComRevPthread(void * data)
{

    struct timeval tv;
    fd_set rfds;

    tv.tv_sec=15;
    tv.tv_usec=0;

    int nread;
    int i,j,ret,datalen;

    unsigned char buff[BUFSIZE]={0,};
    unsigned char databuf[BUFSIZE]={0,};
    ret = 0;
    //UPDBG("read zb modem\n");

    //pthread_detach(pthread_self());
    while (STOP==FALSE)
    {
        //UPDBG("zb phread wait...\n");

        tv.tv_sec=15;
        tv.tv_usec=0;

        FD_ZERO(&rfds);
        FD_SET(zb_fd, &rfds);
        /*
 On  success,  select() and pselect() return the number of file descrip-
       tors contained in the three returned  descriptor  sets  (that  is,  the
       total  number  of  bits  that  are set in readfds, writefds, exceptfds)
       which may be zero if the timeout expires  before  anything  interesting
       happens.  On error, -1 is returned, and errno is set appropriately
        */
        ret = select(1+zb_fd, &rfds, NULL, NULL, &tv);
        //if (select(1+fd, &rfds, NULL, NULL, &tv)>0)
        if(ret >0)
        {
            UPDBG("zb select wait...\n");
            if (FD_ISSET(zb_fd, &rfds))
            {

                gNwkStatusFlag = 0x01;// any data of uart can flag it's status.
                nread=tty_read(zb_fd,buff, 1);
            //    UPDBG("fun:%s,line :%d\n",__FUNCTION__,__LINE__);
                buff[nread]='\0';
                //UPDBG("0x%x\n",buff[0]);
//#if 0
/*
                if(buff[0]=='\r'){
                    UPDBG("find return\n");
                    nread=tty_read(zb_fd,buff, 1);
                    UPDBG("readlen=%d\n", nread);
                    buff[nread]='\0';
                    UPDBG("0x%x\n",buff[0]);
*/
                    //@wei Acturally , it should be 0x01
  /*************--cmd frame format--****************/
  //
   /***  | sop | cmd | len | data | fcs |  ***/
   /***  |  1  |  2  |  1  | len  |  1  |  ***/
                    if(buff[0]==0x02){//0x0002 

                        i=0;
                        databuf[i] = buff[0];
                        i++;
                        nread=tty_read(zb_fd,buff, 2);
                        //UPDBG("readlen=%d\n", nread);
                        databuf[i] = buff[0];
                        i++;
                        databuf[i] = buff[1];
                        //len,
                        //databuf: len,channel[0],channel[1],
                        i++;
                        /*UPDBG("%x",buff[0]);
                        UPDBG("%x",buff[1]);
                        UPDBG("\n");*/

                         gCmdValidFlag = CMD_CalcFCS(buff, 2);
                         if(gCmdValidFlag == 1){
                                UPDBG("cmd is valid\n");
                                nread=tty_read(zb_fd,buff, 1);
                                //UPDBG("readlen=%d\n", nread);
                                databuf[i] = buff[0];
                                datalen = buff[0];
                                //UPDBG("datalen=%x\n",datalen);
                                i++;
                                if(datalen!=0){
                                    nread=tty_read(zb_fd,buff, datalen);
                                    ///UPDBG("readlen=%d\n", nread);
                                    for(j=0;j<nread;j++,i++){
                                        databuf[i]= buff[j];
                                    }
                                }
                                nread=tty_read(zb_fd,buff, 1);
                                ///UPDBG("readlen=%d\n", nread);
                                ///UPDBG("rCalcFcs:%x\n",buff[0]);

                                databuf[datalen+1+2+1]= Data_CalcFCS(databuf+1, datalen+3);
                                ///UPDBG("cCalcFcs:%x\n",databuf[datalen+1+2+1]);
                                //unsigned char NOFCS = 1;
                                if(databuf[datalen+1+2+1]==buff[0]){//||NOFCS == 1){
                                    ///UPDBG("CalcFcs OK\n");
                                      gFrameValidFlag = 0x01;
                                      gNwkStatusFlag = 0x01;// recover the coord's status.
                                }
                                  else{
                                        gFrameValidFlag = 0x0;
                                        continue;
                                  }
                                  UPDBG("\n---ZIGBEE DATA:");
                                for(j=0;j<(datalen+1+2+1+1);j++){
                                    UPDBG("%-4x",databuf[j]);
                                }
                                ///UPDBG("\n");

                                  if(gFrameValidFlag == 0x01){
                                        Data_PackageParser(databuf, datalen+1+2+1);
                                  }
                            //if() //async @wei
                         }

                    }
                //}// buff='\r'
//#endif

            }
             else{
                        UPDBG("not tty zb_fd.\n");
             }
        }
        else if(ret == 0){
        		//UPDBG("USER # LINE :991 time second :%d \n",tv.tv_sec);
        	    //if no 
        		static int count;
        		//CMD_ZIGBEE_RESTART();
                Udbg("zb read wait timeout!!!\n");
                gNwkStatusFlag = 0x00;
                DeviceNodeDestory();  //by @wei command for test

        }
        else{// ret <0
                UPDBG("zb select error.\n");
                //perror(ret);
        }

    }

    UPDBG("exit from reading zb com\n");
    return NULL;
}

/*--------------------------------------------------------*/
void* ComSendPthread(void * data)
{
	int i;
        UPDBG("zb send com cmd\n");
		//Data_CalcFCS(txbuff+1, (unsigned char)3);
	//pthread_detach(pthread_self());
        while (STOP==FALSE)
        {
           	//CMD_NwkDetect();
           	//while(SEND_PAUSE)
           	//	usleep(500);
        	pthread_mutex_lock(&mutex_trans); // by @wei
        	//usleep(500000);
           	CMD_GetZigBeeNwkTopo();
           	//usleep(500000);
           	Udbg("h\n");
           	pthread_mutex_unlock(&mutex_trans);
        	sleep(2);
               //usleep(600000);
		//usleep(600000);
	
        }

        return NULL; /* wait for child to die or it will become a zombie */
}

void* MsgRevPthread(void )
{
int reval;
int rflags=IPC_NOWAIT|MSG_NOERROR;
while (STOP==FALSE)
{
    reval=msgrcv(msgid,&msg_rbuf,sizeof(msg_rbuf.phone),10,rflags);
if (reval==-1)
{
    UPDBG("Read msg error!\n");
    //break;
}
else
{UPDBG("%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%gprs_msg%s%%%%%%%%%%%%%%%%%%%%%%%%%%%%%",msg_rbuf.phone);
    int i=0;
    UPDBG("msg_rbuf.phone:\n");
    for(i=0;i<12;i++)
    {
        UPDBG("%c",msg_rbuf.phone[i]);
    }
    gprs_msg(msg_rbuf.phone, msg_rbuf.phone[11]);

}
usleep(50000);
usleep(50000);
usleep(50000);
usleep(50000);
usleep(50000);
}
}

int ComPthreadMonitorStart(void)
{
	gNwkStatusFlag = 0x0;
	gFrameValidFlag = 0x0;
	gCmdValidFlag = 0x0;
	gIntLock = 0x0;

	gBTStatusFlag =0x0;
	gBTDatas = 0x0;
	gRFDatas = 0x0;
	
        tty_init(&zb_fd, "/dev/ttySAC1",ZB_BAUDRATE);
        //tty_init(&bt_fd, "/dev/ttySAC2",BT_BAUDRATE);
        //tty_init(&rf_fd, "/dev/ttySAC3",RF_BAUDRATE);	
        //tty_init(&gprs_fd, "/dev/ttyS0",GPRS_BAUDRATE);
		
	 //gprs_init();
		
        sa.sa_handler = SigChild_Handler;
        sa.sa_flags = 0;
        sigaction(SIGCHLD,&sa,NULL); /* handle dying child */
		
	    pthread_mutex_init(&mutex, NULL);
	    pthread_mutex_init(&mutex_trans,NULL);

        pthread_create(&th_kb, NULL, KeyBoardPthread, 0);
        //@wei recv thread created here.
        pthread_create(&th_rev, NULL, ComRevPthread, 0);
        //@wei keep update 
        pthread_create(&th_send, NULL, ComSendPthread, 0);
		
//	 pthread_create(&bt_rev, NULL, BlueToothRevPthread, 0);	
  //       pthread_create(&rf_rev, NULL, RFIDRevPthread, 0);
         //pthread_create(&msg_rev, NULL, MsgRevPthread, 0);

	return 0;
}

int ComPthreadMonitorExit(void)
{

        pthread_join(th_kb, &retval);
        pthread_join(th_rev, &retval);
        //pthread_join(th_send, &retval);

    //    pthread_join(bt_rev, &retval);		
      //  pthread_join(rf_rev, &retval);	

        //pthread_join(msg_rev, &retval);
	pthread_mutex_destroy(&mutex);
         //gprs_exit();
        tty_end(zb_fd);
        //tty_end(bt_fd);
        //tty_end(rf_fd);		
        //tty_end(gprs_fd);	
		
	DeviceNodeDestory();

	return 0;
}
