#ifndef SIMULATION
#define SIMULATION

#include <stdio.h>
#include <stdlib.h>
#include "event.h"
typedef Event DataType;

#include "apqueue.h"

struct tellerStats
{
  int finishService;
  int totalCustomerCount;
  int totalCustomerWait;
  int totalService;
  struct event timeline[MaxPQSize];
  int timelineCount;
};
typedef struct tellerStats TellerStats;

struct simulation
{
  int simulationLength;
  int numTellers;
  int nextCustomer;
  int arrivalLow, arrivalHigh;
  int serviceLow, serviceHigh;
  TellerStats tstat[11];
  PQueue pq;
  isVip ivs[10];
  int ivsIndex;
};
typedef struct simulation Simulation;

int NextArrivalTime(Simulation *s);
int Get_ServiceTime(Simulation *);
int NextAvailableTeller(Simulation *s, isVip iv, int currentTime);
void InitSimulation(Simulation *s);
void RunSimulation(Simulation *s);
void PrintSimulationResults(Simulation *s);
isVip GenerateRandomVipStatus(void);

isVip GenerateRandomVipStatus(void)
{
  return (rand() % 5 == 0) ? Vip : notVip;
}

void InitSimulation(Simulation *s)
{
  int i;
  Event *firstevent = (Event *)malloc(sizeof(Event));

  for (i = 1; i <= 10; i++)
  {
    s->tstat[i].finishService = 0;
    s->tstat[i].totalService = 0;
    s->tstat[i].totalCustomerWait = 0;
    s->tstat[i].totalCustomerCount = 0;
    s->tstat[i].timelineCount = 0;
  }
  s->nextCustomer = 1;
  s->ivsIndex = 0;

  isVip temp[10] = {notVip, notVip, Vip, notVip, Vip, notVip, Vip, notVip, notVip, Vip};
  for (i = 0; i < 10; i++)
  {
    s->ivs[i] = temp[i];
  }

  printf("输入模拟时间（分钟）：");
  scanf("%d", &s->simulationLength);
  printf("输入出纳员数量：");
  scanf("%d", &s->numTellers);
  printf("输入到达时间范围（分钟）：");
  scanf("%d%d", &s->arrivalLow, &s->arrivalHigh);
  printf("输入服务时间范围（分钟）：");
  scanf("%d%d", &s->serviceLow, &s->serviceHigh);

  InitEvent(firstevent, 0, arrival, 1, 0, 0, 0, s->ivs[s->ivsIndex++]);
  InitPQueue(&(s->pq));
  PQInsert(&(s->pq), *firstevent);
  free(firstevent);
}

int NextArrivalTime(Simulation *s)
{
  return s->arrivalLow + rand() % (s->arrivalHigh - s->arrivalLow + 1);
}

int Get_ServiceTime(Simulation *s)
{
  return s->serviceLow + rand() % (s->serviceHigh - s->serviceLow + 1);
}

int NextAvailableTeller(Simulation *s, isVip iv, int currentTime)
{
  int i;
  if (iv == Vip)
  {
    if (s->tstat[1].finishService < currentTime && s->tstat[1].totalCustomerCount < 5)
    {
      return 1;
    }
    for (i = 2; i <= s->numTellers; i++)
    {
      if (s->tstat[i].finishService < currentTime)
      {
        return i;
      }
    }
    return (rand() % (s->numTellers - 1)) + 2;
  }
  else
  {
    for (i = 2; i <= s->numTellers; i++)
    {
      if (s->tstat[i].finishService == 0)
      {
        return i;
      }
    }
    return (rand() % (s->numTellers - 1)) + 2;
  }
}

