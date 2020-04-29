#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/wait.h>
#include <sys/syscall.h>

#define FIFO 0
#define SJF 1
#define PSJF 2
#define RR 3
#define Idle -1
#define DONE -2
#define CLOSE -1
#define Get_Time 334
#define Printk 335
#define Queue_Size 100
#define Unit_Time() { volatile unsigned long i; for(i=0;i<1000000UL;i++); }

int Num_prc;
int head = 0;
int tail = 0;
int Queue[Queue_Size];
int scheduler(char Prc_name[Num_prc][64],int Prc_ReadyTime[Num_prc],int Prc_ExecTime[Num_prc],int policy);
int InitPrc(int Process_ExecTime,char Prc_name[64]);
int Find_Next_prc(int Prc_PID[Num_prc],char Prc_name[Num_prc][64],int Prc_ReadyTime[Num_prc],int Prc_ExecTime[Num_prc],int policy);
int Context_Switch(int Prc_PID[Num_prc],char Prc_name[Num_prc][64],int Prc_ReadyTime[Num_prc],int Prc_ExecTime[Num_prc],int policy,int Current_time,int Current_prc,int RR_StartT);

int main(int argc, char const *argv[])
{
	char policy[64];
	scanf("%s",policy);
	scanf("%d",&Num_prc);
	char Prc_Name[Num_prc][64];
	int Prc_ReadyTime[Num_prc];
	int Prc_ExecTime[Num_prc];
	for(int i = 0; i < Num_prc; i++){
		scanf("%s",Prc_Name[i]);
		scanf("%d",&Prc_ReadyTime[i]);
		scanf("%d",&Prc_ExecTime[i]);
	}
#ifdef DEBUG
	for(int i = 0; i< Num_prc; i++){
		printf("%s ",Prc_Name[i]);
		printf("%d ",Prc_ReadyTime[i]);
		printf("%d\n",Prc_ExecTime[i]);
	}
#endif
	if(strcmp(policy, "FIFO") == 0){
		scheduler(Prc_Name,Prc_ReadyTime,Prc_ExecTime,FIFO);
	}
	else if(strcmp(policy, "SJF") == 0){
		scheduler(Prc_Name,Prc_ReadyTime,Prc_ExecTime,SJF);
	}
	else if(strcmp(policy, "PSJF") == 0){
		scheduler(Prc_Name,Prc_ReadyTime,Prc_ExecTime,PSJF);
	}
	else if(strcmp(policy, "RR") == 0){
		scheduler(Prc_Name,Prc_ReadyTime,Prc_ExecTime,RR);
	}
	else{
		printf("policy_Fail\n");
	}
	return 0;
}
int scheduler(char Prc_name[Num_prc][64],int Prc_ReadyTime[Num_prc],int Prc_ExecTime[Num_prc],int policy){
		
	int status = Idle;// -1 when scheduler idle
	int Finish = 0;
	int Prc_PID[Num_prc];
	int Current_time = 0;
	int RR_StartT;
	struct sched_param param;
	param.sched_priority = 0;

	for(int i = 0; i < Num_prc; i++){
		Prc_PID[i] = CLOSE;
	}
	for(int i = 0; i < 100; i++){
		Queue[i] = -1;
	}
	sched_setscheduler(getpid(), SCHED_OTHER, &param);//指定高priority給scheduler

	while(1){

		if(status != Idle){//檢查正在run的process完成沒
			
			if(Prc_ExecTime[status] == 0 && Prc_PID[status] != DONE){
				//printf("status = %d\n",status);
				waitpid(Prc_PID[status], NULL, 0);//收屍
				if(policy == RR){
					Queue[head] = -1;				
					if(head == (Queue_Size - 1)){
						head = 0;						
					}
					else{
						head++;
					}
				}
				Prc_PID[status] = DONE;//-2代表Process執行完了
				status = Idle;
				Finish++;
				if(Finish == Num_prc){
					break;
				}
			}
			else if(Prc_ExecTime[status] > 0){
				status = status;
			}
		}

		for(int i = 0; i < Num_prc; i++){//檢查有沒有要被Init的process
			if(Prc_ReadyTime[i] == Current_time){
				//printf("init prc %d\n",i);
				Prc_PID[i] = InitPrc(Prc_ExecTime[i],Prc_name[i]);
				//printf("Prc_PID[%d] = %d\n",i,Prc_PID[i]);
				param.sched_priority = 0;
				sched_setscheduler(Prc_PID[i], SCHED_IDLE, &param);//設定低priority給剛啟動的process來block住
				if(policy == RR){
					Queue[tail] = i;
					if(tail == (Queue_Size - 1)){
						tail = 0;						
					}
					else{
						tail++;
					}
				}
			}
		}
		if(status == Idle){//如果scheduler是空閒
			status = Find_Next_prc(Prc_PID,Prc_name,Prc_ReadyTime,Prc_ExecTime,policy);
			if(status != Idle){			
				RR_StartT = Current_time;
				param.sched_priority = 0;
				sched_setscheduler(Prc_PID[status], SCHED_OTHER, &param);//設定高priority給next Process
			}
		}
		else{
			int before = status;
			status = Context_Switch(Prc_PID,Prc_name,Prc_ReadyTime,Prc_ExecTime,policy,Current_time,status,RR_StartT);
			if(before != status){
				//printf("before = %d now = %d\n",before,status);
				param.sched_priority = 0;
				sched_setscheduler(Prc_PID[status], SCHED_OTHER, &param);
				param.sched_priority = 0;
				sched_setscheduler(Prc_PID[before], SCHED_IDLE, &param);
			}		
		}
		Unit_Time();
		if(status != Idle){
			Prc_ExecTime[status]--;
		}
		//printf("Current_time = %d\n",Current_time);
		Current_time++;
	}
	exit(0);
}

