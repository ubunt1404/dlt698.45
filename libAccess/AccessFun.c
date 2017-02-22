#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <ctype.h>
#include <dirent.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "AccessFun.h"
#include "filebase.h"
#include "StdDataType.h"
#include "Objectdef.h"
#include "EventObject.h"
#include "ParaDef.h"
#include "PublicFunction.h"

#define 	LIB_ACCESS_VER 			0x0001

CLASS_INFO	info={};

void clearData()
{
	//冻结类数据清除
	system("rm -rf /nand/freeze");
}

void clearEvent()
{
	//事件类数据清除
	INT8U*	eventbuff=NULL;
	int 	saveflg=0,i=0;
	int		classlen=0;
	Class7_Object	class7={};

	for(i=0; i < sizeof(event_class_len)/sizeof(EVENT_CLASS_INFO);i++)
	{
		if(event_class_len[i].oi) {
			classlen = event_class_len[i].classlen;
			eventbuff = (INT8U *)malloc(classlen);
			if(eventbuff!=NULL) {
				memset(eventbuff,0,classlen);
				fprintf(stderr,"i=%d, oi=%04x, size=%d\n",i,event_class_len[i].oi,classlen);
				saveflg = 0;
				saveflg = readCoverClass(event_class_len[i].oi,0,(INT8U *)eventbuff,classlen,event_para_save);
				fprintf(stderr,"saveflg=%d oi=%04x\n",saveflg,event_class_len[i].oi);
//				int		j=0;
//				INT8U	val;
//				for(j=0;j<classlen;j++) {
//					val = (INT8U )eventbuff[j];
//					fprintf(stderr,"%02x ",val);
//				}
//				fprintf(stderr,"\n");
				if(saveflg) {
					memcpy(&class7,eventbuff,sizeof(Class7_Object));
					fprintf(stderr,"修改前：i=%d,oi=%x,class7.crrentnum=%d\n",i,event_class_len[i].oi,class7.crrentnum);
					if(class7.crrentnum!=0) {
						class7.crrentnum = 0;			//清除当前记录数
						memcpy(eventbuff,&class7,sizeof(Class7_Object));
						saveflg = saveCoverClass(event_class_len[i].oi,0,eventbuff,classlen,event_para_save);
					}
				}
				free(eventbuff);
				eventbuff=NULL;
			}
		}
	}
	system("rm -rf /nand/event/record");
	system("rm -rf /nand/event/current");
}

void clearDemand()
{
	//需量类数据清除
	system("rm -rf /nand/demand");
}


/*
 * 数据区初始化接口函数
 * 返回值 =0: 删除成功
 * =-1：删除失败
 * */
int dataInit(INT16U attr)
{
    struct timeval start={}, end={};
    long  interval=0;
	fprintf(stderr,"[4300]设备参数 属性：%d\n",attr);

 	gettimeofday(&start, NULL);
 	switch(attr) {
	case 3://数据初始化
		clearData();
		clearEvent();
		clearDemand();
		break;
	case 5://事件初始化
		clearEvent();
		break;
	case 6://需量初始化
		clearDemand();
		break;
	}

	gettimeofday(&end, NULL);
	interval = 1000000*(end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec);
    fprintf(stderr,"dataInit interval = %f(ms)\n", interval/1000.0);
 	return 0;
}

/*
 * 通过配置序号删除配置单元
 * 输入参数：oi对象标识，seqnum:要删除的配置序号
 * 返回值：=1：配置单元删除成功
 * =-1:  未查找到OI类数据
 */
int delClassBySeq(OI_698 oi,void *blockdata,int seqnum)
{
	int 	ret=-1;
	INT16S	infoi=-1;
	sem_t   *sem_save=NULL;

	sem_save = InitSem();

	infoi = getclassinfo(oi,&info);
	if(infoi == -1) {
		CloseSem(sem_save);
		return -1;
	}
	if(class_info[infoi].interface_len!=0) {		//该存储单元内部包含的类的公共属性
		if(seqnum>0)
			WriteInterfaceClass(oi,seqnum,Delete);
	}
	if(blockdata==NULL) {
		blockdata = malloc(class_info[infoi].unit_len);
		if(blockdata!=NULL) {
			memset(blockdata,0,class_info[infoi].unit_len);
		}
	}
	ret = save_block_file((char *)class_info[infoi].file_name,blockdata,class_info[infoi].unit_len,class_info[infoi].interface_len,seqnum);
	free(blockdata);
	CloseSem(sem_save);
	return ret;
}

