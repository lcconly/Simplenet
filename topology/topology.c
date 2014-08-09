//文件名: topology/topology.c
//
//描述: 这个文件实现一些用于解析拓扑文件的辅助函数 
//
//创建日期: 2013年
#include "topology.h"
//#define _XOPEN_SOURCE 500
#define MAXHOSTNAME 32   /*网络主机名最大字符串长度*/
#define MAXHOSTNUM 100   /*网络中主机数目最大值*/


//这个函数返回指定主机的节点ID.
//节点ID是节点IP地址最后8位表示的整数.
//例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromname(char* hostname) 
{
	char hostIP[32];
	/*struct hostent* hptr=NULL;
	printf("nimabi\n");
	hptr=gethostbyname(hostname); // 网络字节序
	if(hptr==NULL)
		printf("kongde\n");
	printf("type %d\n",hptr->h_addrtype);
	printf("daozhelile\n");
	inet_ntop(hptr->h_addrtype,hptr->h_addr,hostIP,MAXHOSTNAME);
	printf("hahhahah\n");*/
	gethostipbyname(hostname,hostIP);
	int hostlen=strlen(hostIP);
	int dot_num=0;
	int i=0;
	for(;i<hostlen;i++){
		if(hostIP[i]=='.')
			dot_num++;
		if(dot_num==3)  /*找到第3个点号*/
			break;
	}
	int j=i+1;/*检查剩下的子串里边是否都是数字*/
	for(;j<hostlen;j++){
		if(isdigit(hostIP[j])==0)
			return -1;
	}
	return atoi(hostIP+i+1);  /*将剩下的子串转换成整型ID返回*/
}

//这个函数返回指定的IP地址的节点ID.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromip(struct in_addr* addr)                //这个函数还没有好好测试
{
	char* hostIP=inet_ntoa(*addr);
	printf("hostIP %s\n",hostIP);
	//gethostipbyname(hostname,hostIP);
	int hostlen=strlen(hostIP);
	int dot_num=0;
	int i=0;
	for(;i<hostlen;i++){
		if(hostIP[i]=='.')
			dot_num++;
		if(dot_num==3)  /*找到第3个点号*/
			break;
	}
	int j=i+1;/*检查剩下的子串里边是否都是数字*/
	for(;j<hostlen;j++){
		if(isdigit(hostIP[j])==0)
			return -1;
	}
	return atoi(hostIP+i+1);  /*将剩下的子串转换成整型ID返回*/
}

