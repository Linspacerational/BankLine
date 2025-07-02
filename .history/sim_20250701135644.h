#ifndef SIMULATION
#define SIMULATION

#include <stdio.h>
#include <stdlib.h>
#include "event.h"
typedef Event DataType; /* elements are Event objects */

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
{
  int simulationLength;        /* simulation length */
  int numTellers;              /* number of tellers */
  int nextCustomer;            /* next customer ID */
  int arrivalLow, arrivalHigh; /* next arrival range */
  int serviceLow, serviceHigh; /* service range */
  int maxVipWait;              /* maximum wait time for VIP */
  TellerStats tstat[11];       /* max 10 tellers */
  PQueue pq;                   /* priority queue */
};
typedef struct simulation Simulation;

int NextArrivalTime(Simulation *s);
int Get_ServiceTime(Simulation *s);
int NextAvailableTeller(Simulation *s, isVip iv, int currentTime);
void InitSimulation(Simulation *s);
void RunSimulation(Simulation *s);          /* execute study */
void PrintSimulationResults(Simulation *s); /* print stats */
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
  Event *firstevent = (Event *)malloc(sizeof(Event));

  /* Initialize Teller Information Parameters */
  for (i = 1; i <= 10; i++)
  {
    s->tstat[i].finishService = 0;
    s->tstat[i].totalService = 0;
    s->tstat[i].totalCustomerWait = 0;
    s->tstat[i].totalCustomerCount = 0;
  }
  s->nextCustomer = 1;

  /* reads client input for the study */
  printf("请输入模拟时间（分钟）：");
  scanf("%d", &s->simulationLength);
  printf("请输入银行柜员数量：");
  scanf("%d", &s->numTellers);
  printf("请输入用户到达时间范围（分钟）：");
  scanf("%d%d", &s->arrivalLow, &s->arrivalHigh);
  printf("请输入服务时间范围（分钟）：");
  scanf("%d%d", &s->serviceLow, &s->serviceHigh);
  printf("请输入VIP用户最大等待时间（分钟）：");
  scanf("%d", &s->maxVipWait);

  /* generate first arrival event */
  InitEvent(firstevent, 0, arrival, 1, 0, 0, 0, GenerateRandomVipStatus());
  InitPQueue(&(s->pq));
  PQInsert(&(s->pq), *firstevent);
}

/* determine random time of next arrival */
int NextArrivalTime(Simulation *s)
{
  return s->arrivalLow + rand() % (s->arrivalHigh - s->arrivalLow + 1);
}

/* determine random time for customer service */
int Get_ServiceTime(Simulation *s)
{
  return s->serviceLow + rand() % (s->serviceHigh - s->serviceLow + 1);
}

/* return first available teller based on VIP status */
int NextAvailableTeller(Simulation *s, isVip iv, int currentTime)
{
  int i;
  if (iv == Vip)
  {
    /* 优先选择空闲的VIP窗口（1号窗口） */
    if (s->tstat[1].finishService <= currentTime && s->tstat[1].totalCustomerCount < 5)
      return 1;
    /* 其次选择空闲的普通窗口 */
    for (i = 2; i <= s->numTellers; i++)
      if (s->tstat[i].finishService <= currentTime)
        return i;
    /* 再次选择非空的VIP窗口 */
    if (s->tstat[1].totalCustomerCount < 5)
      return 1;
    /* 最后选择非空的普通窗口 */
    return rand() % (s->numTellers - 1) + 2; /* 随机选择2号及以上窗口 */
  }
  else
  {
    /* 普通用户只能选择普通窗口（2号及以上） */
    for (i = 2; i <= s->numTellers; i++)
      if (s->tstat[i].finishService <= currentTime)
        return i;
    return rand() % (s->numTellers - 1) + 2; /* 随机选择2号及以上窗口 */
  }
}

