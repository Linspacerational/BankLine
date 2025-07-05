#ifndef SIMULATION
#define SIMULATION

#include <stdio.h>
#include <stdlib.h>
#include "event.h"      // 假设包含 Event 结构体和 InitEvent, GetTime, GetEventType, etc.
typedef Event DataType; // PQueue 使用 Event 作为其 DataType

#include "apqueue.h" // 假设包含 PQueue 结构体和 InitPQueue, PQDelete, PQEmpty

#define MAXCUSTLENGTH 1000
#define MAXTELLERLENGTH 11

int VIP_WINDOWS = 1; // 假设 1 号出纳员是 VIP 窗口

// 定义链表节点结构
struct Node
{
  Event customerEvent;
  struct Node *next;
};
typedef struct Node Node;

struct tellerStats
{
  int finishService;
  int totalCustomerCount;
  int totalCustomerWait;
  int totalService;
  struct event timeline[MaxPQSize]; // 假设 MaxPQSize 在 apqueue.h 或 event.h 中定义
  int timelineCount;
  // 新增：用于客户在此出纳员处等待的链表
  Node *customerQueueHead;
  Node *customerQueueTail;
  int queueCount; // 方便检查队列大小的计数
};
typedef struct tellerStats TellerStats;

struct simulation
{
  int simulationLength;
  int numTellers;
  int nextCustomer;
  int arrivalLow, arrivalHigh;
  int serviceLow, serviceHigh;
  int waitHigh; // VIP 客户能容忍的最长等待时间
  TellerStats tstat[MAXTELLERLENGTH];
  PQueue pq;                // 主事件优先级队列
  PQueue vipPQueue;         // 新增：用于高优先级 VIP 的全局优先级队列
  isVip ivs[MAXCUSTLENGTH]; // 预定义的 VIP 状态
  int ivsIndex;

  // 衡量机制性能的指标
  int totalCutInTime;             // 记录插队次数（被中断的时间）
  int totalVipWaitTime;           // 记录所有 VIP 客户的总等待时间
  int totalTellerIdleTime;        // 记录所有出纳员的总空闲时间
  int totalVipCustomerCount;      // 新增：记录总 VIP 客户数量
  int totalOrdinaryCustomerCount; // 新增：记录总普通客户数量
};
typedef struct simulation Simulation;

// 函数原型
int NextArrivalTime(Simulation *s);
int Get_ServiceTime(Simulation *);
int NextAvailableTeller(Simulation *s, isVip iv, int currentTime); // 修改后的原型
void InitSimulation(Simulation *s);
void RunSimulation(Simulation *s);
void PrintSimulationResults(Simulation *s);
isVip GenerateRandomVipStatus(void);
int GetServiceTimeByCustomerID(TellerStats *ts, int customerID); // 新增：根据客户 ID 获取服务时间

// 新增出纳员队列（链表）的辅助函数
void EnqueueCustomer(TellerStats *ts, Event customerEvent);
Event DequeueCustomer(TellerStats *ts);
Event PeekCustomer(TellerStats *ts);
int IsTellerQueueEmpty(TellerStats *ts);
void InsertVipIntoQueue(TellerStats *ts, Event vipEvent); // 该函数现在用于将客户插入队列头部
void RemoveCustomerByID(TellerStats *ts, int customerID); // 用于 VIP 客户换队列

// 生成随机 VIP 状态（五分之一的几率是 VIP）
isVip GenerateRandomVipStatus(void)
{
  return (rand() % 5 == 0) ? Vip : notVip;
}

