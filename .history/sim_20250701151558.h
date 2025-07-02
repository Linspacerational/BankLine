#ifndef SIMULATION
#define SIMULATION

#include <stdio.h>
#include <stdlib.h>
#include "event.h"
typedef Event DataType;       /* elements are Event objects */

#include "apqueue.h"

/* Structure for Teller Info */
struct tellerStats
{
	int finishService;      /* when teller available */
	int totalCustomerCount; /* total of customers serviced */
	int totalCustomerWait;  /* total customer waiting time */
	int totalService;       /* total time servicing customers */
};
typedef struct tellerStats TellerStats;

struct simulation
{/* data used to run the simulation */
	int simulationLength;        /* simulation length */
	int numTellers;              /* number of tellers */
	int nextCustomer;            /* next customer ID */
	int arrivalLow, arrivalHigh; /* next arrival range */
	int serviceLow, serviceHigh; /* service range */
	TellerStats tstat[11];       /* max 10 tellers */
	PQueue pq;                   /* priority queue */
};
typedef struct simulation Simulation;

int NextArrivalTime(Simulation *s);
int Get_ServiceTime(Simulation *);
int NextAvailableTeller(Simulation *s, isVip iv);
void InitSimulation(Simulation *s);
void RunSimulation(Simulation *s);/* execute study */
void PrintSimulationResults(Simulation *s);/* print stats */
isVip GenerateRandomVipStatus(void);

isVip GenerateRandomVipStatus(void)
{
  /* 20% chance of being a VIP */
  return (rand() % 5 == 0) ? Vip : notVip;
}

/* initializes simulation data and prompts client for simulation parameters */
void InitSimulation(Simulation *s)
{
	int i;
	Event *firstevent=(Event *)malloc(sizeof(Event));
	
	/* Initialize Teller Information Parameters */
	for(i = 1; i <= 10; i++)
	{
		s->tstat[i].finishService = 0; 
		s->tstat[i].totalService = 0;
		s->tstat[i].totalCustomerWait = 0;
		s->tstat[i].totalCustomerCount = 0;
	}
	s->nextCustomer = 1;

	/* reads client input for the study */
	printf("Enter the simulation time in minutes: ");
	scanf("%d", &s->simulationLength);
	printf("Enter the number of bank tellers: ");
	scanf("%d", &s->numTellers);
	printf("Enter the range of arrival times in minutes: ");
	scanf("%d%d", &s->arrivalLow, &s->arrivalHigh);
	printf("Enter the range of service times in minutes: ");
	scanf("%d%d", &s->serviceLow, &s->serviceHigh);


	/* generate first arrival event
	   teller#/waittime/servicetime not used for arrival */
	InitEvent(firstevent,0,arrival,1,0,0,0,GenerateRandomVipStatus());
	InitPQueue(&(s->pq));
	PQInsert(&(s->pq), *firstevent);
}

/* determine random time of next arrival */
int NextArrivalTime(Simulation *s)
{
	return s->arrivalLow+rand()%(s->arrivalHigh-s->arrivalLow+1);
}

/* determine random time for customer service */
int Get_ServiceTime(Simulation *s)
{
	return s->serviceLow+rand()%(s->serviceHigh-s->serviceLow+1);
}

/* 根据VIP状态返回第一个可用的出纳员 */
int NextAvailableTeller(Simulation *s, isVip iv)
{
  int i;

  if (iv == Vip)
  {
    // 优先级1：空VIP窗口（1号出纳员）
    if (s->tstat[1].finishService == 0 && s->tstat[1].totalCustomerCount < 5)
    {
      return 1;
    }
    // 优先级2：空普通窗口（2至numTellers号出纳员）
    for (i = 2; i <= s->numTellers; i++)
    {
      if (s->tstat[i].finishService == 0)
      {
        return i;
      }
    }
    // 优先级3：在普通窗口插队（随机选择普通窗口）
    return (rand() % (s->numTellers - 1)) + 2; // 随机普通出纳员（2至numTellers）
  }
  else
  {
    // 普通用户：仅使用普通窗口（2至numTellers号出纳员）
    for (i = 2; i <= s->numTellers; i++)
    {
      if (s->tstat[i].finishService == 0)
      {
        return i; // 返回第一个空闲普通出纳员
      }
    }
    // 如果所有普通出纳员忙碌，随机选择一个普通出纳员
    return (rand() % (s->numTellers - 1)) + 2; // 随机普通出纳员（2至numTellers）
  }
}

