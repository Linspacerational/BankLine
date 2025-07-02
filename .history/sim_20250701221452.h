#ifndef SIMULATION
#define SIMULATION

#include <stdio.h>
#include <stdlib.h>
#include "event.h"
typedef Event DataType;

#include "apqueue.h"

// 定义 MaxTellerQueueSize 以避免使用魔术数字，并使其可配置
#define MaxTellerQueueSize 50 // 假设每个出纳员队列的合理最大尺寸

struct tellerStats
{
  int finishService;
  int totalCustomerCount;
  int totalCustomerWait;
  int totalService;
  struct event timeline[MaxPQSize]; // 这存储了已完成的离开事件用于结果统计
  int timelineCount;
  // 新增：一个简单的客户队列，表示在该出纳员处等待的客户
  Event customerQueue[MaxTellerQueueSize];
  int queueHead;
  int queueTail;
  int queueCount;
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
  isVip ivs[10]; // 预定义的 VIP 状态
  int ivsIndex;
};
typedef struct simulation Simulation;

// 函数原型
int NextArrivalTime(Simulation *s);
int Get_ServiceTime(Simulation *);
int NextAvailableTeller(Simulation *s, isVip iv, int currentTime);
void InitSimulation(Simulation *s);
void RunSimulation(Simulation *s);
void PrintSimulationResults(Simulation *s);
isVip GenerateRandomVipStatus(void);

// 新增的出纳员队列辅助函数
void EnqueueCustomer(TellerStats *ts, Event customerEvent);
Event DequeueCustomer(TellerStats *ts);
Event PeekCustomer(TellerStats *ts);
int IsTellerQueueEmpty(TellerStats *ts);

// 生成随机的 VIP 状态（五分之一的概率是 VIP）
isVip GenerateRandomVipStatus(void)
{
  return (rand() % 5 == 0) ? Vip : notVip;
}

// 初始化仿真参数和数据结构
void InitSimulation(Simulation *s)
{
  int i;
  Event *firstevent = (Event *)malloc(sizeof(Event)); // 为第一个事件分配内存

  // 初始化出纳员统计数据及其队列
  for (i = 1; i <= 10; i++)
  {
    s->tstat[i].finishService = 0;
    s->tstat[i].totalService = 0;
    s->tstat[i].totalCustomerWait = 0;
    s->tstat[i].totalCustomerCount = 0;
    s->tstat[i].timelineCount = 0;
    s->tstat[i].queueHead = 0;
    s->tstat[i].queueTail = 0;
    s->tstat[i].queueCount = 0;
  }
  s->nextCustomer = 1; // 第一个客户 ID
  s->ivsIndex = 0;     // 预定义 VIP 状态的索引

  // 预定义的初始客户 VIP 状态
  isVip temp[10] = {notVip, notVip, Vip, notVip, Vip, notVip, Vip, notVip, notVip, Vip};
  for (i = 0; i < 10; i++)
  {
    s->ivs[i] = temp[i];
  }

  // 提示用户输入仿真参数
  printf("输入模拟时间（分钟）：");
  scanf("%d", &s->simulationLength);
  printf("输入出纳员数量：");
  scanf("%d", &s->numTellers);
  printf("输入到达时间范围（分钟）：");
  scanf("%d%d", &s->arrivalLow, &s->arrivalHigh);
  printf("输入服务时间范围（分钟）：");
  scanf("%d%d", &s->serviceLow, &s->serviceHigh);

  // 初始化并插入第一个到达事件到优先队列
  InitEvent(firstevent, 0, arrival, 1, 0, 0, 0, s->ivs[s->ivsIndex++]);
  InitPQueue(&(s->pq));
  PQInsert(&(s->pq), *firstevent);
  free(firstevent); // 释放临时事件内存
}

// 计算下一个客户的到达时间，在指定范围内
int NextArrivalTime(Simulation *s)
{
  return s->arrivalLow + rand() % (s->arrivalHigh - s->arrivalLow + 1);
}

// 计算随机服务时间，在指定范围内
int Get_ServiceTime(Simulation *s)
{
  return s->serviceLow + rand() % (s->serviceHigh - s->serviceLow + 1);
}

