#ifndef SIMULATION
#define SIMULATION

#include <stdio.h>
#include <stdlib.h>
#include "event.h"      // Assumed to contain Event structure and InitEvent, GetTime, GetEventType, etc.
typedef Event DataType; // PQueue uses Event as its DataType

#include "apqueue.h" // Assumed to contain PQueue structure and InitPQueue, PQInsert, PQDelete, PQEmpty

#define MAXCUSTLENGTH 1000
#define MAXTELLERLENGTH 11

int VIP_WINDOWS = 1;

// Define a Node structure for the linked list
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
  struct event timeline[MaxPQSize]; // Assuming MaxPQSize is defined in apqueue.h or event.h
  int timelineCount;
  // New: Linked list for customers waiting at this teller
  Node *customerQueueHead;
  Node *customerQueueTail;
  int queueCount; // Keep count for easy size check
};
typedef struct tellerStats TellerStats;

struct simulation
{
  int simulationLength;
  int numTellers;
  int nextCustomer;
  int arrivalLow, arrivalHigh;
  int serviceLow, serviceHigh;
  int waitHigh; // Max waiting time a VIP can tolerate
  TellerStats tstat[MAXTELLERLENGTH];
  PQueue pq;        // Main event priority queue
  PQueue vipPQueue; // NEW: Global priority queue for high-priority VIPs
  isVip ivs[MAXCUSTLENGTH];    // Predefined VIP statuses
  int ivsIndex;

  // 衡量机制性能的指标
  int totalCutInTime; // 记录插队次数
  int totalVipWaitTime; // 记录所有VIP客户的总等待时间
  int totalTellerIdleTime; // 记录所有出纳员的总空闲时间
  int totalVipCustomerCount;      // NEW: 记录总VIP客户数量
  int totalOrdinaryCustomerCount; // NEW: 记录总普通客户数量
};
typedef struct simulation Simulation;

// Function prototypes
int NextArrivalTime(Simulation *s);
int Get_ServiceTime(Simulation *);
int NextAvailableTeller(Simulation *s, isVip iv, int currentTime);
void InitSimulation(Simulation *s);
void RunSimulation(Simulation *s);
void PrintSimulationResults(Simulation *s);
isVip GenerateRandomVipStatus(void);
int GetServiceTimeByCustomerID(TellerStats *ts, int customerID); // NEW: Get service time by customer ID

// Helper functions for the new teller queue (linked list)
void EnqueueCustomer(TellerStats *ts, Event customerEvent);
Event DequeueCustomer(TellerStats *ts);
Event PeekCustomer(TellerStats *ts);
int IsTellerQueueEmpty(TellerStats *ts);
void InsertVipIntoQueue(TellerStats *ts, Event vipEvent); // New function for VIP insertion
void RemoveCustomerByID(TellerStats *ts, int customerID); // NEW: For VIP changing queues

// Generates a random VIP status (1 in 5 chance is VIP)
isVip GenerateRandomVipStatus(void)
{
  return (rand() % 5 == 0) ? Vip : notVip;
}

// Initializes the simulation parameters and data structures
void InitSimulation(Simulation *s)
{
  int i;
  Event *firstevent = (Event *)malloc(sizeof(Event)); // Allocate memory for the first event

  // Initialize teller statistics and their queues
  for (i = 1; i <= MAXTELLERLENGTH; i++)
  {
    s->tstat[i].finishService = 0;
    s->tstat[i].totalService = 0;
    s->tstat[i].totalCustomerWait = 0;
    s->tstat[i].totalCustomerCount = 0;
    s->tstat[i].timelineCount = 0;
    s->tstat[i].customerQueueHead = NULL; // Initialize linked list head
    s->tstat[i].customerQueueTail = NULL; // Initialize linked list tail
    s->tstat[i].queueCount = 0;
  }
  s->nextCustomer = 1; // First customer ID
  s->ivsIndex = 0;     // Index for predefined VIP statuses

  s->totalCutInTime = 0;      
  s->totalVipWaitTime = 0;    
  s->totalTellerIdleTime = 0; 

  s->totalVipCustomerCount = 0;      
  s->totalOrdinaryCustomerCount = 0; 

  // Predefined VIP statuses for initial customers
  // isVip temp[COMMONLENGTH] = {Vip, notVip, Vip, Vip, Vip, notVip, Vip, notVip, notVip, Vip}; // Updated VIP sequence
  for (i = 0; i < MAXCUSTLENGTH; i++)
  {
    s->ivs[i] = GenerateRandomVipStatus();
  }

  // Prompt user for simulation parameters
  printf("输入模拟时间（分钟）：");
  scanf("%d", &s->simulationLength);
  printf("输入出纳员数量：");
  scanf("%d", &s->numTellers); // Assuming numTellers is now read as a single integer
  printf("输入到达时间范围（分钟）：");
  scanf("%d%d", &s->arrivalLow, &s->arrivalHigh);
  printf("输入服务时间范围（分钟）：");
  scanf("%d%d", &s->serviceLow, &s->serviceHigh);
  printf("Enter the longest waitting time the customer can tolerate in minutes: ");
  scanf("%d", &s->waitHigh); // User provides the VIP wait tolerance here

  // Initialize and insert the first arrival event into the priority queue
  InitEvent(firstevent, 0, arrival, 1, 0, 0, 0, s->ivs[s->ivsIndex++]);
  InitPQueue(&(s->pq));
  InitPQueue(&(s->vipPQueue)); // NEW: Initialize global VIP priority queue
  PQInsert(&(s->pq), *firstevent);
  free(firstevent); // Free the temporary event memory
}

