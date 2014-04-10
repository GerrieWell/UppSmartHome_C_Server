//##########################################################################
�ļ�˵����
//##########################################################################
-tty.h	���ײ㴮�ڶ�д����ʼ���ӿ�ͷ�ļ���
-zigbee_ctrl.h	��zigbee�豸�����ײ�ӿ�ͷ�ļ������������ڵ�Ĵ��������ӡ�ɾ�������������ҵȡ�
-tty_ctrl.h	:	zigbee�豸������API��װ�ӿ�ͷ�ļ��������Ѷ���Ķ�zigbee���缰�豸�Ĺ���API���ϲ�ֱ��ʹ�ø��ļ��ṩ�ӿڼ��ɡ�

//###########################################################################
��Ҫ���ݼ�����˵����tty_ctrl.h����
//���ݲ���
typedef struct{
    unsigned int  panid;// 16bit PANID��ʶ
    unsigned long int  channel;//32bits �����ŵ�
    unsigned char  maxchild;// ����ӽڵ���Ŀ
    unsigned char  maxdepth;// ����������
    unsigned char  maxrouter;// ���·�ɽڵ���Ŀ(��ǰ��)
}NwkDesp,*pNwkDesp;
�ṹ��˵������ʾzigbee���������Ϣ��

typedef struct{
    unsigned int  nwkaddr;//16bit�����ַ
    unsigned char  	sensortype;// ����������
    unsigned long int  sensorvalue;// 32bits ����������
} SensorDesp,*pSensorDesp;
�ṹ��˵������ʾzigbee�������ڵ�Ļ�����Ϣ��
sensortypeֵ���£�
						0��  SHT11������        
						1��  IRDA������             
						2��  SMOG������         
						3��  INT������
						4��  MICP������
						5��  SET������
						6��  �޴�����


typedef struct{
    unsigned int  nwkaddr;//16bit�����ַ
    unsigned char  macaddr[8];//8�ֽ�64bit������ַ
    unsigned char  depth;// �ڵ��������
    unsigned char  devtype;// �ڵ��豸���ͣ�0��ʾЭ������1��ʾ·������2��ʾ��ͨ�ڵ�
    unsigned int  parentnwkaddr;//16bit���ڵ������ַ
    unsigned char  sensortype;// ��ǰzigbee�ڵ�Ĵ���������
    unsigned long int  sensorvalue;//��ǰzigbee�ڵ�Ĵ���������
    unsigned char  status;// ��ǰzigbee�ڵ������״̬
}DeviceInfo,*pDeviceInfo;
�ṹ��˵������ʾzigbee�ڵ�Ļ�����Ϣ��

struct NodeInfo{
    DeviceInfo *devinfo;// ͬ��
    NodeInfo *next;// ����ָ����
    unsigned char row;// ����
    unsigned char num;// ����
};
�ṹ��˵������ʾzigbee�ڵ��������
NodeInfo *NodeInfoHead=NULL;// ȫ�ֵ�zigbee�ڵ�����ͷ��



//��������
int ZigBeeNwkDetect(void);
���ܣ����Э�����Ƿ��ϵ硣
��������
����ֵ�����Ρ�
0 Э����δ�ϵ磬1 Э�����ϵ硣


NwkDesp *GetZigBeeNwkDesp(void);
���ܣ���ȡ��ǰzigbee����Ļ�����Ϣ��
��������
����ֵ��NwkDespָ�롣


int SetSensorWorkMode(unsigned char Mode);
���ܣ�����zigbee�����д���������ģʽ(ֻ����ж��ʹ�����)��
������0��ѯģʽ��1�Զ�����ģʽ��
����ֵ�����Σ�0�ɹ�����0ʧ�ܡ�


int SetSensorStatus(unsigned int nwkaddr, unsigned  int status);
���ܣ�����zigbee�����д�����״̬(ֻ��������ʹ�����)��
������nwkaddr �������ڵ������ַ��status ״̬��0����IO�͵�ƽ��1����IO�ߵ�ƽ��
����ֵ�����Σ�0�ɹ�����0ʧ�ܡ�


SensorDesp *GetSensorStatus(unsigned int nwkaddr);
���ܣ���ȡ��ǰzigbee����ڵ�Ĵ�����״̬��
������zigbee����ڵ������ַ
����ֵ��SensorDespָ�롣

DeviceInfo* GetZigBeeDevInfo(unsigned int nwkaddr);
���ܣ���ȡ��ǰzigbee����ڵ���豸��Ϣ��
������zigbee����ڵ������ַ
����ֵ��DeviceInfoָ�롣


NodeInfo *GetZigBeeNwkTopo(void);
���ܣ���ȡ��ǰzigbee����ڵ�����˽ṹ����������
��������
����ֵ��DeviceInfoָ�룬������zigbee�ڵ���Ϣ������ͷ��



int ComPthreadMonitorStart(void);
���ܣ�zigbee���ڼ����߳̿����������������𴴽����ڼ����̣߳���������Ӧ�������ݰ���
			Ӧ�ó�����Ҫ���øú��������Ը��¼��zigbee������Ϣ���ڵ�״̬��
��������
����ֵ�����Σ�0�ɹ�����0ʧ�ܡ�


int ComPthreadMonitorExit(void);
���ܣ�zigbee���ڼ����̹߳رպ�����
��������
����ֵ�����Σ�0�ɹ�����0ʧ�ܡ� 