//这个函数返回本机的节点ID
//如果不能获取本机的节点ID, 返回-1.
int topology_getMyNodeID()
{ 
	char buf[32];
	gethostname(buf,32);
	return topology_getNodeIDfromname(buf);
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回邻居数.
int topology_getNbrNum()
{

	/*本函数返回本机的邻居结点数*/
	FILE* fp=fopen("topology.dat","r");
	int nbr_num=0;
	char host_1[MAXHOSTNAME];
	char host_2[MAXHOSTNAME];
	int cost;
	char buf[32];
	gethostname(buf,32);
	if(fp!=NULL){
		while(fscanf(fp,"%s",host_1)>0){
			fscanf(fp,"%s",host_2);
			fscanf(fp,"%d",&cost);
			//printf("%s --- %s --- %s\n",buf,host_1,host_2);
			if(strcmp(buf,host_1)==0||strcmp(buf,host_2)==0)  /*一条链路代价信息中包含主机域名*/{
			    //printf("!!!!!!!!!!!!!!!\n");
                nbr_num++; 
            }
        }
		fclose(fp);
	}
	return nbr_num;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回重叠网络中的总节点数.
int topology_getNodeNum()
{ 
	/*可以用gethostbyname得到主机IP地址*/
	char host[MAXHOSTNUM][MAXHOSTNAME];  
	FILE *fp=fopen("topology.dat","r");
	int hostnum=0;   /*读到的主机数目*/
	if(fp!=NULL){
		int str_num=0;  /*读到的字符串数目*/
		while(fscanf(fp,"%s",host[hostnum])>0){	
			str_num++;
			if(str_num%3!=0)  /*读到的是主机名字而非代价值*/
			{
				int i=0;
				for(;i<hostnum;i++) /*判断主机是否已经存在*/
				{
					if(strcmp(host[hostnum],host[i])==0)
						break;
				}
				if(i==hostnum){  /*找到了新的主机*/
					hostnum++;
				}

			}
		}
		fclose(fp);
	}

 	return hostnum;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含重叠网络中所有节点的ID. 
int* topology_getNodeArray()
{
	int* ID_array=(int*)malloc(sizeof(int)*topology_getNodeNum());/*调用之前实现的函数来获取结点总数目*/
	FILE* fp=fopen("topology.dat","r");
	char host_1[MAXHOSTNAME];
	char host_2[MAXHOSTNAME];
	int cost;
	int ID_tail=0;
	if(fp!=NULL){
		while(fscanf(fp,"%s",host_1)>0){
			fscanf(fp,"%s",host_2);
			fscanf(fp,"%d",&cost);
			/*得到的是网络字节序，需要主那换为主机字节序*/
			int index=0;
			int ID_1=topology_getNodeIDfromname(host_1); 
			for(;index<ID_tail;index++){
				if(ID_array[index]==ID_1) /*已经存在*/
					break;
			}
			if(index==ID_tail){  /*添加新结点*/
				ID_array[ID_tail]=ID_1;
				ID_tail++;
			}
			int ID_2=topology_getNodeIDfromname(host_2);

			for(index=0;index<ID_tail;index++){
				if(ID_array[index]==ID_2) /*已经存在*/
					break;
			}
			if(index==ID_tail){  /*添加新结点*/
				ID_array[ID_tail]=ID_2;
				ID_tail++;
			}
			/*得到点分的IP地址，需要判断是否已经在ID数组中存在*/
		
		}
		
		fclose(fp);
	}
	
 	 return ID_array;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含所有邻居的节点ID.  
int* topology_getNbrArray()
{
	FILE* fp=fopen("topology.dat","r");
	char buf[32];
	gethostname(buf,32);   //取得主机名字
	char host_1[MAXHOSTNAME];
	char host_2[MAXHOSTNAME];
	int cost;
	int nbr_num=topology_getNbrNum();
	int* nbr_array=(int*)malloc(sizeof(int)*(nbr_num));
	int j=0;
	if(fp!=NULL){
		while(fscanf(fp,"%s",host_1)>0&&j<nbr_num){
			fscanf(fp,"%s",host_2);
			fscanf(fp,"%d",&cost);
			if(strcmp(buf,host_1)==0)
			{
				nbr_array[j]=topology_getNodeIDfromname(host_2);
				j++;
			}
			if(strcmp(buf,host_2)==0)
			{
				nbr_array[j]=topology_getNodeIDfromname(host_1);
				j++;
			}
				
		}
		fclose(fp);
	}
	return nbr_array;

}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回指定两个节点之间的直接链路代价. 
//如果指定两个节点之间没有直接链路, 返回INFINITE_COST.
unsigned int topology_getCost(int fromNodeID, int toNodeID)
{	
	if(fromNodeID==toNodeID)
		return 0;
	FILE* fp=fopen("topology.dat","r");
	char host_1[MAXHOSTNAME];
	char host_2[MAXHOSTNAME];
	int cost;
	if(fp!=NULL){
		while(fscanf(fp,"%s",host_1)>0){
			fscanf(fp,"%s",host_2);
			fscanf(fp,"%d",&cost);
			int ID_1=topology_getNodeIDfromname(host_1);
			int ID_2=topology_getNodeIDfromname(host_2);
			if((fromNodeID==ID_1&&toNodeID==ID_2)||(toNodeID==ID_1&&fromNodeID==ID_2)){
				fclose(fp);
				return cost;
			}
		}
	fclose(fp);
	}
	
  	return INFINITE_COST;
}
//根据结点ID得到IP地址，格式不符合返回0
void topology_getIP(int NodeID,char* ip){
	/*char host_1[MAXHOSTNAME];
	char host_2[MAXHOSTNAME];
	int cost;
	char hostIP[32];
	struct hostent* hptr;
	unsigned long ip_addr;
	FILE* fp=fopen("topology.dat","r");
	if(fp!=NULL){
		while(fscanf(fp,"%s",host_1)>0)
		{
			fscanf(fp,"%s",host_2);
			fscanf(fp,"%d",&cost);   
			int ID_1=topology_getNodeIDfromname(host_1);
			int ID_2=topology_getNodeIDfromname(host_2);
			if(ID_1==NodeID){
				hptr=gethostbyname(host_1); // 网络字节序
				inet_ntop(hptr->h_addrtype,hptr->h_addr,hostIP,MAXHOSTNAME);
				ip_addr=inet_addr(hostIP);
				break;
	
			}
			if(ID_2==NodeID){
				hptr=gethostbyname(host_2); // 网络字节序
				inet_ntop(hptr->h_addrtype,hptr->h_addr,hostIP,MAXHOSTNAME);
				ip_addr=inet_addr(hostIP);
				break;
			}

		}
	if(ip_addr!=INADDR_NONE){
		fclose(fp);
	  	return ip_addr;
	}
	}
	fclose(fp);
	return 0;*/
	char host_1[MAXHOSTNAME];
	char host_2[MAXHOSTNAME];
	int cost;
	//char hostIP[32];
	//struct hostent* hptr;
	//unsigned long ip_addr;
	FILE* fp=fopen("topology.dat","r");
	if(fp!=NULL){
		while(fscanf(fp,"%s",host_1)>0)
		{
			fscanf(fp,"%s",host_2);
			fscanf(fp,"%d",&cost);   
			int ID_1=topology_getNodeIDfromname(host_1);
			int ID_2=topology_getNodeIDfromname(host_2);
			if(ID_1==NodeID){
				gethostipbyname(host_1,ip);
				break;
	
			}
			if(ID_2==NodeID){
				gethostipbyname(host_2,ip);
				break;
			}

		}
	fclose(fp);
	}
}

void gethostipbyname(char* name,char* ip){
    if(strcmp(name,"csnetlab_1")==0){
        strcpy(ip,"114.212.190.185");
    }
    else if(strcmp(name,"csnetlab_2")==0){
        strcpy(ip,"114.212.190.186");
    }
    else if(strcmp(name,"csnetlab_3")==0){
        strcpy(ip,"114.212.190.187");
    }
    else if(strcmp(name,"csnetlab_4")==0){
        strcpy(ip,"114.212.190.188");
    }    
    else{
        strcpy(ip,"\0");
    }

}

//int main(){
	
//	printf("%d\n",topology_getNbrNum());
//	FILE* fp1=fopen("topology.dat","r");
//	FILE* fp2=fopen("topology.dat","r");
//	if(fp1==NULL)
//		printf("null fp1");
//	if(fp2==NULL)
//		printf("null fp2");
//	char buf[32];
//	gehostname(buf,32);
//	int id=topology_getNodeIDfromname(LOCAL);
//	printf("%d\n",id);
//}