// Calculates the next customer arrival time within the specified range
int NextArrivalTime(Simulation *s)
{
  return s->arrivalLow + rand() % (s->arrivalHigh - s->arrivalLow + 1);
}

// Calculates a random service time within the specified range
int Get_ServiceTime(Simulation *s)
{
  return s->serviceLow + rand() % (s->serviceHigh - s->serviceLow + 1);
}

// Determines the next available teller based on VIP status and teller availability
int NextAvailableTeller(Simulation *s, isVip iv, int currentTime)
{
  int bestTellerID = -1;
  int minFinishTime = 999999;
  int minQueueCount = 999999;

  if (iv == Vip)
  {
    // 1. Look for any completely idle teller (VIP or Regular)
    // 优先选择 VIP 窗口 (如果存在且空闲)，再选择其他空闲窗口
    if (VIP_WINDOWS > 0 && s->tstat[VIP_WINDOWS].finishService <= currentTime && IsTellerQueueEmpty(&s->tstat[VIP_WINDOWS]))
    {
      return VIP_WINDOWS; // VIP专属窗口空闲，立即分配
    }
    for (int i = 1; i <= s->numTellers; i++)
    {
      if (s->tstat[i].finishService <= currentTime && IsTellerQueueEmpty(&s->tstat[i]))
      {
        return i; // 找到其他空闲出纳员，立即分配
      }
    }

    // 2. 如果没有空闲出纳员，VIP会排到某个出纳员的队列。
    // 为了利好银行（即尽量减少因VIP插队造成的普通客户额外等待），
    // 让VIP优先选择 VIP 窗口，如果 VIP 窗口也忙，则选择队列最短的出纳员排队。
    // 这样VIP客户倾向于集中在某些出纳员，而不是随机插队到任何一个普通出纳员的队列中。
    // 寻找队列最短的出纳员 (包括 VIP 窗口)
    minQueueCount = 999999;
    minFinishTime = 999999; // Added to break ties
    bestTellerID = -1;

    for (int i = 1; i <= s->numTellers; i++)
    {
      if (s->tstat[i].queueCount < minQueueCount)
      {
        minQueueCount = s->tstat[i].queueCount;
        bestTellerID = i;
        minFinishTime = s->tstat[i].finishService;
      }
      else if (s->tstat[i].queueCount == minQueueCount)
      {
        // 如果队列长度相同，选择最早空闲的出纳员
        if (s->tstat[i].finishService < minFinishTime)
        {
          minFinishTime = s->tstat[i].finishService;
          bestTellerID = i;
        }
      }
    }
    return bestTellerID; // 返回队列最短的出纳员ID
  }
  else // notVip (Ordinary Customer)
  {
    // 1. Look for any completely idle REGULAR teller (exclude Teller #1 if it's VIP_WINDOWS)
    for (int i = VIP_WINDOWS + 1; i <= s->numTellers; i++)
    {
      if (s->tstat[i].finishService <= currentTime && IsTellerQueueEmpty(&s->tstat[i]))
      {
        return i; // Found an idle regular teller, assign immediately
      }
    }

    // 2. If no idle regular tellers, find the regular teller with the shortest queue.
    // 普通客户不考虑VIP窗口，只在普通窗口中排队。
    bestTellerID = -1; // Reset for this specific search
    minQueueCount = 999999;
    minFinishTime = 999999;

    for (int i = VIP_WINDOWS + 1; i <= s->numTellers; i++)
    {
      if (s->tstat[i].queueCount < minQueueCount)
      {
        minQueueCount = s->tstat[i].queueCount;
        bestTellerID = i;
        minFinishTime = s->tstat[i].finishService;
      }
      else if (s->tstat[i].queueCount == minQueueCount)
      {
        if (s->tstat[i].finishService < minFinishTime)
        {
          minFinishTime = s->tstat[i].finishService;
          bestTellerID = i;
        }
      }
    }

    // Fallback: If no regular tellers were found (e.g., numTellers is 1 or less than VIP_WINDOWS+1)
    if (bestTellerID == -1)
    {
      printf("No regular tellers available, defaulting to Teller 1.\n");
      return 1;
    }
    return bestTellerID;
  }
}