void RunSimulation(Simulation *s)
{
  Event *e = (Event *)malloc(sizeof(Event));
  Event *newevent = (Event *)malloc(sizeof(Event));
  int nexttime;
  int tellerID;
  int servicetime;
  int waittime;
  isVip iv;

  while (!PQEmpty(&(s->pq)))
  {
    *e = PQDelete(&(s->pq));

    if (GetEventType(e) == arrival)
    {
      if (GetTime(e) > s->simulationLength)
      {
        continue;
      }

      nexttime = GetTime(e) + NextArrivalTime(s);
      if (nexttime <= s->simulationLength && s->ivsIndex < 10)
      {
        s->nextCustomer++;
        InitEvent(newevent, nexttime, arrival, s->nextCustomer, 0, 0, 0, s->ivs[s->ivsIndex++]);
        PQInsert(&(s->pq), *newevent);
      }

      printf("时间: %2d\t%s客户 %d 到达\n", GetTime(e), (GetCustomerType(e) == Vip) ? "VIP" : "普通", GetCustomerID(e));
      servicetime = Get_ServiceTime(s);
      iv = GetCustomerType(e);
      tellerID = NextAvailableTeller(s, iv);
      printf("\t出纳员 %d\t等待时间 %d\t服务时间 %d\n", GetTellerID(e), GetWaitTime(e), GetServiceTime(e));
      
      if (s->tstat[tellerID].finishService < GetTime(e))
      {
        s->tstat[tellerID].finishService = GetTime(e);
      }
      waittime = s->tstat[tellerID].finishService - GetTime(e);

      s->tstat[tellerID].totalCustomerWait += waittime;
      s->tstat[tellerID].totalCustomerCount++;
      s->tstat[tellerID].totalService += servicetime;
      s->tstat[tellerID].finishService = GetTime(e) + waittime + servicetime;

      InitEvent(newevent, s->tstat[tellerID].finishService,
                departure, GetCustomerID(e), tellerID,
                waittime, servicetime, iv);
      PQInsert(&(s->pq), *newevent);

      if (s->tstat[tellerID].timelineCount < MaxPQSize)
      {
        s->tstat[tellerID].timeline[s->tstat[tellerID].timelineCount] = *newevent;
        s->tstat[tellerID].timelineCount++;
      }

      if (iv == Vip && tellerID != 1 && waittime > 0)
      {
        printf("\tVIP客户 %d 在普通出纳员 %d 插队\n", GetCustomerID(e), tellerID);
      }
    }
    else
    {
      printf("时间: %2d\t%s客户 %d 离开\n", GetTime(e), (GetCustomerType(e) == Vip) ? "VIP" : "普通", GetCustomerID(e));
      printf("\t出纳员 %d\t等待时间 %d\t服务时间 %d\n", GetTellerID(e), GetWaitTime(e), GetServiceTime(e));
      tellerID = GetTellerID(e);
      /* 移除 finishService 重置，依赖到达事件的更新 */
    }
  }

  s->simulationLength = (GetTime(e) <= s->simulationLength) ? s->simulationLength : GetTime(e);

  free(e);
  free(newevent);
}

void PrintSimulationResults(Simulation *s)
{
  int cumCustomers = 0, cumWait = 0, i, j;
  int avgCustWait, tellerWorkPercent;
  float tellerWork;

  for (i = 1; i <= s->numTellers; i++)
  {
    cumCustomers += s->tstat[i].totalCustomerCount;
    cumWait += s->tstat[i].totalCustomerWait;
  }

  printf("\n");
  printf("******** 模拟结果总结 ********\n");
  printf("模拟时间：%d 分钟\n", s->simulationLength);
  printf("\t客户总数：%d\n", cumCustomers);
  printf("\t平均客户等待时间：");

  avgCustWait = (int)((float)cumWait / cumCustomers + 0.5);
  printf("%d 分钟\n", avgCustWait);

  for (i = 1; i <= s->numTellers; i++)
  {
    printf("\t出纳员 #%d\t工作百分比 ", i);
    tellerWork = (float)(s->tstat[i].totalService) / s->simulationLength;
    tellerWorkPercent = (int)(tellerWork * 100.0 + 0.5);
    printf("%d%%\n", tellerWorkPercent);

    printf("\t出纳员 #%d 时间线：\n", i);
    for (j = 0; j < s->tstat[i].timelineCount; j++)
    {
      Event *te = &s->tstat[i].timeline[j];
      printf("\t\t时间 %2d：%s客户 %d，服务时间 %d，等待时间 %d\n",
             GetTime(te), (GetCustomerType(te) == Vip) ? "VIP" : "普通",
             GetCustomerID(te), GetServiceTime(te), GetWaitTime(te));
    }
  }
}

#endif /* SIMULATION */