/*
 * 方法：Clean()清空
 * 输入参数：oi对象标识
 * 返回值：=0：配置单元删除成功
 * =-1:  删除错误
 */
int clearClass(OI_698 oi)
{
	INT16S	infoi=-1;
	int		ret = -1;
	char	fname2[FILENAMELEN]={};
	char	cmd[FILENAMELEN]={};
	INT8U	oiA1=0;
	sem_t   *sem_save=NULL;

	sem_save = InitSem();

	infoi = getclassinfo(oi,&info);
	if(infoi==-1) {
		memset(cmd,0,sizeof(cmd));
		oiA1 = (oi & 0xf000) >> 12;
		switch(oiA1) {
		case 3:			//事件类
			sprintf(cmd,"rm -rf /%s/%04x",EVENT_PORP,oi);
			break;
		case 4:			//参变量类
		case 6:			//采集监控类
			sprintf(cmd,"rm -rf %s/%04x/",PARADIR,oi);
			break;
		}
		system(cmd);
		CloseSem(sem_save);
		return 1;
	}
	memset(fname2,0,sizeof(fname2));
	strncpy(fname2,class_info[infoi].file_name,strlen(class_info[infoi].file_name)-4);
	strcat(fname2,".bak");

	ret = unlink(class_info[infoi].file_name);
	ret = unlink(fname2);
	CloseSem(sem_save);
	return ret;
}

/*
 * 方法：Delete() 删除一个配置单元
 * 输入参数：oi对象标识，id:索引
 * 返回值：=0：配置单元删除成功
 * =-1:  删除错误
 */
int deleteClass(OI_698 oi,INT8U id)
{
	char	cmd[FILENAMELEN]={};

	memset(cmd,0,sizeof(cmd));
	sprintf(cmd,"rm -rf %s/%04x/%d.par",PARADIR,oi,id);
	system(cmd);
	sprintf(cmd,"rm -rf %s/%04x/%d.bak",PARADIR,oi,id);
	system(cmd);
	return 1;
}

/*
 * 接口类公共属性读取
 * 输入参数：oi对象标识，blockdata：主文件块缓冲区，size:主文件尺寸，index:文件的存储索引位置
 * 返回值：
 * =1：文件读取成功   =0：文件读取失败   =-1:  未查找到OI类数据信息
 */
//TODO: 未读取备份文件的接口类内容进行判断
int	readInterClass(OI_698 oi,void *dest)
{
	FILE 	*fp=NULL;
	int		num = 0;
	INT16S	infoi=-1;
//	CLASS_INFO	info={};

	infoi = getclassinfo(oi,&info);
	if(infoi==-1) {
		return -1;
	}
	fp = fopen(class_info[infoi].file_name, "r");
	if (fp != NULL) {
		num=fread(dest,1 ,class_info[infoi].interface_len,fp);
		fclose(fp);
	}
	return num;
};
/*
 * 参数类存储
 * 输入参数：oi对象标识，blockdata：主文件块缓冲区，seqnum:对象配置单元序列号，作为文件位置索引
 * 返回值：=1：文件保存成功，=0，文件保存失败，此时建议产生ERC2参数丢失事件通知主站异常
 * =-1:  未查找到OI类数据
 */

int saveParaClass(OI_698 oi,void *blockdata,int seqnum)
{
	int 	ret=-1;
	INT16U	infoi=-1;
	sem_t   *sem_save=NULL;

	sem_save = InitSem();
	infoi = getclassinfo(oi,&info);
	if(infoi == -1) {
		CloseSem(sem_save);
		return -1;
	}
	if(class_info[infoi].interface_len!=0) {		//该存储单元内部包含的类的公共属性
		WriteInterfaceClass(oi,seqnum,AddUpdate);
	}
	ret = save_block_file((char *)class_info[infoi].file_name,blockdata,class_info[infoi].unit_len,class_info[infoi].interface_len,seqnum);
	CloseSem(sem_save);
	return ret;
}

/*
 * 根据OI、配置序号读取某条配置单元内容
 * 输入参数：oi对象标识，seqnum:对象配置单元序列号
 * 返回值：
 * =1：文件读取成功，blockdata：配置单元内容
 * =0： 文件读取失败
 * =-1:  未查找到OI类数据
 */