// 初始化模拟参数和数据结构
void InitSimulation(Simulation *s)
{
  int i;
  Event *firstevent = (Event *)malloc(sizeof(Event)); // 为第一个事件分配内存

  // 初始化出纳员统计数据及其队列
  for (i = 1; i <= MAXTELLERLENGTH; i++)
  {
    s->tstat[i].finishService = 0;
    s->tstat[i].totalService = 0;
    s->tstat[i].totalCustomerWait = 0;
    s->tstat[i].totalCustomerCount = 0;
    s->tstat[i].timelineCount = 0;
    s->tstat[i].customerQueueHead = NULL; // 初始化链表头
    s->tstat[i].customerQueueTail = NULL; // 初始化链表尾
    s->tstat[i].queueCount = 0;
  }
  s->nextCustomer = 1; // 第一个客户 ID
  s->ivsIndex = 0;     // 预定义 VIP 状态的索引

  s->totalCutInTime = 0;
  s->totalVipWaitTime = 0;
  s->totalTellerIdleTime = 0;

  s->totalVipCustomerCount = 0;
  s->totalOrdinaryCustomerCount = 0;
  isVip temp[10] = {Vip, notVip, Vip, Vip, Vip, notVip, notVip, notVip, notVip, notVip};
  // 预定义初始客户的 VIP 状态
  for (i = 0; i < MAXCUSTLENGTH; i++)
  {
    s->ivs[i] = temp[i % 10];
  }

  // 提示用户输入模拟参数
  printf("输入模拟时间（分钟）：");
  scanf("%d", &s->simulationLength);
  printf("输入出纳员数量：");
  scanf("%d", &s->numTellers);
  printf("输入到达时间范围（分钟）：");
  scanf("%d%d", &s->arrivalLow, &s->arrivalHigh);
  printf("输入服务时间范围（分钟）：");
  scanf("%d%d", &s->serviceLow, &s->serviceHigh);
  printf("Enter the longest waitting time the customer can tolerate in minutes: ");
  scanf("%d", &s->waitHigh); // 用户在此处提供 VIP 等待容忍度

  // 初始化并插入第一个到达事件到优先级队列
  // 修正：在到达事件中存储服务时间
  int firstServiceTime = Get_ServiceTime(s);
  InitEvent(firstevent, 0, arrival, 1, 0, 0, firstServiceTime, s->ivs[s->ivsIndex++]);
  InitPQueue(&(s->pq));
  InitPQueue(&(s->vipPQueue)); // 新增：初始化全局 VIP 优先级队列
  PQInsert(&(s->pq), *firstevent);
  free(firstevent); // 释放临时事件内存
}

// 在指定范围内计算下一个客户到达时间
int NextArrivalTime(Simulation *s)
{
  return s->arrivalLow + rand() % (s->arrivalHigh - s->arrivalLow + 1);
}

// 在指定范围内计算随机服务时间
int Get_ServiceTime(Simulation *s)
{
  return s->serviceLow + rand() % (s->serviceHigh - s->serviceLow + 1);
}

// 修改：确定下一个可用出纳员，优先 VIP
int NextAvailableTeller(Simulation *s, isVip iv, int currentTime)
{
  int bestTellerID = -1;
  int minFinishTime = 9999999; // 使用一个较大的初始值
  int minQueueCount = 9999999;

  // 阶段 1：寻找任何完全空闲的出纳员
  for (int i = 1; i <= s->numTellers; i++)
  {
    if (s->tstat[i].finishService <= currentTime && IsTellerQueueEmpty(&s->tstat[i]))
    {
      return i; // 找到一个空闲出纳员，立即返回
    }
  }

  // 阶段 2：如果没有空闲出纳员
  if (iv == Vip) // VIP 客户：寻找预计完成服务时间最早的出纳员（可能中断）
  {
    for (int i = 1; i <= s->numTellers; i++)
    {
      if (s->tstat[i].finishService < minFinishTime)
      {
        minFinishTime = s->tstat[i].finishService;
        bestTellerID = i;
      }
      else if (s->tstat[i].finishService == minFinishTime)
      {
        // 完成时间相同，选择队列更短的
        if (s->tstat[i].queueCount < s->tstat[bestTellerID].queueCount)
        {
          bestTellerID = i;
        }
      }
    }
  }
  else // 普通客户：寻找队列最短的出纳员
  {
    for (int i = 1; i <= s->numTellers; i++)
    {
      if (s->tstat[i].queueCount < minQueueCount)
      {
        minQueueCount = s->tstat[i].queueCount;
        bestTellerID = i;
      }
      else if (s->tstat[i].queueCount == minQueueCount)
      {
        // 队列长度相同，选择完成时间更早的
        if (s->tstat[i].finishService < s->tstat[bestTellerID].finishService)
        {
          bestTellerID = i;
        }
      }
    }
  }

  // 默认分配给 1 号出纳员（如果前面都没有找到）
  if (bestTellerID == -1)
  {
    return 1;
  }
  return bestTellerID;
}