// Runs the simulation
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

    if (GetTime(e) > s->simulationLength)
    {
      if (GetEventType(e) == arrival)
      {
        continue;
      }
    }

    if (GetEventType(e) == arrival)
    {
      printf("时间: %2d\t%s客户 %d 到达\n", GetTime(e), (GetCustomerType(e) == Vip) ? "VIP" : "普通", GetCustomerID(e));

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

      // DEBUG print: Show decision parameters
      printf("DEBUG: 客户 %d (VIP: %d) 到达时间 %d. 选择的出纳员: %d. 出纳员 %d 预计空闲时间: %d. 出纳员 %d 队列是否为空: %d. (当前队列长度: %d)\n",
             GetCustomerID(e), iv, GetTime(e), tellerID, tellerID, s->tstat[tellerID].finishService, tellerID, IsTellerQueueEmpty(&s->tstat[tellerID]), s->tstat[tellerID].queueCount);

      // Handle customer queuing and service
      if (s->tstat[tellerID].finishService <= GetTime(e) && IsTellerQueueEmpty(&s->tstat[tellerID]))
      {
        waittime = 0;

        s->tstat[tellerID].totalCustomerWait += waittime;
        s->tstat[tellerID].totalCustomerCount++;
        s->tstat[tellerID].totalService += servicetime;

        InitEvent(newevent, GetTime(e) + servicetime,
                  departure, GetCustomerID(e), tellerID,
                  waittime, servicetime, iv);
        PQInsert(&(s->pq), *newevent);
        s->tstat[tellerID].finishService = GetTime(e) + servicetime;

        if (s->tstat[tellerID].timelineCount < MaxPQSize)
        {
          s->tstat[tellerID].timeline[s->tstat[tellerID].timelineCount] = *newevent;
          s->tstat[tellerID].timelineCount++;
        }
        printf("\t出纳员 %d\t等待时间 %d\t服务时间 %d (立即服务)\n", tellerID, waittime, servicetime);
      }
      else
      {
        if (iv == Vip && tellerID > VIP_WINDOWS)
        {
          // VIP插队：仅当VIP排到普通窗口时计算插队时间
          // 为了利好银行，我们应该尽量避免这种插队，或者优化插队时的计算。
          // 这里保持原有的插队逻辑，但通过 NextAvailableTeller 引导 VIP 到队列最短的窗口。
          InsertVipIntoQueue(&s->tstat[tellerID], *e);
          Node *current = s->tstat[tellerID].customerQueueHead;
          int vip_inserted_service_time = Get_ServiceTime(s); // 预估VIP的服务时间
          while (current != NULL)
          {
            // 只有当VIP确实插到普通客户前面时，才计算插队时间
            if (GetCustomerType(&current->customerEvent) == notVip && GetCustomerID(&current->customerEvent) != GetCustomerID(e) /* 确保不是VIP自己 */)
            {
              s->totalCutInTime += vip_inserted_service_time; // 每次插队，增加普通客户的等待时间
            }
            current = current->next;
          }
          printf("\tVIP客户 %d 在出纳员 %d 排队 (插队). 队列头: %d, 队列尾: %d, 当前队列长度: %d\n",
                 GetCustomerID(e), tellerID,
                 (s->tstat[tellerID].customerQueueHead ? GetCustomerID(&s->tstat[tellerID].customerQueueHead->customerEvent) : 0),
                 (s->tstat[tellerID].customerQueueTail ? GetCustomerID(&s->tstat[tellerID].customerQueueTail->customerEvent) : 0),
                 s->tstat[tellerID].queueCount);
        }
        else
        {
          EnqueueCustomer(&s->tstat[tellerID], *e);
          printf("\t客户 %d 在出纳员 %d 排队 (正常入队). 队列头: %d, 队列尾: %d, 当前队列长度: %d\n",
                 GetCustomerID(e), tellerID,
                 (s->tstat[tellerID].customerQueueHead ? GetCustomerID(&s->tstat[tellerID].customerQueueHead->customerEvent) : 0),
                 (s->tstat[tellerID].customerQueueTail ? GetCustomerID(&s->tstat[tellerID].customerQueueTail->customerEvent) : 0),
                 s->tstat[tellerID].queueCount);
        }
      }
    }
    else // GetEventType(e) == departure
    {
      if (GetCustomerType(e) == Vip)
        s->totalVipCustomerCount++;
      else
        s->totalOrdinaryCustomerCount++;

      printf("时间: %2d\t%s客户 %d 离开\n", GetTime(e), (GetCustomerType(e) == Vip) ? "VIP" : "普通", GetCustomerID(e));
      printf("\t出纳员 %d\t等待时间 %d\t服务时间 %d\n", GetTellerID(e), GetWaitTime(e), GetServiceTime(e));
      tellerID = GetTellerID(e);

      printf("DEBUG: 出纳员 %d 离开时间 %d. 队列是否为空: %d. (当前队列长度: %d)\n", tellerID, GetTime(e), IsTellerQueueEmpty(&s->tstat[tellerID]), s->tstat[tellerID].queueCount);

      // Step 1: Recalculate waiting times for ALL VIPs in ALL queues and populate global vipPQueue
      InitPQueue(&(s->vipPQueue));

      for (int i = 1; i <= s->numTellers; i++)
      {
        Node *current = s->tstat[i].customerQueueHead;
        while (current != NULL)
        {
          Event *queuedEvent = &current->customerEvent;
          if (GetCustomerType(queuedEvent) == Vip)
          {
            int current_wait_time = GetTime(e) - GetTime(queuedEvent);
            if (current_wait_time >= s->waitHigh)
            {
              Event temp_vip_event;
              InitEvent(&temp_vip_event, -current_wait_time, departure,
                        GetCustomerID(queuedEvent), i,
                        current_wait_time, 0, Vip);

              PQInsert(&(s->vipPQueue), temp_vip_event);
            }
          }
          current = current->next;
        }
      }

      // Step 2: Decide next customer to serve based on VIP priority
      Event nextCustomerToServe;
      int originalTellerID_of_vip = -1;

      if (!PQEmpty(&(s->vipPQueue)))
      {
        // Serve highest priority VIP from global list (Requirement 2)
        nextCustomerToServe = PQDelete(&(s->vipPQueue));
        originalTellerID_of_vip = GetTellerID(&nextCustomerToServe);
        int vipCustomerID_to_remove = GetCustomerID(&nextCustomerToServe);

        // --- 利好银行修改: 减少插队造成的普通客户等待时间 ---
        // 只有当 VIP 确实是从当前出纳员的队列中被移除时，才调整 totalCutInTime。
        // 如果是从其他队列过来的，则不影响当前出纳员队列的 cut-in。
        // 并且，如果 VIP 换到空闲出纳员，也不会造成额外插队时间。
        if (originalTellerID_of_vip == tellerID)
        {
          Node *orig = s->tstat[originalTellerID_of_vip].customerQueueHead;
          int found_vip = 0;
          int vip_service_time = GetServiceTimeByCustomerID(&s->tstat[originalTellerID_of_vip], vipCustomerID_to_remove); // 客户实际服务时间
          while (orig != NULL)
          {
            if (!found_vip && GetCustomerID(&orig->customerEvent) == vipCustomerID_to_remove)
            {
              found_vip = 1; // 找到 VIP
            }
            else if (found_vip && GetCustomerType(&orig->customerEvent) == notVip)
            {
              // 这些普通客户原本要等待该 VIP 的服务时间，现在 VIP 换队，所以减少了这部分等待时间
              s->totalCutInTime -= vip_service_time;
            }
            orig = orig->next;
          }
        }
        RemoveCustomerByID(&s->tstat[originalTellerID_of_vip], vipCustomerID_to_remove);

        // 2. 计算新队列（当前tellerID）所有普通客户因VIP插队增加的等待时间
        // 只有当当前出纳员队列不为空且有普通客户时才增加
        Node *curr = s->tstat[tellerID].customerQueueHead;
        int new_vip_service_time = Get_ServiceTime(s); // 给这个VIP分配新的服务时间
        while (curr != NULL)
        {
          if (GetCustomerType(&curr->customerEvent) == notVip)
          {
            s->totalCutInTime += new_vip_service_time; // 增加普通客户的等待时间
          }
          curr = curr->next;
        }
        printf("DEBUG: VIP客户 %d (来自出纳员 %d 的队列) 等待时间 %d 达到最长等待时间 %d，优先服务。\n", vipCustomerID_to_remove, originalTellerID_of_vip, GetWaitTime(&nextCustomerToServe), s->waitHigh);

        waittime = GetWaitTime(&nextCustomerToServe);
        servicetime = Get_ServiceTime(s);

        s->tstat[tellerID].totalCustomerWait += waittime;
        s->tstat[tellerID].totalCustomerCount++;
        s->tstat[tellerID].totalService += servicetime;

        int nextCustomerDepartureTime = GetTime(e) + servicetime;
        s->tstat[tellerID].finishService = nextCustomerDepartureTime;

        InitEvent(newevent, nextCustomerDepartureTime, departure,
                  GetCustomerID(&nextCustomerToServe), tellerID,
                  waittime, servicetime, GetCustomerType(&nextCustomerToServe));
        PQInsert(&(s->pq), *newevent);

        if (s->tstat[tellerID].timelineCount < MaxPQSize)
        {
          s->tstat[tellerID].timeline[s->tstat[tellerID].timelineCount] = *newevent;
          s->tstat[tellerID].timelineCount++;
        }
        printf("\t出纳员 %d 开始服务排队的 %s客户 %d\n", tellerID, (GetCustomerType(&nextCustomerToServe) == Vip) ? "VIP" : "普通", GetCustomerID(&nextCustomerToServe));
      }
      else if (!IsTellerQueueEmpty(&s->tstat[tellerID]))
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

  if (GetTime(e) > s->simulationLength)
  {
    s->simulationLength = GetTime(e);
  }

  free(e);
  free(newevent);
}