int  readParaClass(OI_698 oi,void *blockdata,int seqnum)
{
	int 	ret=-1;
	INT16U	infoi=-1;
	sem_t   *sem_save=NULL;

	sem_save = InitSem();
	infoi = getclassinfo(oi,&info);
	if(infoi==-1) {
		return -1;
	}
	ret = block_file_sync((char *)class_info[infoi].file_name,blockdata,class_info[infoi].unit_len,class_info[infoi].interface_len,seqnum);
	CloseSem(sem_save);
	return ret;
}

/////////////////////////////////////////////////////////////////////////////////////////

/*
 * 输入参数：	oi:对象标识，seqno:记录序号，blockdata:存储数据，savelen：存储长度，
 * 			type：存储类型【	根据宏定义SaveFile_type 】
 * 返回值：=1：文件存储成功
 */
int saveCoverClass(OI_698 oi,INT16U seqno,void *blockdata,int savelen,int type)
{
	int		ret = 0;
	char	fname[FILENAMELEN]={};
	sem_t   *sem_save=NULL;

	sem_save = InitSem();
	memset(fname,0,sizeof(fname));
	getFileName(oi,seqno,type,fname);
	switch(type) {
	case event_para_save:
	case para_vari_save:
	case coll_para_save:
	case acs_coef_save:
	case acs_energy_save:
	case para_init_save:
	case calc_voltage_save:
		fprintf(stderr,"saveClass file=%s ",fname);
		ret = save_block_file(fname,blockdata,savelen,0,0);
		break;
	case event_record_save:
	case event_current_save:
		writeCoverFile(fname,blockdata,savelen);
		break;
	}
	CloseSem(sem_save);
	return ret;
}

/*
 * 输入参数：	oi:对象标识，seqno:记录序号，
 * 			type：存储类型【	根据宏定义SaveFile_type 】
 * 返回值：相关对象标识的类的存储文件长度
 * =-1: 无效数据
 */
int getClassFileLen(OI_698 oi,INT16U seqno,int type)
{
	int		filelen = -1;
	char	fname[FILENAMELEN]={};

	memset(fname,0,sizeof(fname));
	getFileName(oi,seqno,type,fname);
	filelen = getFileLen(fname);
	return filelen;
}
/*
 * 输入参数：	oi:对象标识，seqno:记录序号，blockdata:存储数据，savelen：存储长度，
 * 			type：存储类型【	根据宏定义SaveFile_type 】
 * 返回值：=1：文件存储成功
 * =-1: 文件不存在
 */