// 运行模拟
void RunSimulation(Simulation *s)
{
  Event *e = (Event *)malloc(sizeof(Event));        // 当前事件
  Event *newevent = (Event *)malloc(sizeof(Event)); // 要调度的新事件
  int nexttime;
  int tellerID;
  int waittime;
  isVip iv;

  while (!PQEmpty(&(s->pq)))
  {
    *e = PQDelete(&(s->pq)); // 从优先级队列中获取下一个事件

    // 如果事件时间超过模拟长度，考虑它是否是不应该处理的到达事件
    if (GetTime(e) > s->simulationLength)
    {
      if (GetEventType(e) == arrival)
      {
        // 如果到达事件发生在模拟长度之后，我们不再处理它。
        // 这阻止了新客户在模拟名义结束之后进入系统。
        continue;
      }
      // 发生在模拟长度之后的离开事件仍必须处理，
      // 以正确计算出纳员统计信息并清空队列。
    }

    if (GetEventType(e) == arrival)
    {
      printf("时间: %2d\t%s客户 %d 到达\n", GetTime(e), (GetCustomerType(e) == Vip) ? "VIP" : "普通", GetCustomerID(e));

      // 如果在模拟限制和预定义 VIP 列表限制内，调度下一个到达事件
      nexttime = GetTime(e) + NextArrivalTime(s);
      if (nexttime <= s->simulationLength && s->ivsIndex < MAXCUSTLENGTH)
      {
        s->nextCustomer++;
        // 修正：在到达事件中存储服务时间
        int newCustomerServiceTime = Get_ServiceTime(s);
        InitEvent(newevent, nexttime, arrival, s->nextCustomer, 0, 0, newCustomerServiceTime, s->ivs[s->ivsIndex++]);
        PQInsert(&(s->pq), *newevent);
      }

      // 获取当前到达客户的服务时间（已在 Event 中存储）
      int currentCustomerServiceTime = GetServiceTime(e);
      iv = GetCustomerType(e);
      tellerID = NextAvailableTeller(s, iv, GetTime(e));

      // 调试打印：显示决策参数
      printf("DEBUG: 客户 %d (VIP: %d) 到达时间 %d. 选择的出纳员: %d. 出纳员 %d 预计空闲时间: %d. 出纳员 %d 队列是否为空: %d. (当前队列长度: %d)\n",
             GetCustomerID(e), iv, GetTime(e), tellerID, tellerID, s->tstat[tellerID].finishService, tellerID, IsTellerQueueEmpty(&s->tstat[tellerID]), s->tstat[tellerID].queueCount);

      if (iv == Vip) // VIP 客户到达
      {
        // 检查出纳员是否忙碌且当前服务的是普通客户，是否可以中断
        if (s->tstat[tellerID].finishService > GetTime(e)) // 出纳员忙碌
        {
          Event *currentlyServedEvent = NULL;
          // 获取当前正在服务的客户事件（通常是 timeline 中的最后一个事件）
          if (s->tstat[tellerID].timelineCount > 0)
          {
            currentlyServedEvent = &s->tstat[tellerID].timeline[s->tstat[tellerID].timelineCount - 1];
          }

          // 确保有客户正在服务，且该客户是普通客户
          if (currentlyServedEvent != NULL && GetCustomerType(currentlyServedEvent) == notVip)
          {
            // 发生中断：VIP 插队，普通客户被打断
            int interruptedCustomerID = GetCustomerID(currentlyServedEvent);
            int originalServiceTime = GetServiceTime(currentlyServedEvent);
            // 实际开始服务时间 = 预计离开时间 - 原始服务时间
            int serviceStartTime = GetTime(currentlyServedEvent) - GetServiceTime(currentlyServedEvent);
            int timeServedSoFar = GetTime(e) - serviceStartTime;              // 已服务时间
            int remainingServiceTime = originalServiceTime - timeServedSoFar; // 剩余服务时间
            if (remainingServiceTime < 0)
              remainingServiceTime = 0; // 避免负值

            printf("时间: %2d\tVIP客户 %d 插队到出纳员 %d。普通客户 %d 被打断。已服务时间: %d，剩余服务时间: %d。\n",
                   GetTime(e), GetCustomerID(e), tellerID, interruptedCustomerID, timeServedSoFar, remainingServiceTime);

            // 1. 中断服务：出纳员立即空闲
            s->tstat[tellerID].finishService = GetTime(e);
            // 修正：从总服务时间中扣除未完成的服务时间
            s->tstat[tellerID].totalService -= remainingServiceTime;

            // 2. 将被打断的普通客户放回队列头部
            Event interruptedCustomerRequeuedEvent;
            InitEvent(&interruptedCustomerRequeuedEvent, GetTime(e), arrival, // 重新设置为到达事件类型
                      interruptedCustomerID, tellerID,
                      GetWaitTime(currentlyServedEvent), // 仅携带原始等待时间，不加已服务时间
                      originalServiceTime, notVip);

            InsertVipIntoQueue(&s->tstat[tellerID], interruptedCustomerRequeuedEvent); // 插入队列头部

            // 3. 计算插队造成的额外等待时间
            s->totalCutInTime += remainingServiceTime;

            // 4. 立即开始服务当前到达的 VIP 客户
            waittime = 0; // VIP 立即服务，等待时间为 0
            s->tstat[tellerID].totalCustomerWait += waittime;
            s->tstat[tellerID].totalCustomerCount++;
            // 使用 VIP 客户自己的服务时间
            s->tstat[tellerID].totalService += currentCustomerServiceTime;

            InitEvent(newevent, GetTime(e) + currentCustomerServiceTime, departure,
                      GetCustomerID(e), tellerID, waittime, currentCustomerServiceTime, iv);
            PQInsert(&(s->pq), *newevent);
            s->tstat[tellerID].finishService = GetTime(e) + currentCustomerServiceTime; // 更新出纳员的下一个可用时间

            if (s->tstat[tellerID].timelineCount < MaxPQSize)
            {
              s->tstat[tellerID].timeline[s->tstat[tellerID].timelineCount] = *newevent;
              s->tstat[tellerID].timelineCount++;
            }
            printf("\t出纳员 %d\t等待时间 %d\t服务时间 %d (VIP 立即服务，普通客户被中断)\n", tellerID, waittime, currentCustomerServiceTime);
          }
          else
          { // 出纳员忙碌，但不是在服务可中断的普通客户（例如，在服务另一个 VIP）
            printf("DEBUG: VIP客户 %d 到达，出纳员 %d 忙碌但无法中断普通客户或服务中为其他 VIP。正常入队。\n", GetCustomerID(e), tellerID);
            EnqueueCustomer(&s->tstat[tellerID], *e); // VIP 正常排队
          }
        }
        else if (s->tstat[tellerID].finishService <= GetTime(e) && IsTellerQueueEmpty(&s->tstat[tellerID]))
        {
          // 出纳员空闲且队列为空，VIP 立即服务
          waittime = 0;
          s->tstat[tellerID].totalCustomerWait += waittime;
          s->tstat[tellerID].totalCustomerCount++;
          // 使用 VIP 客户自己的服务时间
          s->tstat[tellerID].totalService += currentCustomerServiceTime;

          InitEvent(newevent, GetTime(e) + currentCustomerServiceTime,
                    departure, GetCustomerID(e), tellerID,
                    waittime, currentCustomerServiceTime, iv);
          PQInsert(&(s->pq), *newevent);
          s->tstat[tellerID].finishService = GetTime(e) + currentCustomerServiceTime;

          if (s->tstat[tellerID].timelineCount < MaxPQSize)
          {
            s->tstat[tellerID].timeline[s->tstat[tellerID].timelineCount] = *newevent;
            s->tstat[tellerID].timelineCount++;
          }
          printf("\t出纳员 %d\t等待时间 %d\t服务时间 %d (立即服务)\n", tellerID, waittime, currentCustomerServiceTime);
        }
        else // 出纳员忙碌，有队列，且 VIP 无法中断（例如，队列中有其他 VIP）
        {
          // VIP 客户正常排队
          EnqueueCustomer(&s->tstat[tellerID], *e);
          printf("\t客户 %d (VIP: %d) 在出纳员 %d 排队 (正常入队). 队列头: %d, 队列尾: %d, 当前队列长度: %d\n",
                 GetCustomerID(e), iv, tellerID,
                 (s->tstat[tellerID].customerQueueHead ? GetCustomerID(&s->tstat[tellerID].customerQueueHead->customerEvent) : 0),
                 (s->tstat[tellerID].customerQueueTail ? GetCustomerID(&s->tstat[tellerID].customerQueueTail->customerEvent) : 0),
                 s->tstat[tellerID].queueCount);
        }
      }
      else // 普通客户到达
      {
        if (s->tstat[tellerID].finishService <= GetTime(e) && IsTellerQueueEmpty(&s->tstat[tellerID]))
        {
          waittime = 0; // 立即服务
          s->tstat[tellerID].totalCustomerWait += waittime;
          s->tstat[tellerID].totalCustomerCount++;
          // 使用普通客户自己的服务时间
          s->tstat[tellerID].totalService += currentCustomerServiceTime;

          InitEvent(newevent, GetTime(e) + currentCustomerServiceTime,
                    departure, GetCustomerID(e), tellerID,
                    waittime, currentCustomerServiceTime, iv);
          PQInsert(&(s->pq), *newevent);
          s->tstat[tellerID].finishService = GetTime(e) + currentCustomerServiceTime; // 更新出纳员的下一个可用时间

          if (s->tstat[tellerID].timelineCount < MaxPQSize)
          {
            s->tstat[tellerID].timeline[s->tstat[tellerID].timelineCount] = *newevent;
            s->tstat[tellerID].timelineCount++;
          }
          printf("\t出纳员 %d\t等待时间 %d\t服务时间 %d (立即服务)\n", tellerID, waittime, currentCustomerServiceTime);
        }
        else // 客户需要排队
        {
          EnqueueCustomer(&s->tstat[tellerID], *e); // *e 携带了正确的服务时间
          printf("\t客户 %d (VIP: %d) 在出纳员 %d 排队 (正常入队). 队列头: %d, 队列尾: %d, 当前队列长度: %d\n",
                 GetCustomerID(e), iv, tellerID,
                 (s->tstat[tellerID].customerQueueHead ? GetCustomerID(&s->tstat[tellerID].customerQueueHead->customerEvent) : 0),
                 (s->tstat[tellerID].customerQueueTail ? GetCustomerID(&s->tstat[tellerID].customerQueueTail->customerEvent) : 0),
                 s->tstat[tellerID].queueCount);
        }
      }
    }
    else // GetEventType(e) == departure
    {
      // 累加 VIP/普通客户数量和等待时间
      if (GetCustomerType(e) == Vip)
      {
        s->totalVipCustomerCount++;
        s->totalVipWaitTime += GetWaitTime(e); // 累加 VIP 的实际等待时间
      }
      else
      {
        s->totalOrdinaryCustomerCount++;
      }

      printf("时间: %2d\t%s客户 %d 离开\n", GetTime(e), (GetCustomerType(e) == Vip) ? "VIP" : "普通", GetCustomerID(e));
      printf("\t出纳员 %d\t等待时间 %d\t服务时间 %d\n", GetTellerID(e), GetWaitTime(e), GetServiceTime(e));
      tellerID = GetTellerID(e);

      printf("DEBUG: 出纳员 %d 离开时间 %d. 队列是否为空: %d. (当前队列长度: %d)\n", tellerID, GetTime(e), IsTellerQueueEmpty(&s->tstat[tellerID]), s->tstat[tellerID].queueCount);

      // --- VIP 长等待策略 ---
      // 步骤 1：重新检查所有队列中的 VIP，将等待时间过长的 VIP 放入全局 vipPQueue
      InitPQueue(&(s->vipPQueue)); // 重新初始化以清空

      for (int i = 1; i <= s->numTellers; i++)
      {
        Node *current = s->tstat[i].customerQueueHead;
        while (current != NULL)
        {
          Event *queuedEvent = &current->customerEvent;
          if (GetCustomerType(queuedEvent) == Vip)
          {
            int current_wait_time = GetTime(e) - GetTime(queuedEvent); // 当前已等待时间
            if (current_wait_time >= s->waitHigh)
            {
              // 存储必要信息，并用负等待时间以便 PQ 排序（小值优先）
              Event temp_vip_event;
              // 修正：直接从队列中的事件获取服务时间
              InitEvent(&temp_vip_event, -current_wait_time, departure,
                        GetCustomerID(queuedEvent), i,                        // 存储原始出纳员 ID
                        current_wait_time, GetServiceTime(queuedEvent), Vip); // 存储实际等待时间和服务时间
              PQInsert(&(s->vipPQueue), temp_vip_event);
            }
          }
          current = current->next;
        }
      }

      // 步骤 2：决定下一个客户服务（优先处理等待过长的 VIP）
      if (!PQEmpty(&(s->vipPQueue)))
      {
        Event highPriorityVip = PQDelete(&(s->vipPQueue));
        int vipCustomerID = GetCustomerID(&highPriorityVip);
        int originalTeller = GetTellerID(&highPriorityVip); // VIP 最初所在的队列出纳员
        int vipWaitedTime = GetWaitTime(&highPriorityVip);
        int vipServiceTime = GetServiceTime(&highPriorityVip); // VIP 原始的服务时间

        // 寻找一个空闲的出纳员，或者一个可以中断普通客户的出纳员
        int targetTellerForVip = -1;
        int foundInterruptibleTeller = 0;

        // 1. 优先寻找空闲出纳员
        for (int i = 1; i <= s->numTellers; i++)
        {
          if (s->tstat[i].finishService <= GetTime(e) && IsTellerQueueEmpty(&s->tstat[i]))
          {
            targetTellerForVip = i;
            break;
          }
        }

        // 2. 如果没有空闲出纳员，寻找当前正在服务普通客户的出纳员进行中断
        if (targetTellerForVip == -1)
        {
          for (int i = 1; i <= s->numTellers; i++)
          {
            if (s->tstat[i].finishService > GetTime(e))
            { // 出纳员忙碌
              Event *currentlyServedEvent = NULL;
              if (s->tstat[i].timelineCount > 0)
              {
                currentlyServedEvent = &s->tstat[i].timeline[s->tstat[i].timelineCount - 1];
              }
              if (currentlyServedEvent != NULL && GetCustomerType(currentlyServedEvent) == notVip)
              {
                targetTellerForVip = i; // 找到可中断的出纳员
                foundInterruptibleTeller = 1;
                break;
              }
            }
          }
        }

        if (targetTellerForVip != -1)
        {
          // 从原队列中移除 VIP
          RemoveCustomerByID(&s->tstat[originalTeller], vipCustomerID);

          if (foundInterruptibleTeller)
          {
            // 执行中断逻辑
            Event *interruptedCustomerEvent = &s->tstat[targetTellerForVip].timeline[s->tstat[targetTellerForVip].timelineCount - 1];
            int interruptedCustomerID = GetCustomerID(interruptedCustomerEvent);
            int originalInterruptedServiceTime = GetServiceTime(interruptedCustomerEvent);
            int serviceStartTime = GetTime(interruptedCustomerEvent) - GetServiceTime(interruptedCustomerEvent); // 修正为服务开始时间
            int timeServedSoFar = GetTime(e) - serviceStartTime;
            int remainingServiceTime = originalInterruptedServiceTime - timeServedSoFar;
            if (remainingServiceTime < 0)
              remainingServiceTime = 0;

            printf("时间: %2d\t优先 VIP客户 %d (来自原出纳员 %d) 转移到出纳员 %d 并中断普通客户 %d。已服务时间: %d，剩余服务时间: %d。\n",
                   GetTime(e), vipCustomerID, originalTeller, targetTellerForVip, interruptedCustomerID, timeServedSoFar, remainingServiceTime);

            // 中断服务，出纳员立即空闲
            s->tstat[targetTellerForVip].finishService = GetTime(e);
            // 修正：从总服务时间中扣除未完成的服务时间
            s->tstat[targetTellerForVip].totalService -= remainingServiceTime;

            // 将被打断的普通客户放回队列头部
            Event interruptedCustomerRequeuedEvent;
            InitEvent(&interruptedCustomerRequeuedEvent, GetTime(e), arrival,
                      interruptedCustomerID, targetTellerForVip,
                      GetWaitTime(interruptedCustomerEvent), // 仅携带原始等待时间
                      originalInterruptedServiceTime, notVip);
            InsertVipIntoQueue(&s->tstat[targetTellerForVip], interruptedCustomerRequeuedEvent);

            s->totalCutInTime += remainingServiceTime;
          }

          // 服务优先的 VIP
          s->tstat[targetTellerForVip].totalCustomerWait += vipWaitedTime; // 使用累积的等待时间
          s->tstat[targetTellerForVip].totalCustomerCount++;
          s->tstat[targetTellerForVip].totalService += vipServiceTime; // VIP 原始的服务时间

          int vipDepartureTime = GetTime(e) + vipServiceTime;
          s->tstat[targetTellerForVip].finishService = vipDepartureTime;

          InitEvent(newevent, vipDepartureTime, departure,
                    vipCustomerID, targetTellerForVip,
                    vipWaitedTime, vipServiceTime, Vip);
          PQInsert(&(s->pq), *newevent);

          if (s->tstat[targetTellerForVip].timelineCount < MaxPQSize)
          {
            s->tstat[targetTellerForVip].timeline[s->tstat[targetTellerForVip].timelineCount] = *newevent;
            s->tstat[targetTellerForVip].timelineCount++;
          }
          printf("\t出纳员 %d 开始服务高优先级 VIP 客户 %d\n", targetTellerForVip, vipCustomerID);
        }
        else
        {
          // 没有找到合适的空闲或可中断的出纳员。VIP 留在原队列。
          printf("DEBUG: 优先 VIP客户 %d 无法转移到空闲或可中断的柜员。继续在原队列等待。\n", vipCustomerID);
          // 此 VIP 事件已从 vipPQueue 中移除，它将由其原始队列在适当时候处理。
        }
      }

      // 如果没有高优先级 VIP 客户得到服务，或者该 VIP 无法转移，
      // 则继续服务当前出纳员队列中的下一个客户。
      if (!IsTellerQueueEmpty(&s->tstat[tellerID]))
      {
        printf("DEBUG: 出纳员 %d 队列不为空。处理下一个客户。\n", tellerID);
        Event nextCustomerInLine = DequeueCustomer(&s->tstat[tellerID]);
        // 修正：从事件中获取服务时间，而不是重新生成
        int serviceTimeForNext = GetServiceTime(&nextCustomerInLine);

        printf("DEBUG: Dequeued C%d from T%d. Post-dequeue Queue: Head:%d, Tail:%d, Count:%d\n",
               GetCustomerID(&nextCustomerInLine), tellerID,
               (s->tstat[tellerID].customerQueueHead ? GetCustomerID(&s->tstat[tellerID].customerQueueHead->customerEvent) : 0),
               (s->tstat[tellerID].customerQueueTail ? GetCustomerID(&s->tstat[tellerID].customerQueueTail->customerEvent) : 0),
               s->tstat[tellerID].queueCount);

        int waitTimeForNext = GetTime(e) - GetTime(&nextCustomerInLine); // 当前时间 - 客户重新入队时间
        if (waitTimeForNext < 0)
          waitTimeForNext = 0;

        // 累积该客户的总等待时间：之前累积的等待时间 + 重新排队后的等待时间
        int totalCustomerAccumulatedWait = GetWaitTime(&nextCustomerInLine) + waitTimeForNext;

        s->tstat[tellerID].totalCustomerWait += totalCustomerAccumulatedWait;
        s->tstat[tellerID].totalCustomerCount++;
        s->tstat[tellerID].totalService += serviceTimeForNext; // 使用本次服务时间

        int nextCustomerDepartureTime = GetTime(e) + serviceTimeForNext;
        s->tstat[tellerID].finishService = nextCustomerDepartureTime;

        InitEvent(newevent, nextCustomerDepartureTime,
                  departure, GetCustomerID(&nextCustomerInLine), tellerID,
                  totalCustomerAccumulatedWait, serviceTimeForNext, GetCustomerType(&nextCustomerInLine)); // 使用累积等待时间
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
        printf("DEBUG: 出纳员 %d 队列为空。出纳员变为空闲。\n", tellerID);
        s->tstat[tellerID].finishService = GetTime(e);
      }
    }
  }

  // 确保 simulationLength 反映了实际的最后一个事件时间（如果它超出了初始长度）
  if (GetTime(e) > s->simulationLength)
  {
    s->simulationLength = GetTime(e);
  }

  free(e);
  free(newevent);
}