// 根据 VIP 状态和出纳员可用性确定下一个可用出纳员
int NextAvailableTeller(Simulation *s, isVip iv, int currentTime)
{
  int i;
  int bestTellerID = -1;
  int minFinishTime = 999999; // 一个非常大的数字，表示无穷大

  // --- VIP 客户逻辑 ---
  if (iv == Vip)
  {
    // 优先级 1：空闲的 VIP 出纳员（出纳员 #1）
    if (s->tstat[1].finishService <= currentTime && IsTellerQueueEmpty(&s->tstat[1]))
    {
      return 1;
    }

    // 优先级 2：空闲的普通出纳员
    for (i = 2; i <= s->numTellers; i++)
    {
      if (s->tstat[i].finishService <= currentTime && IsTellerQueueEmpty(&s->tstat[i]))
      {
        return i; // 找到一个空闲的普通出纳员
      }
    }

    // 优先级 3：抢占普通出纳员队列中的非 VIP 客户
    // 寻找一个普通出纳员（非出纳员 1），其队列中的下一个客户不是 VIP
    for (i = 2; i <= s->numTellers; i++)
    {
      if (!IsTellerQueueEmpty(&s->tstat[i]))
      {
        Event nextInQueue = PeekCustomer(&s->tstat[i]);
        if (GetCustomerType(&nextInQueue) != Vip)
        {
          return i; // 这个出纳员是抢占的候选人
        }
      }
    }

    // 优先级 4：非空闲的 VIP 出纳员（出纳员 #1）
    // 如果 VIP 出纳员忙碌，但它是 VIP 的唯一选择或最佳选择
    if (s->tstat[1].finishService > currentTime || !IsTellerQueueEmpty(&s->tstat[1]))
    {
      return 1; // 在 VIP 出纳员处排队
    }

    // 优先级 5：非空闲的普通出纳员（最后选择，选择等待时间最短的）
    // 在所有忙碌的出纳员中（不包括 VIP 出纳员，如果它严格只服务 VIP 且忙碌），找到结束时间最早的出纳员
    for (i = 2; i <= s->numTellers; i++)
    {
      if (s->tstat[i].finishService < minFinishTime)
      {
        minFinishTime = s->tstat[i].finishService;
        bestTellerID = i;
      }
    }
    // 如果所有出纳员都忙碌，返回普通出纳员中最早结束的那个
    if (bestTellerID != -1)
    {
      return bestTellerID;
    }
  }
  // --- 非 VIP 客户逻辑 ---
  else // if (iv == notVip)
  {
    // 首先寻找一个空闲的普通出纳员
    for (i = 2; i <= s->numTellers; i++)
    {
      if (s->tstat[i].finishService <= currentTime && IsTellerQueueEmpty(&s->tstat[i]))
      {
        return i; // 找到一个空闲的普通出纳员
      }
    }

    // 如果没有空闲的普通出纳员，寻找结束时间最早的普通出纳员
    for (i = 2; i <= s->numTellers; i++)
    {
      if (s->tstat[i].finishService < minFinishTime)
      {
        minFinishTime = s->tstat[i].finishService;
        bestTellerID = i;
      }
    }
    // 如果所有普通出纳员都忙碌，返回最早结束的那个
    if (bestTellerID != -1)
    {
      return bestTellerID;
    }
  }

  // 备用：如果根据优先级没有找到特定出纳员，
  // 返回一个随机普通出纳员（如果逻辑完整，理论上不应到达此处）
  // 或者，更健壮的备用方案：寻找队列最短或结束时间最早的出纳员，无论类型。
  // 为简化起见，如果所有其他方法都失败，或者只剩出纳员 1 且其忙碌，则选择一个随机普通出纳员。
  if (s->numTellers > 1)
  {
    return (rand() % (s->numTellers - 1)) + 2; // 随机普通出纳员
  }
  else
  {
    return 1; // 只有一个出纳员（VIP 出纳员），返回它
  }
}