/* implements the simulation */
void RunSimulation(Simulation *s)
{
  Event *e = (Event *)malloc(sizeof(Event));
  Event *newevent = (Event *)malloc(sizeof(Event));
  int nexttime;
  int tellerID;
  int servicetime;
  int waittime;
  isVip iv;

  /* run till priority queue is empty */
  while (!PQEmpty(&(s->pq)))
  {
    /* get next event */
    *e = PQDelete(&(s->pq));

    /* handle an arrival event */
    if (GetEventType(e) == arrival)
    {
      /* compute time for next arrival */
      nexttime = GetTime(e) + NextArrivalTime(s);

      if (nexttime <= s->simulationLength)
      {
        /* generate arrival for next customer */
        s->nextCustomer++;
        InitEvent(newevent, nexttime, arrival, s->nextCustomer, 0, 0, 0, GenerateRandomVipStatus());
        PQInsert(&(s->pq), *newevent);
      }

      printf("时间: %2d\t%s用户 %d 到达\n", GetTime(e), (GetCustomerType(e) == Vip) ? "VIP" : "普通", GetCustomerID(e));

      /* generate departure event for current customer */
      servicetime = Get_ServiceTime(s);
      iv = GetCustomerType(e);
      tellerID = NextAvailableTeller(s, iv, GetTime(e));

      /* 计算等待时间 */
      waittime = (s->tstat[tellerID].finishService > GetTime(e)) ? s->tstat[tellerID].finishService - GetTime(e) : 0;

      /* 检查VIP用户等待时间是否超过最大限制 */
      if (iv == Vip && waittime > s->maxVipWait)
      {
        /* 重新选择窗口，优先选择空闲窗口 */
        tellerID = NextAvailableTeller(s, iv, GetTime(e));
        waittime = (s->tstat[tellerID].finishService > GetTime(e)) ? s->tstat[tellerID].finishService - GetTime(e) : 0;
      }

      /* 如果柜员空闲，更新柜员的finishService为当前时间 */
      if (s->tstat[tellerID].finishService <= GetTime(e))
        s->tstat[tellerID].finishService = GetTime(e);

      /* 更新柜员统计信息 */
      s->tstat[tellerID].totalCustomerWait += waittime;
      s->tstat[tellerID].totalCustomerCount++;
      s->tstat[tellerID].totalService += servicetime;
      s->tstat[tellerID].finishService += servicetime;

      /* 创建出发事件并加入优先级队列 */
      InitEvent(newevent, s->tstat[tellerID].finishService, departure, GetCustomerID(e), tellerID, waittime, servicetime, iv);
      PQInsert(&(s->pq), *newevent);
    }
    /* handle a departure event */
    else
    {
      printf("时间: %2d\t%s用户 %d 离开\n", GetTime(e), (GetCustomerType(e) == Vip) ? "VIP" : "普通", GetCustomerID(e));
      printf("\t柜员 %d\t等待时间 %d\t服务时间 %d\n", GetTellerID(e), GetWaitTime(e), GetServiceTime(e));
      tellerID = GetTellerID(e);
      /* 如果没有用户等待，标记柜员空闲 */
      if (GetTime(e) == s->tstat[tellerID].finishService)
        s->tstat[tellerID].finishService = 0;
    }
  }

  /* 调整模拟时间以包括柜员的加班时间 */
  s->simulationLength = (GetTime(e) <= s->simulationLength) ? s->simulationLength : GetTime(e);
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
  printf("******** 模拟结果总结 ********\n");
  printf("模拟时间: %d 分钟\n", s->simulationLength);
  printf("\t用户总数: %d\n", cumCustomers);
  printf("\t平均用户等待时间: ");

  avgCustWait = (cumCustomers > 0) ? (int)((float)cumWait / cumCustomers + 0.5) : 0;
  printf("%d 分钟\n", avgCustWait);
  for (i = 1; i <= s->numTellers; i++)
  {
    printf("\t柜员 #%d\t工作时间占比 ", i);
    tellerWork = (float)(s->tstat[i].totalService) / s->simulationLength;
    tellerWorkPercent = (int)(tellerWork * 100.0 + 0.5);
    printf("%d%%\n", tellerWorkPercent);
  }
}

#endif /* SIMULATION */