// 打印模拟结果
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

  // 计算总出纳员空闲时间
  for (i = 1; i <= s->numTellers; i++)
  {
    s->totalTellerIdleTime += (s->simulationLength - s->tstat[i].totalService);
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
    // 确保 simulationLength 为正，避免除以零
    tellerWork = (s->simulationLength > 0) ? (float)(s->tstat[i].totalService) / s->simulationLength : 0.0;
    tellerWorkPercent = (int)(tellerWork * 100.0 + 0.5);
    printf("%d%%\n", tellerWorkPercent);

    printf("\t出纳员 #%d 时间线：\n", i);
    // 遍历每个出纳员的事件时间线
    for (j = 0; j < s->tstat[i].timelineCount; j++)
    {
      Event *te = &s->tstat[i].timeline[j];
      int iv = GetCustomerType(te);
      int waitTime = GetWaitTime(te);
      printf("\t\t时间 %2d：%s客户 %d，服务时间 %d，等待时间 %d\n",
             GetTime(te), (iv == Vip) ? "VIP" : "普通",
             GetCustomerID(te), GetServiceTime(te), waitTime);
    }
  }
  printf("\n");
  printf("VIP客户平均等待时间：%f 分钟\n", (s->totalVipCustomerCount > 0) ? (float)s->totalVipWaitTime / s->totalVipCustomerCount : 0.0);
  printf("出纳员平均空闲时间：%f 分钟\n", (s->numTellers > 0) ? (float)s->totalTellerIdleTime / s->numTellers : 0.0);
  printf("普通用户被插队所平均多出的等待时间：%f 分钟\n", (s->totalOrdinaryCustomerCount > 0) ? (float)s->totalCutInTime / s->totalOrdinaryCustomerCount : 0.0);
}