// Prints the simulation results (unchanged in logic, but output format matters for Python)
void PrintSimulationResults(Simulation *s)
{
  int cumCustomers = 0, cumWait = 0, i, j;
  int avgCustWait;
  float tellerWork;
  int tellerWorkPercent;

  for (i = 1; i <= s->numTellers; i++)
  {
    cumCustomers += s->tstat[i].totalCustomerCount;
    cumWait += s->tstat[i].totalCustomerWait;
  }

  // Calculate total teller idle time
  // This is sum of (simulationLength - totalService) for each teller, which is total idle time for each teller, then summed up.
  // The average is totalTellerIdleTime / numTellers.
  s->totalTellerIdleTime = 0; // Reset before recalculating
  for (i = 1; i <= s->numTellers; i++)
  {
    s->totalTellerIdleTime += (s->simulationLength - s->tstat[i].totalService);
  }

  printf("\n");
  printf("******** 模拟结果总结 ********\n");
  printf("模拟时间：%d 分钟\n", s->simulationLength);
  printf("\t客户总数：%d\n", cumCustomers);
  printf("\t平均客户等待时间：");

  avgCustWait = (cumCustomers > 0) ? (int)((float)cumWait / cumCustomers + 0.5) : 0;
  printf("%d 分钟\n", avgCustWait);

  for (i = 1; i <= s->numTellers; i++)
  {
    printf("\t出纳员 #%d\t工作百分比 ", i);
    tellerWork = (s->simulationLength > 0) ? (float)(s->tstat[i].totalService) / s->simulationLength : 0.0;
    tellerWorkPercent = (int)(tellerWork * 100.0 + 0.5);
    printf("%d%%\n", tellerWorkPercent);

    printf("\t出纳员 #%d 时间线：\n", i);
    for (j = 0; j < s->tstat[i].timelineCount; j++)
    {
      Event *te = &s->tstat[i].timeline[j];
      int iv = GetCustomerType(te);
      int waitTime = GetWaitTime(te);
      if (iv == Vip)
      {
        s->totalVipWaitTime += waitTime;
      }
      printf("\t\t时间 %2d：%s客户 %d，服务时间 %d，等待时间 %d\n",
             GetTime(te), (iv == Vip) ? "VIP" : "普通",
             GetCustomerID(te), GetServiceTime(te), waitTime);
    }
  }
  printf("\n");
  // These are the lines that analysis.py expects
  printf("VIP客户平均等待时间：%f 分钟\n", (s->totalVipCustomerCount > 0) ? (float)s->totalVipWaitTime / s->totalVipCustomerCount : 0.0);
  printf("出纳员平均空闲时间：%f 分钟\n", (s->numTellers > 0) ? (float)s->totalTellerIdleTime / s->numTellers : 0.0);
  printf("普通用户被插队所平均多出的等待时间：%f 分钟\n", (s->totalOrdinaryCustomerCount > 0) ? (float)s->totalCutInTime / s->totalOrdinaryCustomerCount : 0.0);
}