// 运行仿真
void RunSimulation(Simulation *s)
{
  Event *e = (Event *)malloc(sizeof(Event));        // 当前事件
  Event *newevent = (Event *)malloc(sizeof(Event)); // 将要安排的新事件
  int nexttime;
  int tellerID;
  int servicetime;
  int waittime;
  isVip iv;

  while (!PQEmpty(&(s->pq)))
  {
    *e = PQDelete(&(s->pq)); // 从优先队列中获取下一个事件

    // 如果事件时间超过仿真时长，则跳过
    if (GetTime(e) > s->simulationLength)
    {
      // 然而，我们需要处理所有在仿真时长内安排但在此之后完成的离开事件。
      // 为简化起见，对于超过仿真时长的到达事件，我们将继续。
      // 一个更健壮的仿真会处理所有已安排的事件直到它们离开。
      if (GetEventType(e) == arrival)
      {
        continue;
      }
    }

    if (GetEventType(e) == arrival)
    {
      printf("时间: %2d\t%s客户 %d 到达\n", GetTime(e), (GetCustomerType(e) == Vip) ? "VIP" : "普通", GetCustomerID(e));

      // 如果在仿真限制和预定义 VIP 列表限制内，安排下一个到达事件
      nexttime = GetTime(e) + NextArrivalTime(s);
      if (nexttime <= s->simulationLength && s->ivsIndex < 10)
      {
        s->nextCustomer++;
        InitEvent(newevent, nexttime, arrival, s->nextCustomer, 0, 0, 0, s->ivs[s->ivsIndex++]);
        PQInsert(&(s->pq), *newevent);
      }

      servicetime = Get_ServiceTime(s);
      iv = GetCustomerType(e);
      tellerID = NextAvailableTeller(s, iv, GetTime(e));

      // 处理客户排队和服务
      // 如果出纳员当前空闲（finishService 在过去）且其队列为空
      if (s->tstat[tellerID].finishService <= GetTime(e) && IsTellerQueueEmpty(&s->tstat[tellerID]))
      {
        s->tstat[tellerID].finishService = GetTime(e); // 出纳员立即开始服务
        waittime = 0;
        // 直接为这个客户创建并插入离开事件
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
        printf("\t出纳员 %d\t等待时间 %d\t服务时间 %d\n", tellerID, waittime, servicetime);
      }
      else // 出纳员忙碌或有队列
      {
        // 如果是 VIP 客户且尝试在普通出纳员处插队
        if (iv == Vip && tellerID != 1 && !IsTellerQueueEmpty(&s->tstat[tellerID]))
        {
          Event nextInQueue = PeekCustomer(&s->tstat[tellerID]);
          if (GetCustomerType(&nextInQueue) != Vip)
          {
            // 这是抢占场景。VIP 客户进入队列前端。
            // 出纳员当前的 'finishService' 是为被抢占的客户设置的。
            // VIP 将在出纳员本应完成当前客户时开始，或者如果出纳员技术上空闲，则立即开始。
            waittime = s->tstat[tellerID].finishService - GetTime(e);
            if (waittime < 0)
              waittime = 0; // 如果出纳员已经空闲

            // 我们需要将当前的 VIP 事件插入到该出纳员队列的前端，
            // 并将被抢占的非 VIP 客户推到后面。
            // 对于基于数组的队列，这是一种简化。
            // 真正的链表或每个出纳员的优先级队列会更好。
            // 对于这个基于数组的队列，我们将“概念上”插入：
            // 1. 出队被抢占的客户。
            // 2. 入队 VIP 客户。
            // 3. 入队被抢占的客户（现在在末尾）。

            Event preemptedCustomer = DequeueCustomer(&s->tstat[tellerID]);
            // 将被抢占的客户的等待时间调整到当前VIP到来时间，因为VIP插队导致其等待时间延长
            // 这是一个简化，更准确的做法是重新计算所有后续客户的等待时间
            preemptedCustomer.ime = GetTime(e); // 假设被“推后”到当前VIP到达时间

            EnqueueCustomer(&s->tstat[tellerID], *e);                // 入队 VIP
            EnqueueCustomer(&s->tstat[tellerID], preemptedCustomer); // 入队被抢占的客户（放回队列末尾）
            printf("\tVIP客户 %d 在普通出纳员 %d 插队\n", GetCustomerID(e), tellerID);
          }
          else
          {
            // VIP 在另一个 VIP 或普通客户后面排队，不需要抢占。
            EnqueueCustomer(&s->tstat[tellerID], *e);
          }
        }
        else
        {
          // 普通客户或 VIP 不抢占，直接加入队列
          EnqueueCustomer(&s->tstat[tellerID], *e);
        }
        printf("\t客户 %d 排队等候在出纳员 %d\n", GetCustomerID(e), tellerID);
      }
    }
    else // GetEventType(e) == departure
    {
      printf("时间: %2d\t%s客户 %d 离开\n", GetTime(e), (GetCustomerType(e) == Vip) ? "VIP" : "普通", GetCustomerID(e));
      printf("\t出纳员 %d\t等待时间 %d\t服务时间 %d\n", GetTellerID(e), GetWaitTime(e), GetServiceTime(e));
      tellerID = GetTellerID(e);

      // 出纳员现在空闲（或将在 GetTime(e) 时空闲）。
      // 检查该出纳员队列中是否有等待的客户，并安排下一个。
      if (!IsTellerQueueEmpty(&s->tstat[tellerID]))
      {
        Event nextCustomerInLine = DequeueCustomer(&s->tstat[tellerID]);
        int serviceTimeForNext = Get_ServiceTime(s);
        // 下一个客户的等待时间是从其到达时间到出纳员空闲（当前离开时间）的时间
        int waitTimeForNext = GetTime(e) - GetTime(&nextCustomerInLine);
        if (waitTimeForNext < 0)
          waitTimeForNext = 0; // 不应为负数

        s->tstat[tellerID].totalCustomerWait += waitTimeForNext;
        s->tstat[tellerID].totalCustomerCount++; // 客户在开始服务时计数
        s->tstat[tellerID].totalService += serviceTimeForNext;
        s->tstat[tellerID].finishService = GetTime(e) + serviceTimeForNext; // 出纳员在此客户后空闲

        InitEvent(newevent, s->tstat[tellerID].finishService,
                  departure, GetCustomerID(&nextCustomerInLine), tellerID,
                  waitTimeForNext, serviceTimeForNext, GetCustomerType(&nextCustomerInLine));
        PQInsert(&(s->pq), *newevent);

        if (s->tstat[tellerID].timelineCount < MaxPQSize)
        {
          s->tstat[tellerID].timeline[s->tstat[tellerID].timelineCount] = *newevent;
          s->tstat[tellerID].timelineCount++;
        }
        printf("\t出纳员 %d 开始服务排队的 %s客户 %d\n", tellerID, (GetCustomerType(&nextCustomerInLine) == Vip) ? "VIP" : "普通", GetCustomerID(&nextCustomerInLine));
      }
      else
      {
        // 出纳员变为空闲
        s->tstat[tellerID].finishService = GetTime(e); // 出纳员从此刻起空闲
      }
    }
  }

  // 确保 simulationLength 反映实际的最后一个事件时间，如果它超出了初始设置。
  s->simulationLength = (GetTime(e) <= s->simulationLength) ? s->simulationLength : GetTime(e);

  free(e);
  free(newevent);
}