// 新增出纳员队列（链表）的辅助函数
void EnqueueCustomer(TellerStats *ts, Event customerEvent)
{
  Node *newNode = (Node *)malloc(sizeof(Node));
  if (newNode == NULL)
  {
    fprintf(stderr, "错误：为新节点分配内存失败。\n");
    return;
  }
  newNode->customerEvent = customerEvent;
  newNode->next = NULL;

  if (ts->customerQueueTail == NULL) // 队列为空
  {
    ts->customerQueueHead = newNode;
    ts->customerQueueTail = newNode;
  }
  else
  {
    ts->customerQueueTail->next = newNode;
    ts->customerQueueTail = newNode;
  }
  ts->queueCount++;
}

Event DequeueCustomer(TellerStats *ts)
{
  if (ts->customerQueueHead == NULL)
  {
    fprintf(stderr, "错误：从空的出纳员队列中出队。\n");
    Event emptyEvent;
    InitEvent(&emptyEvent, -1, arrival, -1, -1, -1, -1, notVip); // 指示错误
    return emptyEvent;
  }

  Node *temp = ts->customerQueueHead;
  Event customer = temp->customerEvent;
  ts->customerQueueHead = ts->customerQueueHead->next;

  if (ts->customerQueueHead == NULL) // 最后一个元素出队
  {
    ts->customerQueueTail = NULL;
  }
  free(temp);
  ts->queueCount--;
  return customer;
}