// Helper functions for the new teller queue (linked list)
void EnqueueCustomer(TellerStats *ts, Event customerEvent)
{
  Node *newNode = (Node *)malloc(sizeof(Node));
  if (newNode == NULL)
  {
    fprintf(stderr, "Error: Memory allocation failed for new node.\n");
    return;
  }
  newNode->customerEvent = customerEvent;
  newNode->next = NULL;

  if (ts->customerQueueTail == NULL) // Queue is empty
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
    fprintf(stderr, "Error: Dequeue from empty teller queue.\n");
    Event emptyEvent;
    InitEvent(&emptyEvent, -1, arrival, -1, -1, -1, -1, notVip); // Indicate an error
    return emptyEvent;
  }

  Node *temp = ts->customerQueueHead;
  Event customer = temp->customerEvent;
  ts->customerQueueHead = ts->customerQueueHead->next;

  if (ts->customerQueueHead == NULL) // Last element dequeued
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
    fprintf(stderr, "Error: Peek from empty teller queue.\n");
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

// New function: Inserts a VIP customer into the queue according to priority rules
// VIP插队时直接插到队头，从而避免移动原先的一个到队尾。若再有vip插队则插到前一个vip之后
void InsertVipIntoQueue(TellerStats *ts, Event vipEvent)
{
  Node *newNode = (Node *)malloc(sizeof(Node));
  if (newNode == NULL)
  {
    fprintf(stderr, "Error: Memory allocation failed for VIP new node.\n");
    return;
  }
  newNode->customerEvent = vipEvent;
  newNode->next = NULL;

  if (IsTellerQueueEmpty(ts))
  {
    // If the queue is empty, VIP is the first.
    ts->customerQueueHead = newNode;
    ts->customerQueueTail = newNode;
  }
  else
  {
    Node *current = ts->customerQueueHead;
    Node *prev = NULL;

    // Find the last VIP in the queue, or the first non-VIP.
    // New VIP should be inserted AFTER existing VIPs at the front.
    while (current != NULL && GetCustomerType(&current->customerEvent) == Vip)
    {
      prev = current;
      current = current->next;
    }

    if (prev == NULL) // No existing VIPs at the front, insert at head (before first non-VIP or if queue was all non-VIPs)
    {
      newNode->next = ts->customerQueueHead;
      ts->customerQueueHead = newNode;
    }
    else // Insert after the last VIP (prev points to the last VIP found)
    {
      newNode->next = prev->next;
      prev->next = newNode;
      if (newNode->next == NULL) // If inserted at the very end
      {
        ts->customerQueueTail = newNode;
      }
    }
  }
  ts->queueCount++;
}

// NEW: Function to remove a customer by ID from a teller's queue
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
    // Customer not found in this queue, nothing to do
    return;
  }

  if (prev == NULL)
  {
    // Removing the head node
    ts->customerQueueHead = current->next;
  }
  else
  {
    // Removing a middle or tail node
    prev->next = current->next;
  }

  if (current == ts->customerQueueTail)
  {
    // If the removed node was the tail, update tail
    ts->customerQueueTail = prev;
  }

  free(current);
  ts->queueCount--;
  // If queue becomes empty after removal, ensure tail is NULL
  if (ts->customerQueueHead == NULL)
  {
    ts->customerQueueTail = NULL;
  }
}

// 根据customerID查找其服务时间
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
  return 0; // 未找到，返回0
}

#endif /* SIMULATION */