// 打印仿真结果
void PrintSimulationResults(Simulation *s)
{
  int cumCustomers = 0, cumWait = 0, i, j;
  int avgCustWait;
  float tellerWork;
  int tellerWorkPercent;

  // 计算累积统计数据
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

  // 计算平均客户等待时间
  avgCustWait = (cumCustomers > 0) ? (int)((float)cumWait / cumCustomers + 0.5) : 0;
  printf("%d 分钟\n", avgCustWait);

  // 打印出纳员特定统计数据和时间线
  for (i = 1; i <= s->numTellers; i++)
  {
    printf("\t出纳员 #%d\t工作百分比 ", i);
    tellerWork = (s->simulationLength > 0) ? (float)(s->tstat[i].totalService) / s->simulationLength : 0.0;
    tellerWorkPercent = (int)(tellerWork * 100.0 + 0.5);
    printf("%d%%\n", tellerWorkPercent);

    printf("\t出纳员 #%d 时间线：\n", i);
    // 遍历每个出纳员的事件时间线
    for (j = 0; j < s->tstat[i].timelineCount; j++)
    {
      Event *te = &s->tstat[i].timeline[j];
      printf("\t\t时间 %2d：%s客户 %d，服务时间 %d，等待时间 %d\n",
             GetTime(te), (GetCustomerType(te) == Vip) ? "VIP" : "普通",
             GetCustomerID(te), GetServiceTime(te), GetWaitTime(te));
    }
  }
}

// 新增的出纳员队列辅助函数（简单的循环数组队列）
void EnqueueCustomer(TellerStats *ts, Event customerEvent)
{
  if (ts->queueCount < MaxTellerQueueSize)
  {
    ts->customerQueue[ts->queueTail] = customerEvent;
    ts->queueTail = (ts->queueTail + 1) % MaxTellerQueueSize;
    ts->queueCount++;
  }
  else
  {
    fprintf(stderr, "错误：出纳员队列已满。\n");
    // 错误处理：队列溢出，可能需要重新分配或丢弃客户
  }
}

Event DequeueCustomer(TellerStats *ts)
{
  if (ts->queueCount > 0)
  {
    Event customer = ts->customerQueue[ts->queueHead];
    ts->queueHead = (ts->queueHead + 1) % MaxTellerQueueSize;
    ts->queueCount--;
    return customer;
  }
  else
  {
    // 返回无效事件或处理空队列错误
    fprintf(stderr, "错误：从空的出纳员队列出队。\n");
    Event emptyEvent;
    InitEvent(&emptyEvent, -1, arrival, -1, -1, -1, -1, notVip); // 表示错误
    return emptyEvent;
  }
}

Event PeekCustomer(TellerStats *ts)
{
  if (ts->queueCount > 0)
  {
    return ts->customerQueue[ts->queueHead];
  }
  else
  {
    fprintf(stderr, "错误：从空的出纳员队列窥视。\n");
    Event emptyEvent;
    InitEvent(&emptyEvent, -1, arrival, -1, -1, -1, -1, notVip);
    return emptyEvent;
  }
}

int IsTellerQueueEmpty(TellerStats *ts)
{
  return ts->queueCount == 0;
}

#endif /* SIMULATION */