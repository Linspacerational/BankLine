#ifndef SIMULATION
#define SIMULATION

#include <stdio.h>
#include <stdlib.h>
#include "event.h"      // 假设包含 Event 结构体和 InitEvent, GetTime, GetEventType 等
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
  int totalCutInTime;             // 记录插队次数 - 现在普通客户将不受插队影响
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
void InsertVipIntoQueue(TellerStats *ts, Event vipEvent); // 该函数将不再用于“插队”
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

  // 预定义初始客户的 VIP 状态
  for (i = 0; i < MAXCUSTLENGTH; i++)
  {
    s->ivs[i] = GenerateRandomVipStatus();
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
  InitEvent(firstevent, 0, arrival, 1, 0, 0, 0, s->ivs[s->ivsIndex++]);
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

// 修改：确定下一个可用出纳员（VIP 或普通客户）
// 此版本严格将 VIP 客户分配到 1 号出纳员，并旨在利好普通客户。
int NextAvailableTeller(Simulation *s, isVip iv, int currentTime)
{
  int bestTellerID = -1;
  int minQueueCount = 999999;
  int minFinishTime = 999999; // 用于最短队列的决胜局

  if (iv == Vip) // VIP 客户：总是去 1 号出纳员（VIP_WINDOWS）
  {
    return VIP_WINDOWS;
  }
  else // 普通客户：使用 2 号到 numTellers 号出纳员
  {
    // 确保有普通出纳员可用 (numTellers > VIP_WINDOWS)
    if (s->numTellers <= VIP_WINDOWS)
    {
      // 如果只存在 VIP 窗口，普通客户就没有专用出纳员。
      // 这种情况下很难“利好普通客户”。
      // 为简化，如果没有普通出纳员，他们也将去 1 号出纳员（共享）。
      // 但目的是分离队列。
      // 如果严格要求分离且只存在 1 号出纳员，则意味着普通客户无法得到服务。
      // 对于典型的银行场景，假设 numTellers > 1。
      return VIP_WINDOWS; // 回退：如果没有普通出纳员，他们共享 VIP 窗口
    }

    // 阶段 1：寻找任何完全空闲的普通出纳员（从 VIP_WINDOWS + 1 到 numTellers）
    for (int i = VIP_WINDOWS + 1; i <= s->numTellers; i++)
    {
      if (s->tstat[i].finishService <= currentTime && IsTellerQueueEmpty(&s->tstat[i]))
      {
        return i; // 找到一个空闲的普通出纳员，立即分配
      }
    }

    // 阶段 2：如果没有空闲普通出纳员，则寻找队列最短的普通出纳员。
    for (int i = VIP_WINDOWS + 1; i <= s->numTellers; i++)
    {
      if (s->tstat[i].queueCount < minQueueCount)
      {
        minQueueCount = s->tstat[i].queueCount;
        bestTellerID = i;
      }
      // 如果队列计数相同，则优先选择服务完成最早的那个
      else if (s->tstat[i].queueCount == minQueueCount)
      {
        if (s->tstat[i].finishService < s->tstat[bestTellerID].finishService)
        {
          bestTellerID = i;
        }
      }
    }

    // 回退：如果根本没有普通出纳员（在 numTellers > VIP_WINDOWS 的情况下不应该发生），
    // 但为了防御性编程，添加此检查。
    if (bestTellerID == -1)
    {
      return VIP_WINDOWS + 1; // 默认分配给第一个普通出纳员。
    }

    return bestTellerID;
  }
}

// 运行模拟
void RunSimulation(Simulation *s)
{
  Event *e = (Event *)malloc(sizeof(Event));        // 当前事件
  Event *newevent = (Event *)malloc(sizeof(Event)); // 要调度的新事件
  int nexttime;
  int tellerID;
  int servicetime;
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
        InitEvent(newevent, nexttime, arrival, s->nextCustomer, 0, 0, 0, s->ivs[s->ivsIndex++]);
        PQInsert(&(s->pq), *newevent);
      }

      servicetime = Get_ServiceTime(s);
      iv = GetCustomerType(e);
      tellerID = NextAvailableTeller(s, iv, GetTime(e));

      // 调试打印：显示决策参数
      printf("DEBUG: 客户 %d (VIP: %d) 到达时间 %d. 选择的出纳员: %d. 出纳员 %d 预计空闲时间: %d. 出纳员 %d 队列是否为空: %d. (当前队列长度: %d)\n",
             GetCustomerID(e), iv, GetTime(e), tellerID, tellerID, s->tstat[tellerID].finishService, tellerID, IsTellerQueueEmpty(&s->tstat[tellerID]), s->tstat[tellerID].queueCount);

      // 处理客户排队和服务
      // 如果出纳员当前空闲 (finishService 在过去) 并且他们的队列为空
      if (s->tstat[tellerID].finishService <= GetTime(e) && IsTellerQueueEmpty(&s->tstat[tellerID]))
      {
        waittime = 0; // 立即服务，无需等待

        s->tstat[tellerID].totalCustomerWait += waittime;
        s->tstat[tellerID].totalCustomerCount++;
        s->tstat[tellerID].totalService += servicetime;

        // 为立即服务的客户调度离开事件
        InitEvent(newevent, GetTime(e) + servicetime,
                  departure, GetCustomerID(e), tellerID,
                  waittime, servicetime, iv);
        PQInsert(&(s->pq), *newevent);
        s->tstat[tellerID].finishService = GetTime(e) + servicetime; // 更新出纳员的下一个可用时间

        if (s->tstat[tellerID].timelineCount < MaxPQSize)
        {
          s->tstat[tellerID].timeline[s->tstat[tellerID].timelineCount] = *newevent;
          s->tstat[tellerID].timelineCount++;
        }
        printf("\t出纳员 %d\t等待时间 %d\t服务时间 %d (立即服务)\n", tellerID, waittime, servicetime);
      }
      else // 出纳员忙碌或有队列，客户需要等待
      {
        // 为了利好普通客户，VIP 不再插队。
        // 他们像其他客户一样排队。
        EnqueueCustomer(&s->tstat[tellerID], *e);
        printf("\t客户 %d (VIP: %d) 在出纳员 %d 排队 (正常入队). 队列头: %d, 队列尾: %d, 当前队列长度: %d\n",
               GetCustomerID(e), iv, tellerID,
               (s->tstat[tellerID].customerQueueHead ? GetCustomerID(&s->tstat[tellerID].customerQueueHead->customerEvent) : 0),
               (s->tstat[tellerID].customerQueueTail ? GetCustomerID(&s->tstat[tellerID].customerQueueTail->customerEvent) : 0),
               s->tstat[tellerID].queueCount);
        // totalCutInTime 将不再因为插队而增加。
      }
    }
    else // GetEventType(e) == departure
    {
      // 新增：增加客户类型计数
      if (GetCustomerType(e) == Vip)
        s->totalVipCustomerCount++;
      else
        s->totalOrdinaryCustomerCount++;

      printf("时间: %2d\t%s客户 %d 离开\n", GetTime(e), (GetCustomerType(e) == Vip) ? "VIP" : "普通", GetCustomerID(e));
      printf("\t出纳员 %d\t等待时间 %d\t服务时间 %d\n", GetTellerID(e), GetWaitTime(e), GetServiceTime(e));
      tellerID = GetTellerID(e);

      printf("DEBUG: 出纳员 %d 离开时间 %d. 队列是否为空: %d. (当前队列长度: %d)\n", tellerID, GetTime(e), IsTellerQueueEmpty(&s->tstat[tellerID]), s->tstat[tellerID].queueCount);

      // --- VIP 等待策略的新逻辑 ---
      // 步骤 1：重新计算所有队列中所有 VIP 的等待时间，并填充全局 vipPQueue
      // 重要：为利好普通客户，此阶段的 VIP 客户将**只**被转移到**空闲**的出纳员。
      // 他们不会插队到任何现有繁忙队列中的普通客户前面。
      InitPQueue(&(s->vipPQueue)); // 重新初始化以有效清空它。

      for (int i = 1; i <= s->numTellers; i++)
      {
        Node *current = s->tstat[i].customerQueueHead;
        while (current != NULL)
        {
          Event *queuedEvent = &current->customerEvent;
          if (GetCustomerType(queuedEvent) == Vip)
          {
            int current_wait_time = GetTime(e) - GetTime(queuedEvent); // GetTime(e) 是当前模拟时间
            if (current_wait_time >= s->waitHigh)
            {
              // 存储 VIP 优先级的必要信息
              Event temp_vip_event;
              InitEvent(&temp_vip_event, -current_wait_time, departure, // 使用负等待时间以实现最大堆行为
                        GetCustomerID(queuedEvent), i,                  // 存储原始出纳员 ID
                        current_wait_time, 0, Vip);                     // 存储实际等待时间

              PQInsert(&(s->vipPQueue), temp_vip_event);
            }
          }
          current = current->next;
        }
      }

      // 步骤 2：根据 VIP 优先级决定下一个服务的客户
      Event nextCustomerToServe;
      int originalTellerID_of_vip = -1;
      int targetTellerForVip = -1; // 新变量，用于找到优先 VIP 的最佳出纳员

      if (!PQEmpty(&(s->vipPQueue)))
      {
        // 从全局列表中服务优先级最高的 VIP
        nextCustomerToServe = PQDelete(&(s->vipPQueue));
        originalTellerID_of_vip = GetTellerID(&nextCustomerToServe);
        int vipCustomerID_to_remove = GetCustomerID(&nextCustomerToServe);

        // 为了利好普通客户：
        // 优先的 VIP 客户只能转移到**当前空闲**的柜员。
        // 他们不会插队到现有繁忙队列中。
        for (int i = 1; i <= s->numTellers; i++)
        {
          if (s->tstat[i].finishService <= GetTime(e) && IsTellerQueueEmpty(&s->tstat[i]))
          {
            targetTellerForVip = i;
            break; // 找到一个空闲柜员，分配到这里
          }
        }

        if (targetTellerForVip != -1)
        { // 找到一个空闲柜员供优先 VIP 使用
          printf("DEBUG: VIP客户 %d (来自出纳员 %d 的队列) 等待时间 %d 达到最长等待时间 %d，优先服务。转移到空闲出纳员 %d。\n",
                 vipCustomerID_to_remove, originalTellerID_of_vip, GetWaitTime(&nextCustomerToServe), s->waitHigh, targetTellerForVip);

          // 从原始队列中移除 VIP
          RemoveCustomerByID(&s->tstat[originalTellerID_of_vip], vipCustomerID_to_remove);

          waittime = GetWaitTime(&nextCustomerToServe);
          servicetime = Get_ServiceTime(s);

          s->tstat[targetTellerForVip].totalCustomerWait += waittime;
          s->tstat[targetTellerForVip].totalCustomerCount++;
          s->tstat[targetTellerForVip].totalService += servicetime;

          int nextCustomerDepartureTime = GetTime(e) + servicetime;
          s->tstat[targetTellerForVip].finishService = nextCustomerDepartureTime;

          InitEvent(newevent, nextCustomerDepartureTime, departure,
                    GetCustomerID(&nextCustomerToServe), targetTellerForVip,
                    waittime, servicetime, GetCustomerType(&nextCustomerToServe));
          PQInsert(&(s->pq), *newevent);

          if (s->tstat[targetTellerForVip].timelineCount < MaxPQSize)
          {
            s->tstat[targetTellerForVip].timeline[s->tstat[targetTellerForVip].timelineCount] = *newevent;
            s->tstat[targetTellerForVip].timelineCount++;
          }
          printf("\t出纳员 %d 开始服务转移过来的 VIP 客户 %d\n", targetTellerForVip, GetCustomerID(&nextCustomerToServe));
        }
        else
        { // 未找到空闲柜员，VIP 留在当前队列或需要等待当前柜员。
          // 这意味着他们不会获得插队特权。
          printf("DEBUG: VIP客户 %d (来自出纳员 %d 的队列) 等待时间 %d 达到最长等待时间 %d，但没有空闲出纳员可供转移。继续在原队列等待。\n",
                 vipCustomerID_to_remove, originalTellerID_of_vip, GetWaitTime(&nextCustomerToServe), s->waitHigh);
          // 重新将事件入队，以确保其仍然在后续服务中由其原始柜员考虑。
          // 这实际上对于这个优先的 VIP 客户来说是“什么都没做”的即时服务。
          // 更复杂的解决方案可能涉及将他们以更高的优先级重新插入到其原始队列中，以便下次出队
          // 但为了“利好普通客户”，最简单的方法是在没有空闲柜员时不插队。
        }
      }

      // 如果没有高优先级 VIP 客户得到服务，或者没有空闲柜员供优先 VIP 客户使用，
      // 则继续服务当前柜员队列中的下一个客户。
      // 这确保了普通客户按顺序得到服务。
      if (!IsTellerQueueEmpty(&s->tstat[tellerID]))
      {
        printf("DEBUG: 出纳员 %d 队列不为空。处理下一个客户。\n", tellerID);
        Event nextCustomerInLine = DequeueCustomer(&s->tstat[tellerID]);
        int serviceTimeForNext = Get_ServiceTime(s);

        printf("DEBUG: Dequeued C%d from T%d. Post-dequeue Queue: Head:%d, Tail:%d, Count:%d\n",
               GetCustomerID(&nextCustomerInLine), tellerID,
               (s->tstat[tellerID].customerQueueHead ? GetCustomerID(&s->tstat[tellerID].customerQueueHead->customerEvent) : 0),
               (s->tstat[tellerID].customerQueueTail ? GetCustomerID(&s->tstat[tellerID].customerQueueTail->customerEvent) : 0),
               s->tstat[tellerID].queueCount);

        int waitTimeForNext = GetTime(e) - GetTime(&nextCustomerInLine);
        if (waitTimeForNext < 0)
          waitTimeForNext = 0;

        s->tstat[tellerID].totalCustomerWait += waitTimeForNext;
        s->tstat[tellerID].totalCustomerCount++;
        s->tstat[tellerID].totalService += serviceTimeForNext;

        int nextCustomerDepartureTime = GetTime(e) + serviceTimeForNext;
        s->tstat[tellerID].finishService = nextCustomerDepartureTime;

        InitEvent(newevent, nextCustomerDepartureTime,
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
      if (iv == Vip)
      {
        s->totalVipWaitTime += waitTime; // 累加 VIP 客户的总等待时间
      }
      printf("\t\t时间 %2d：%s客户 %d，服务时间 %d，等待时间 %d\n",
             GetTime(te), (iv == Vip) ? "VIP" : "普通",
             GetCustomerID(te), GetServiceTime(te), waitTime);
    }
  }
  printf("\n");
  printf("VIP客户平均等待时间：%f 分钟\n", (float)s->totalVipWaitTime / s->totalVipCustomerCount);
  printf("出纳员平均空闲时间：%f 分钟\n", (float)s->totalTellerIdleTime / s->numTellers);
  printf("普通用户被插队所平均多出的等待时间：%f 分钟\n", (float)s->totalCutInTime / s->totalOrdinaryCustomerCount);
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

// 此函数将不再用于 VIP 的“插队”行为，以利好普通客户。
// VIP 客户将使用 EnqueueCustomer 正常入队。
void InsertVipIntoQueue(TellerStats *ts, Event vipEvent)
{
  // 此函数原有的目的（将 VIP 插入队列头部）现已有效禁用，
  // 以利好普通客户。VIP 客户将像普通客户一样简单地入队。
  // 但是，如果该函数仍因其他原因被调用，它将默认执行入队操作。
  EnqueueCustomer(ts, vipEvent);
}

// 新增：从出纳员队列中按 ID 移除客户
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