/* 实现模拟 */
void RunSimulation(Simulation *s)
{
  Event *e = (Event *)malloc(sizeof(Event));
  Event *newevent = (Event *)malloc(sizeof(Event));
  int nexttime;
  int tellerID;
  int servicetime;
  int waittime;
  isVip iv;

  /* 直到优先队列为空 */
  while (!PQEmpty(&(s->pq)))
  {
    /* 获取下一个事件（时间决定优先级） */
    *e = PQDelete(&(s->pq));

    /* 处理到达事件 */
    if (GetEventType(e) == arrival)
    {
      /* 计算下一次到达时间 */
      nexttime = GetTime(e) + NextArrivalTime(s);

      if (nexttime <= s->simulationLength)
      {
        /* 生成下一个客户的到达事件 */
        s->nextCustomer++;
        InitEvent(newevent, nexttime, arrival, s->nextCustomer, 0, 0, 0, GenerateRandomVipStatus());
        PQInsert(&(s->pq), *newevent);
      }

      printf("时间: %2d\t%s客户 %d 到达\n", GetTime(e), (GetCustomerType(e) == Vip) ? "VIP" : "普通", GetCustomerID(e));

      /* 为当前客户生成离开事件 */
      servicetime = Get_ServiceTime(s);
      iv = GetCustomerType(e);
      tellerID = NextAvailableTeller(s, iv); // 根据VIP状态分配出纳员

      /* 计算等待时间 */
      if (s->tstat[tellerID].finishService < GetTime(e))
      {
        s->tstat[tellerID].finishService = GetTime(e); // 出纳员空闲，立即开始服务
      }
      waittime = s->tstat[tellerID].finishService - GetTime(e);

      /* 更新出纳员统计 */
      s->tstat[tellerID].totalCustomerWait += waittime;
      s->tstat[tellerID].totalCustomerCount++;
      s->tstat[tellerID].totalService += servicetime;                         // 累加服务时间
      s->tstat[tellerID].finishService = GetTime(e) + waittime + servicetime; // 更新完成时间
      
      /* 创建离开事件 */
      InitEvent(newevent, s->tstat[tellerID].finishService,
                departure, GetCustomerID(e), tellerID,
                waittime, servicetime, iv);
      PQInsert(&(s->pq), *newevent);

      /* 记录VIP插队行为（如果适用） */
      if (iv == Vip && tellerID != 1 && waittime > 0)
      {
        printf("\tVIP客户 %d 在普通出纳员 %d 插队\n", GetCustomerID(e), tellerID);
      }
    }
    /* 处理离开事件 */
    else
    {
      printf("时间: %2d\t%s客户 %d 离开\n", GetTime(e), (GetCustomerType(e) == Vip) ? "VIP" : "普通", GetCustomerID(e));
      printf("\t出纳员 %d\t等待时间 %d\t服务时间 %d\n", GetTellerID(e), GetWaitTime(e), GetServiceTime(e));
      tellerID = GetTellerID(e);
      /* 如果没有客户等待出纳员，标记出纳员空闲 */
      if (GetTime(e) == s->tstat[tellerID].finishService)
      {
        s->tstat[tellerID].finishService = 0;
      }
    }
  }

  /* 调整模拟时间以考虑加班 */
  s->simulationLength = (GetTime(e) <= s->simulationLength) ? s->simulationLength : GetTime(e);

  /* 修正 totalService，确保不超过模拟时间 */
  for (int i = 1; i <= s->numTellers; i++)
  {
    if (s->tstat[i].totalService > s->simulationLength)
    {
      s->tstat[i].totalService = s->simulationLength;
    }
  }

  free(e);
  free(newevent);
}

/* summarize the simulation results */
void PrintSimulationResults(Simulation *s)
{
	int cumCustomers = 0, cumWait = 0, i;
	int avgCustWait, tellerWorkPercent;
	float tellerWork;

	for (i = 1; i <= s->numTellers; i++)
	{
		cumCustomers += s->tstat[i].totalCustomerCount;
		cumWait += s->tstat[i].totalCustomerWait;
	}

	printf("\n");
	printf("******** Simulation Summary ********\n");
	printf("Simulation of %d minutes\n", s->simulationLength);
	printf("\tNo. of Customers: %d\n", cumCustomers);
	printf("\tAverage Customer Wait: ");
	
	avgCustWait = (int)((float)cumWait/cumCustomers + 0.5);
	printf("%d minutes\n", avgCustWait);
	for(i=1;i <= s->numTellers;i++)
	{
		printf("\tTeller #%d\tWorking ", i);
		/* display percent rounded to nearest integer value */
		tellerWork = (float)(s->tstat[i].totalService)/s->simulationLength;
		tellerWorkPercent = (int) (tellerWork * 100.0 + 0.5);
		printf("%d\n", tellerWorkPercent);
	}
}

#endif  /* SIMULATION */