int InitPrc(int Process_ExecTime,char Prc_name[64]){
	//printf("init\n");
	int pid = fork();
	if(pid == 0){
		char OUT[512];
		sprintf(OUT,"%s %d\n",Prc_name,getpid());
		printf("%s",OUT);
		fflush(stdout);
		char msg[256];
		long long int start_time_s,start_time_n;
		syscall(Get_Time,&start_time_s,&start_time_n); 
		for(int i = 0; i < Process_ExecTime;i++){
			Unit_Time();
		}
		long long end_time_s,end_time_n;
		syscall(Get_Time,&end_time_s,&end_time_n);
		sprintf(msg, "[Project1] %d %lld.%09lld %lld.%09lld\n", getpid(), start_time_s, start_time_n, end_time_s, end_time_n);
		syscall(Printk,msg);
		//printf("%d finish!\n",getpid());
		exit(0);
	}
	return pid;
}

int Find_Next_prc(int Prc_PID[Num_prc],char Prc_name[Num_prc][64],int Prc_ReadyTime[Num_prc],int Prc_ExecTime[Num_prc],int policy){
	int next = -1;
	//printf("Find_Next\n");
	int Compare = -1;
	if(policy == FIFO){
		for(int i = 0; i < Num_prc; i++){
			if(Prc_PID[i] != DONE && Prc_PID[i] != CLOSE){
				if(Compare == -1){
					next = i;
					Compare = Prc_ReadyTime[i];
				}
				else{
					if(Prc_ReadyTime[i] < Compare){
						next = i;
						Compare = Prc_ReadyTime[i];
					}
				}
			}
		}
	}
	else if(policy == SJF || policy == PSJF){
		for(int i = 0; i < Num_prc; i++){
			if(Prc_PID[i] != DONE && Prc_PID[i] != CLOSE){
				if(Compare == -1){
					next = i;
					Compare = Prc_ExecTime[i];
				}
				else{
					if(Prc_ExecTime[i] < Compare){
						next = i;
						Compare = Prc_ExecTime[i];
					}
				}
			}
		}
	}
	else if(policy == RR){
		next = Queue[head];
	}
	else{
		fprintf(stderr, "Find_Next_prc_ERROR\n");
	}
	//printf("%d Is Next(F)\n",next);
	return next;
}
int Context_Switch(int Prc_PID[Num_prc],char Prc_name[Num_prc][64],int Prc_ReadyTime[Num_prc],int Prc_ExecTime[Num_prc],int policy,int Current_time,int Current_prc,int RR_StartT){
	int next = Current_prc;
	if(policy == FIFO || policy == SJF){
		//printf("%d Is Next(C)\n",next);
		return next;
	}
	else if(policy == PSJF){
		//printf("PSJF!\n");
		int Compare = Current_prc;
		for(int i = 0; i < Num_prc; i++){
			if(Prc_PID[i] != DONE && Prc_PID[i] != CLOSE && Prc_ExecTime[i] < Prc_ExecTime[Compare]){
				next = i;
				Compare = Prc_ExecTime[i];
			}
		}
	}
	else if(policy == RR){
		if((Current_time - RR_StartT) % 500 == 0){
			Queue[tail] = Queue[head];
			if(tail == (Queue_Size - 1)){
				tail = 0;						
			}
			else{
				tail++;
			}
			Queue[head] = -1;			
			if(head == (Queue_Size - 1)){
				head = 0;						
			}
			else{
				head++;
			}		
			next = Queue[head];
		}
	}
	//printf("%d Is Next(C)\n",next);
	return next;
}