Event PeekCustomer(TellerStats *ts)
{
  if (ts->customerQueueHead == NULL)
  {
    fprintf(stderr, "错误：从空的出纳员队列中窥视。\n");
    Event emptyEvent;
    InitEvent(&emptyEvent, -1, arrival, -1, -1, -1, -1, notVip);
    return emptyEvent;
  }
  return ts->customerQueueHead->customerEvent;
}

int IsTellerQueueEmpty(TellerStats *ts)
{
  return ts->customerQueueHead == NULL;
}

// 修改：该函数现在用于将客户插入队列头部 (例如，被打断的普通客户)
void InsertVipIntoQueue(TellerStats *ts, Event customerEvent)
{
  Node *newNode = (Node *)malloc(sizeof(Node));
  if (newNode == NULL)
  {
    fprintf(stderr, "错误：为新节点分配内存失败。\n");
    return;
  }
  newNode->customerEvent = customerEvent;
  newNode->next = NULL;

  if (ts->customerQueueHead == NULL) // 队列为空
  {
    ts->customerQueueHead = newNode;
    ts->customerQueueTail = newNode;
  }
  else
  {
    // 插入队列头部
    newNode->next = ts->customerQueueHead;
    ts->customerQueueHead = newNode;
  }
  ts->queueCount++;
}