int readCoverClass(OI_698 oi,INT16U seqno,void *blockdata,int datalen,int type)
{
	int		ret = 0;
	char	fname[FILENAMELEN]={};
//	int		readlen = 0;
	sem_t   *sem_save=NULL;
//	void 	*blockdata1=NULL;

	sem_save = InitSem();
	memset(fname,0,sizeof(fname));
	switch(type) {
	case event_para_save:
	case para_vari_save:
	case coll_para_save:
	case acs_coef_save:
	case acs_energy_save:
		ret = readFileName(oi,seqno,type,fname);
		if(ret==0) {		//文件存在
			fprintf(stderr,"readClass %s filelen=%d\n",fname,datalen);
			ret = block_file_sync(fname,blockdata,datalen,0,0);
//			fprintf(stderr,"ret=%d\n",ret);
		}else  {		//无配置文件，读取系统初始化参数
			memset(fname,0,sizeof(fname));
			readFileName(oi,seqno,para_init_save,fname);
//			fprintf(stderr,"read /nor/init的参数文件：  Class %s filelen=%d\n",fname,datalen);
			ret = block_file_sync(fname,blockdata,datalen,0,0);
		}
		break;
	case para_init_save:
		ret = readFileName(oi,seqno,type,fname);
		fprintf(stderr,"readClass %s filelen=%d\n",fname,datalen);
		if(ret==0) {
			ret = block_file_sync(fname,blockdata,datalen,0,0);
		}
	break;
	case event_record_save:
	case event_current_save:
		ret = readFileName(oi,seqno,type,fname);
		if(ret==0) {
			ret = readCoverFile(fname,blockdata,datalen);
		}
		break;
	}
	//信号量post，注意正常退出
	CloseSem(sem_save);
	return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////

/*
 * 计算某个普通采集方案一天需要存几次，是否与采集任务里面的执行频率有关？
 */
INT16U CalcFreq(CLASS_6015 class6015)
{
	INT16U freq=0;
	INT8U  unit=0;
	INT16U value=0;
	switch(class6015.data.type)
	{
	case 0://采集当前数据
	case 1://采集上第n次
	case 2://按冻结时标采集
		freq = 1;
		break;
	case 3://按时标间隔采集
		unit = class6015.data.data[0];
		value = (class6015.data.data[1]<<8) + class6015.data.data[2];
		break;
	}
	if(class6015.data.type == 3)
	{
		switch(unit)
		{
		case 0:
			freq = 24*60*60/value;
			break;
		case 1:
			freq = 24*60/value;
			break;
		case 2:
			freq = 24/value;
			break;
		case 3:
		case 4:
		case 5:
			freq = 1;
			break;
		}
	}
	return freq;
}
void getTaskFileName(INT8U taskid,TS ts,char *fname)
{
	char dirname[FILENAMELEN]={};
	if (fname==NULL)
		return ;
	memset(fname,0,FILENAMELEN);
	sprintf(dirname,"%s",TASKDATA);
	makeSubDir(dirname);
	sprintf(dirname,"%s/%03d",TASKDATA,taskid);
	makeSubDir(dirname);
	sprintf(fname,"%s/%03d/%04d%02d%02d.dat",TASKDATA,taskid,ts.Year,ts.Month,ts.Day);
//	fprintf(stderr,"getFileName fname=%s\n",fname);
}
INT16U GetFileOadLen(INT8U units,INT8U tens)//个位十位转化为一个INT16U
{
	INT16U total = 0;
	total = tens;
	return (total<<8)+units;
}
/*
 * 返回位置和长度
 */
int GetPosofOAD(INT8U *file_buf,OAD oad_master,OAD oad_relate,HEAD_UNIT *head_unit,INT8U unitnum,INT8U *tmpbuf)
{
	int i=0,j=0,datapos=0,curlen=0,valid_cnt = 0;
	OAD oad_hm,oad_hr;
	fprintf(stderr,"\n--GetPosofOAD--unitnum=%d\n",unitnum);
	memset(&oad_hm,0xee,sizeof(OAD));
	memset(&oad_hr,0xee,sizeof(OAD));
	for(i=0;i<unitnum;i++)
	{
		curlen = GetFileOadLen(head_unit[i].num[1],head_unit[i].num[0]);
		fprintf(stderr,"\n%02x %02x\n",head_unit[i].num[1],head_unit[i].num[0]);
		fprintf(stderr,"\n-head_unit[%d].type=%d-GetPosofOAD--curlen=%d,valid_cnt=%d\n",i,head_unit[i].type,curlen,valid_cnt);
		if(head_unit[i].type == 1)
		{
			memcpy(&oad_hr,&head_unit[i].oad,sizeof(OAD));
			memset(&oad_hm,0xee,sizeof(OAD));
			valid_cnt = curlen;
			continue;
		}
		if(valid_cnt==0)//关联oad失效
			memset(&oad_hr,0xee,sizeof(OAD));
		if(valid_cnt>0)
			valid_cnt--;//road包括cnt个oad，在非零前，关联oad有效
		if(head_unit[i].type == 0)
		{
			memcpy(&oad_hm,&head_unit[i].oad,sizeof(OAD));
		}
		fprintf(stderr,"\nhead master oad:%04x%02x%02x\n",oad_hm.OI,oad_hm.attflg,oad_hm.attrindex);
		fprintf(stderr,"\nhead relate oad:%04x%02x%02x\n",oad_hr.OI,oad_hr.attflg,oad_hr.attrindex);
		fprintf(stderr,"\nbaow master oad:%04x%02x%02x\n",oad_master.OI,oad_master.attflg,oad_master.attrindex);
		fprintf(stderr,"\nbaow relate oad:%04x%02x%02x\n",oad_relate.OI,oad_relate.attflg,oad_relate.attrindex);
		if(memcmp(&oad_hm,&oad_master,sizeof(OAD))==0 && memcmp(&oad_hr,&oad_relate,sizeof(OAD))==0)
		{
			fprintf(stderr,"\n-----oad equal!\n");
			memcpy(tmpbuf,&file_buf[datapos],curlen);
			fprintf(stderr,"\n文件中的数据:");
			for(j=0;j<400;j++)
			{
				if(j%20==0)
					fprintf(stderr,"\n");
				fprintf(stderr," %02x",file_buf[j]);
			}
			fprintf(stderr,"\n");
			fprintf(stderr,"\n文件中取出的数据(%d)datapos=%d:",curlen,datapos);
			for(j=0;j<curlen;j++)
				fprintf(stderr," %02x",tmpbuf[j]);
			fprintf(stderr,"\n");
			return curlen;
		}
		else
			fprintf(stderr,"\n-----oad not equal!\n");
		if(head_unit[i].type == 0)
			datapos += curlen;

		if(valid_cnt==0)
		{
			memset(&oad_hr,0xee,sizeof(OAD));
		}

	}
	return 0;
}
/*
 * 读取抄表数据，读取某个测量点某任务某天一整块数据，放在内存里，根据需要提取数据,并根据csd组报文
 */
int ComposeSendBuff(TS *ts,INT8U seletype,INT8U taskid,TSA *tsa_con,INT8U tsa_num,CSD_ARRAYTYPE csds,INT8U *SendBuf)
{
	OAD  oadm,oadr;
	FILE *fp  = NULL;
	INT16U headlen=0,blocklen=0,unitnum=0,freq=0,sendindex=0,retlen=0,schpos=0;
	INT8U *databuf_tmp=NULL;
	INT8U tmpbuf[256];
	INT8U headl[2],blockl[2];
	int i=0,j=0,k=0,m=0;
	CLASS_6015	class6015={};
	CLASS_6013	class6013={};
	HEAD_UNIT *head_unit = NULL;
	TS ts_now;
	TSGet(&ts_now);
	char	fname[FILENAMELEN]={};
	memset(&class6013,0,sizeof(CLASS_6013));
	readCoverClass(0x6013,taskid,&class6013,sizeof(class6013),coll_para_save);
	memset(&class6015,0,sizeof(CLASS_6015));
	readCoverClass(0x6015,class6013.sernum,&class6015,sizeof(CLASS_6015),coll_para_save);
	freq = CalcFreq(class6015);
	////////////////////////////////////////////////////////////////////////////////test
//	memset(&class6015,0xee,sizeof(CLASS_6015));
//	class6015.csds.num = 1;
//	class6015.csds.csd[0].type=1;
//	class6015.csds.csd[0].csd.road.oad.OI =0x5004;
//	class6015.csds.csd[0].csd.road.oad.attflg = 0x02;
//	class6015.csds.csd[0].csd.road.oad.attrindex = 0x00;
//	class6015.csds.csd[0].csd.road.num = 3;
//	class6015.csds.csd[0].csd.road.oads[0].OI = 0x2021;
//	class6015.csds.csd[0].csd.road.oads[0].attflg = 0x02;
//	class6015.csds.csd[0].csd.road.oads[0].attrindex = 0x00;
//	class6015.csds.csd[0].csd.road.oads[1].OI = 0x0010;
//	class6015.csds.csd[0].csd.road.oads[1].attflg = 0x02;
//	class6015.csds.csd[0].csd.road.oads[1].attrindex = 0x00;
//	class6015.csds.csd[0].csd.road.oads[2].OI = 0x0020;
//	class6015.csds.csd[0].csd.road.oads[2].attflg = 0x02;
//	class6015.csds.csd[0].csd.road.oads[2].attrindex = 0x00;
//	freq = 1;
//	taskid=1;
	//////////////////////////////////////////////////////////////////////////////////test

	getTaskFileName(taskid,ts_now,fname);
//	ReadFileHeadLen(fname,&headlen,&unitlen);
//	headbuf = (INT8U *)malloc(headlen);
//	unitnum = (headlen-4)/sizeof(HEAD_UNIT);
//	ReadFileHead(fname,headlen,unitlen,unitnum,headbuf);
//	databuf_tmp = malloc(unitlen);
//	savepos = (int *)malloc(tsa_num*sizeof(int));
//	fprintf(stderr,"\n-------------6--%s\n",fname);
	fp = fopen(fname,"r");
	if(fp == NULL)//文件没内容 组文件头，如果文件已存在，提取文件头信息
	{
		return 0;
	}
	else
	{
//		fprintf(stderr,"\n-------------7\n");
		fread(headl,2,1,fp);
		headlen = headl[0];
		headlen = (headl[0]<<8) + headl[1];
		fread(&blockl,2,1,fp);
//		fprintf(stderr,"\nblocklen=%02x %02x\n",blockl[1],blockl[0]);
		blocklen = blockl[0];
		blocklen = (blockl[0]<<8) + blockl[1];
		unitnum = (headlen-4)/sizeof(HEAD_UNIT);
		databuf_tmp = (INT8U *)malloc(blocklen);

		head_unit = (HEAD_UNIT *)malloc(headlen-4);
		fread(head_unit,headlen-4,1,fp);

		fseek(fp,headlen,SEEK_SET);//跳过文件头
		while(!feof(fp))//找存储结构位置
		{
//			fprintf(stderr,"\n-------------8---blocklen=%d,headlen=%d,tsa_num=%d\n",blocklen,headlen,tsa_num);
			//00 00 00 00 00 00 00 00 07 05 00 00 00 00 00 01
			//00 00 00 00 00 00 00 00 00 07 05 00 00 00 00 00 01
			memset(databuf_tmp,0xee,blocklen);
			if(fread(databuf_tmp,blocklen,1,fp) == 0)
				break;
			for(i=0;i<tsa_num;i++)
			{
				for(m=0;m<freq;m++)
				{
					schpos = m*blocklen/freq;
//					fprintf(stderr,"\n-------------9---i=%d\n",i);
//					fprintf(stderr,"\n1addr1:%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
//							databuf_tmp[schpos+16],databuf_tmp[schpos+15],databuf_tmp[schpos+14],databuf_tmp[schpos+13],
//							databuf_tmp[schpos+12],databuf_tmp[schpos+11],databuf_tmp[schpos+10],databuf_tmp[schpos+9],
//							databuf_tmp[schpos+8],databuf_tmp[schpos+7],databuf_tmp[schpos+6],	databuf_tmp[schpos+5],
//							databuf_tmp[schpos+4],databuf_tmp[schpos+3],databuf_tmp[schpos+2],databuf_tmp[schpos+1]);
//					fprintf(stderr,"\n1addr2:%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
//							tsa_con[i].addr[16],tsa_con[i].addr[15],tsa_con[i].addr[14],tsa_con[i].addr[13],
//							tsa_con[i].addr[12],tsa_con[i].addr[11],tsa_con[i].addr[10],tsa_con[i].addr[9],
//							tsa_con[i].addr[8],tsa_con[i].addr[7],tsa_con[i].addr[6],	tsa_con[i].addr[5],
//							tsa_con[i].addr[4],tsa_con[i].addr[3],tsa_con[i].addr[2],tsa_con[i].addr[1],tsa_con[i].addr[0]);
					if(memcmp(&databuf_tmp[schpos+1],&tsa_con[i].addr[1],16)==0)//找到了存储结构的位置，一个存储结构可能含有unitnum个单元
					{
						fprintf(stderr,"\naddr1:%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
								databuf_tmp[schpos+16],databuf_tmp[schpos+15],databuf_tmp[schpos+14],databuf_tmp[schpos+13],
								databuf_tmp[schpos+12],databuf_tmp[schpos+11],databuf_tmp[schpos+10],databuf_tmp[schpos+9],
								databuf_tmp[schpos+8],databuf_tmp[schpos+7],databuf_tmp[schpos+6],	databuf_tmp[schpos+5],
								databuf_tmp[schpos+4],databuf_tmp[schpos+3],databuf_tmp[schpos+2],databuf_tmp[schpos+1]);
						fprintf(stderr,"\naddr2:%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
								databuf_tmp[schpos+16],databuf_tmp[schpos+15],databuf_tmp[schpos+14],databuf_tmp[schpos+13],
								databuf_tmp[schpos+12],databuf_tmp[schpos+11],databuf_tmp[schpos+10],databuf_tmp[schpos+9],
								databuf_tmp[schpos+8],databuf_tmp[schpos+7],databuf_tmp[schpos+6],	databuf_tmp[schpos+5],
								databuf_tmp[schpos+4],databuf_tmp[schpos+3],databuf_tmp[schpos+2],databuf_tmp[schpos+1]);
//						fprintf(stderr,"\n------------10---find addr\n");
						for(j=0;j<csds.num;j++)
						{
//							fprintf(stderr,"\n-------%d:(type=%d)\n",j,csds.csd[j].type);
							if(csds.csd[j].type != 0 && csds.csd[j].type != 1)
								continue;
							if(csds.csd[j].type == 1)
							{
								SendBuf[sendindex++] = 0x01;//aray
								SendBuf[sendindex++] = csds.csd[j].csd.road.num;
								for(k=0;k<csds.csd[j].csd.road.num;k++)
								{
									memset(tmpbuf,0x00,256);
									memcpy(&oadm,&csds.csd[j].csd.road.oads[k],sizeof(OAD));
									memcpy(&oadr,&csds.csd[j].csd.road.oad,sizeof(OAD));
//									fprintf(stderr,"\nmaster oad:%04x%02x%02x\n",oadm.OI,oadm.attflg,oadm.attrindex);
//									fprintf(stderr,"\nrelate oad:%04x%02x%02x\n",oadr.OI,oadr.attflg,oadr.attrindex);
									if((retlen = GetPosofOAD(&databuf_tmp[schpos],oadm,oadr,head_unit,unitnum,tmpbuf))==0)
										SendBuf[sendindex++] = 0;
									else
									{
//										fprintf(stderr,"\n--type=1--tmpbuf[0]=%02x\n",tmpbuf[0]);
										if(tmpbuf[0]==0)//NULL
											SendBuf[sendindex++] = 0;
										else
										{
											memcpy(&SendBuf[sendindex],tmpbuf,retlen);
											sendindex += retlen;
										}
									}
//									fprintf(stderr,"\n---1----k=%d retlen=%d\n",k,retlen);
								}
							}
							if(csds.csd[j].type == 0)
							{
								if(csds.csd[j].csd.oad.OI == 0x4001)
								{
									SendBuf[sendindex++]=0x55;
									for(k=1;k<17;k++)
									{
//										fprintf(stderr,"\n--databuf_tmp[%d+%d]=%d\n",schpos,k,databuf_tmp[schpos+k]);
										if(databuf_tmp[schpos+k]==0)
											continue;
										memcpy(&SendBuf[sendindex],&databuf_tmp[schpos+k],databuf_tmp[schpos+k]+1);
										sendindex += databuf_tmp[schpos+k]+1;
										break;
									}
									continue;
								}
								memset(tmpbuf,0x00,256);
								memcpy(&oadm,&csds.csd[j].csd.oad,sizeof(OAD));
								memset(&oadr,0xee,sizeof(OAD));
								if((retlen = GetPosofOAD(&databuf_tmp[schpos],oadm,oadr,head_unit,unitnum,tmpbuf))==0)
									SendBuf[sendindex++] = 0;
								else
								{
//									fprintf(stderr,"\n--type=0--tmpbuf[0]=%02x\n",tmpbuf[0]);
									if(tmpbuf[0]==0)//NULL
										SendBuf[sendindex++] = 0;
									else
									{
										memcpy(&SendBuf[sendindex],tmpbuf,retlen);
										sendindex += retlen;
									}
								}
//								fprintf(stderr,"\n---0----k=%d retlen=%d\n",k,retlen);
							}
						}
	//					return sendindex;
						continue;
					}
				}
			}
		}
	}
	return sendindex;
}
/*
 * 根据招测类型组织报文
 * 如果MS选取的测量点过多，不能同时上报，分帧
 */
INT8U getSelector(RSD select, INT8U selectype, CSD_ARRAYTYPE csds, INT8U *data, int *datalen)
{
	TS ts_info[2];//时标选择，根据普通采集方案存储时标选择进行，此处默认为相对当日0点0分 todo
	INT8U taskid,tsa_num=0;
//	TSA tsa_con = NULL;
//	tsa_num = GetTsaNum(select);//得到tsa个数
//	tsa_con = malloc(tsa_num*sizeof(TSA));
//	GetTsaGroup(select,tsa_con);//计算得到tsa组
//	taskid = GetTaskId(rcsd);//根据rcsd得到应该去哪个任务里找，如果涉及到多个任务呢？应该不会
	//测试写死
	///////////////////////////////////////////////////////////////test
//	tsa_num = 3;
	TSA tsa_con[] = {
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0x05,0x00,0x00,0x00,0x00,0x00,0x01,
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0x05,0x00,0x00,0x00,0x00,0x00,0x02,
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0x05,0x00,0x00,0x00,0x00,0x00,0x03,
	};
//	fprintf(stderr,"\n-------------1\n");
//	memset(&csds,0xee,sizeof(CSD_ARRAYTYPE));
//	csds.num = 5;
//	csds.csd[0].type=0;
//	csds.csd[0].csd.oad.OI = 0x4001;
//	csds.csd[0].csd.oad.attflg = 0x02;
//	csds.csd[0].csd.oad.attrindex = 0;
//	csds.csd[1].type=0;
//	csds.csd[1].csd.oad.OI = 0x6040;
//	csds.csd[1].csd.oad.attflg = 0x02;
//	csds.csd[1].csd.oad.attrindex = 0;
//	csds.csd[2].type=0;
//	csds.csd[2].csd.oad.OI = 0x6041;
//	csds.csd[2].csd.oad.attflg = 0x02;
//	csds.csd[2].csd.oad.attrindex = 0;
//	csds.csd[3].type=0;
//	csds.csd[3].csd.oad.OI = 0x6042;
//	csds.csd[3].csd.oad.attflg = 0x02;
//	csds.csd[3].csd.oad.attrindex = 0;
//	csds.csd[4].type=1;
//	csds.csd[4].csd.road.oad.OI = 0x5004;
//	csds.csd[4].csd.road.oad.attflg = 0x02;
//	csds.csd[4].csd.road.oad.attrindex = 0;
//	csds.csd[4].csd.road.num = 3;
//	csds.csd[4].csd.road.oads[0].OI = 0x2021;
//	csds.csd[4].csd.road.oads[0].attflg = 0x02;
//	csds.csd[4].csd.road.oads[0].attrindex = 0;
//	csds.csd[4].csd.road.oads[1].OI = 0x0010;
//	csds.csd[4].csd.road.oads[1].attflg = 0x02;
//	csds.csd[4].csd.road.oads[1].attrindex = 0;
//	csds.csd[4].csd.road.oads[2].OI = 0x0020;
//	csds.csd[4].csd.road.oads[2].attflg = 0x02;
//	csds.csd[4].csd.road.oads[2].attrindex = 0;
//	csds.num = 1;
//	csds.csd[0].type=1;
//	csds.csd[0].csd.road.oad.OI = 0x5004;
//	csds.csd[0].csd.road.oad.attflg = 0x02;
//	csds.csd[0].csd.road.oad.attrindex = 0;
//	csds.csd[0].csd.road.num = 3;
//	csds.csd[0].csd.road.oads[0].OI = 0x2021;
//	csds.csd[0].csd.road.oads[0].attflg = 0x02;
//	csds.csd[0].csd.road.oads[0].attrindex = 0;
//	csds.csd[0].csd.road.oads[1].OI = 0x0010;
//	csds.csd[0].csd.road.oads[1].attflg = 0x02;
//	csds.csd[0].csd.road.oads[1].attrindex = 0;
//	csds.csd[0].csd.road.oads[2].OI = 0x0020;
//	csds.csd[0].csd.road.oads[2].attflg = 0x02;
//	csds.csd[0].csd.road.oads[2].attrindex = 0;
	taskid = 1;
	///////////////////////////////////////////////////////////////test
	fprintf(stderr,"\n-------------2 selectype=%d\n",selectype);
	switch(selectype)
	{
	case 5://例子中招测冻结数据，包括分钟小时日月冻结数据招测方法
		memcpy(&ts_info[0],&select.selec5.collect_save,sizeof(DateTimeBCD));
//		ReadNorData(ts_info,taskid,tsa_con,tsa_num);
		//////////////////////////////////////////////////////////////////////test
		TSGet(&ts_info[0]);
		//////////////////////////////////////////////////////////////////////test
		*datalen = ComposeSendBuff(&ts_info[0],selectype,taskid,tsa_con,tsa_num,csds,data);
		break;
	case 7://例子中招测实时数据方法
		*datalen = ComposeSendBuff(&ts_info[0],selectype,taskid,tsa_con,tsa_num,csds,data);
		break;
	default:
		break;
	}
	return 0;
}