// 从出纳员队列中按 ID 移除客户
void RemoveCustomerByID(TellerStats *ts, int customerID)
{
  Node *current = ts->customerQueueHead;
  Node *prev = NULL;

  while (current != NULL && GetCustomerID(&current->customerEvent) != customerID)
  {
    prev = current;
    current = current->next;
  }

  if (current == NULL)
  {
    // 客户未在此队列中找到，无需操作
    return;
  }

  if (prev == NULL)
  {
    // 移除头部节点
    ts->customerQueueHead = current->next;
  }
  else
  {
    // 移除中间或尾部节点
    prev->next = current->next;
  }

  if (current == ts->customerQueueTail)
  {
    // 如果移除的节点是尾部，更新尾部
    ts->customerQueueTail = prev;
  }

  free(current);
  ts->queueCount--;
  // 如果移除后队列变空，确保尾部为 NULL
  if (ts->customerQueueHead == NULL)
  {
    ts->customerQueueTail = NULL;
  }
}

// 根据 customerID 查找其服务时间
int GetServiceTimeByCustomerID(TellerStats *ts, int customerID)
{
  Node *curr = ts->customerQueueHead;
  while (curr != NULL)
  {
    if (GetCustomerID(&curr->customerEvent) == customerID)
    {
      return GetServiceTime(&curr->customerEvent);
    }
    curr = curr->next;
  }
  return 0; // 未找到，返回 0
}

#endif /* SIMULATION */