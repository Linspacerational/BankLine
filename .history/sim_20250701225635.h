#ifndef SIMULATION
#define SIMULATION

#include <stdio.h>
#include <stdlib.h>
#include "event.h"
typedef Event DataType;

#include "apqueue.h"

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
  struct event timeline[MaxPQSize];
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
  TellerStats tstat[11];
  PQueue pq;
  isVip ivs[10]; // Predefined VIP statuses
  int ivsIndex;
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

// Helper functions for the new teller queue (linked list)
void EnqueueCustomer(TellerStats *ts, Event customerEvent);
Event DequeueCustomer(TellerStats *ts);
Event PeekCustomer(TellerStats *ts);
int IsTellerQueueEmpty(TellerStats *ts);
void InsertVipIntoQueue(TellerStats *ts, Event vipEvent); // New function for VIP insertion

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
  for (i = 1; i <= 10; i++)
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

  // Predefined VIP statuses for initial customers
  isVip temp[10] = {Vip, notVip, Vip, Vip, Vip, notVip, Vip, notVip, notVip, Vip}; // Updated VIP sequence
  for (i = 0; i < 10; i++)
  {
    s->ivs[i] = temp[i];
  }

  // Prompt user for simulation parameters
  printf("输入模拟时间（分钟）：");
  scanf("%d", &s->simulationLength);
  printf("输入出纳员数量：");
  scanf("%d", &s->numTellers);
  printf("输入到达时间范围（分钟）：");
  scanf("%d%d", &s->arrivalLow, &s->arrivalHigh);
  printf("输入服务时间范围（分钟）：");
  scanf("%d%d", &s->serviceLow, &s->serviceHigh);

  // Initialize and insert the first arrival event into the priority queue
  InitEvent(firstevent, 0, arrival, 1, 0, 0, 0, s->ivs[s->ivsIndex++]);
  InitPQueue(&(s->pq));
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
  int i;
  int bestTellerID = -1;
  int minFinishTime = 999999; // A very large number to represent infinity

  // --- VIP Customer Logic ---
  if (iv == Vip)
  {
    // 优先级 1: 空闲的 VIP 出纳员 (Teller #1)
    if (s->tstat[1].finishService <= currentTime && IsTellerQueueEmpty(&s->tstat[1]))
    {
      return 1;
    }

    // 优先级 2: 空闲的普通出纳员
    for (i = 2; i <= s->numTellers; i++)
    {
      if (s->tstat[i].finishService <= currentTime && IsTellerQueueEmpty(&s->tstat[i]))
      {
        return i; // Found an empty regular teller
      }
    }

    // 优先级 3: 非空闲 VIP 出纳员 (Teller #1)
    // 如果 VIP 出纳员忙碌，VIP 客户可以排队
    if (s->tstat[1].finishService > currentTime || !IsTellerQueueEmpty(&s->tstat[1]))
    {
      return 1;
    }

    // 优先级 4: 插队到普通出纳员队列 (即排到普通窗口队列最前面)
    // 寻找一个非空闲的普通出纳员，以便 VIP 可以插队
    for (i = 2; i <= s->numTellers; i++)
    {
      if (!IsTellerQueueEmpty(&s->tstat[i]))
      {
        // VIP 客户可以在普通窗口队列中插队（即排到普通窗口队列最前面）。
        // 这里不检查队头是否是VIP，因为规则是“插队到普通窗口队列最前面”，
        // 后续VIP再插队则插到前一个VIP之后。这个逻辑由 InsertVipIntoQueue 负责。
        return i; // 这个出纳员是插队的候选人
      }
    }

    // 如果所有 VIP 窗口和普通窗口都忙碌，且无法插队（即所有普通窗口队列里下一个都是 VIP 或者都空）
    // 则选择非空普通窗口中最快可用的一个排队
    for (i = 2; i <= s->numTellers; i++)
    {
      // 找出当前 finishService 最早的普通出纳员
      if (s->tstat[i].finishService < minFinishTime)
      {
        minFinishTime = s->tstat[i].finishService;
        bestTellerID = i;
      }
    }
    // 如果找到了最佳普通出纳员
    if (bestTellerID != -1)
    {
      return bestTellerID;
    }
  }
  // --- Non-VIP Customer Logic ---
  else // if (iv == notVip)
  {
    // 查找空闲的普通出纳员
    for (i = 2; i <= s->numTellers; i++)
    {
      if (s->tstat[i].finishService <= currentTime && IsTellerQueueEmpty(&s->tstat[i]))
      {
        return i; // Found an empty regular teller
      }
    }

    // 如果没有空闲的普通出纳员，则查找最快可用的普通出纳员（最短队列或最早 finishService）
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

  // Fallback: This should ideally not be reached if logic is complete
  // If no teller found based on rules, default to teller 1 or a random one.
  // For safety, return teller 1 (VIP teller) if all else fails.
  // This implies the customer will eventually queue there if no other options.
  return 1;
}

// Runs the simulation
void RunSimulation(Simulation *s)
{
  Event *e = (Event *)malloc(sizeof(Event));        // Current event
  Event *newevent = (Event *)malloc(sizeof(Event)); // New event to be scheduled
  int nexttime;
  int tellerID;
  int servicetime;
  int waittime;
  isVip iv;

  while (!PQEmpty(&(s->pq)))
  {
    *e = PQDelete(&(s->pq)); // Get the next event from the priority queue

    // If the event time exceeds simulation length, consider if it's an arrival that shouldn't be processed
    if (GetTime(e) > s->simulationLength)
    {
      if (GetEventType(e) == arrival)
      {
        // If an arrival event happens after simulationLength, we don't process it further.
        // This prevents new customers from entering the system after the simulation nominally ends.
        continue;
      }
      // Departure events that happen after simulationLength must still be processed
      // to correctly calculate teller stats and clear queues.
    }

    if (GetEventType(e) == arrival)
    {
      printf("时间: %2d\t%s客户 %d 到达\n", GetTime(e), (GetCustomerType(e) == Vip) ? "VIP" : "普通", GetCustomerID(e));

      // Schedule the next arrival if within simulation limits and predefined VIP list limits
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

      // Handle customer queuing and service
      // If the teller is currently free (finishService is in the past) and their queue is empty
      if (s->tstat[tellerID].finishService <= GetTime(e) && IsTellerQueueEmpty(&s->tstat[tellerID]))
      {
        waittime = 0;                                  // Immediate service, no wait
        s->tstat[tellerID].finishService = GetTime(e); // Teller starts service immediately

        s->tstat[tellerID].totalCustomerWait += waittime;
        s->tstat[tellerID].totalCustomerCount++;
        s->tstat[tellerID].totalService += servicetime;

        // Schedule departure for this immediately served customer
        InitEvent(newevent, GetTime(e) + servicetime,
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
      else // Teller is busy or has a queue, customer needs to wait
      {
        if (iv == Vip && tellerID != 1)
        { // VIP trying to potentially preempt in a regular teller queue
          // VIP 用户选择窗口的优先级为：空 VIP 窗口 > 空普通窗口 > 插队到普通窗口队列。
          // InsertVipIntoQueue handles the logic for placing VIPs after existing VIPs in the queue.
          InsertVipIntoQueue(&s->tstat[tellerID], *e);
          printf("\tVIP客户 %d 在普通出纳员 %d 插队\n", GetCustomerID(e), tellerID);
        }
        else
        {
          // Regular customer or VIP goes to VIP teller, just enqueue at the end
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

      // Teller is now free (at GetTime(e)). Check if there are customers waiting in this teller's queue
      if (!IsTellerQueueEmpty(&s->tstat[tellerID]))
      {
        Event nextCustomerInLine = DequeueCustomer(&s->tstat[tellerID]);
        int serviceTimeForNext = Get_ServiceTime(s);

        // Calculate wait time for the next customer:
        // Wait time = (Teller becomes free at this departure event's time) - (Next customer's arrival time)
        int waitTimeForNext = GetTime(e) - GetTime(&nextCustomerInLine);
        if (waitTimeForNext < 0)
          waitTimeForNext = 0; // Should not be negative

        s->tstat[tellerID].totalCustomerWait += waitTimeForNext;
        s->tstat[tellerID].totalCustomerCount++; // Count customer when they START service
        s->tstat[tellerID].totalService += serviceTimeForNext;

        // Schedule departure for the next customer in line
        int nextCustomerDepartureTime = GetTime(e) + serviceTimeForNext;
        s->tstat[tellerID].finishService = nextCustomerDepartureTime; // Update teller's next available time

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
        // Queue is empty, teller becomes idle
        s->tstat[tellerID].finishService = GetTime(e); // Teller is free from this time
      }
    }
  }

  // Ensure simulationLength reflects the actual last event time if it extends beyond initial.
  s->simulationLength = (GetTime(e) <= s->simulationLength) ? s->simulationLength : GetTime(e);

  free(e);
  free(newevent);
}

// Prints the simulation results
void PrintSimulationResults(Simulation *s)
{
  int cumCustomers = 0, cumWait = 0, i, j;
  int avgCustWait;
  float tellerWork;
  int tellerWorkPercent;

  // Calculate cumulative statistics
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

  // Calculate average customer wait time
  avgCustWait = (cumCustomers > 0) ? (int)((float)cumWait / cumCustomers + 0.5) : 0;
  printf("%d 分钟\n", avgCustWait);

  // Print teller-specific statistics and timelines
  for (i = 1; i <= s->numTellers; i++)
  {
    printf("\t出纳员 #%d\t工作百分比 ", i);
    tellerWork = (s->simulationLength > 0) ? (float)(s->tstat[i].totalService) / s->simulationLength : 0.0;
    tellerWorkPercent = (int)(tellerWork * 100.0 + 0.5);
    printf("%d%%\n", tellerWorkPercent);

    printf("\t出纳员 #%d 时间线：\n", i);
    // Iterate through the timeline of events for each teller
    for (j = 0; j < s->tstat[i].timelineCount; j++)
    {
      Event *te = &s->tstat[i].timeline[j];
      printf("\t\t时间 %2d：%s客户 %d，服务时间 %d，等待时间 %d\n",
             GetTime(te), (GetCustomerType(te) == Vip) ? "VIP" : "普通",
             GetCustomerID(te), GetServiceTime(te), GetWaitTime(te));
    }
  }
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

  // Rule: VIP 用户可以在 VIP 窗口排队等候，或者在普通窗口队列中插队（即排到普通窗口队列最前面）。
  // Rule: 插队的 VIP 用户在普通窗口中按照到达顺序先后进行排队。
  // This means if there are already VIPs at the front of the queue, the new VIP goes after them.
  // Otherwise, it goes to the very front.

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
    int inserted = 0;

    // Find the last VIP in the queue, or the first non-VIP
    while (current != NULL && GetCustomerType(&current->customerEvent) == Vip)
    {
      prev = current;
      current = current->next;
    }

    if (prev == NULL) // No existing VIPs at the front, insert at head
    {
      newNode->next = ts->customerQueueHead;
      ts->customerQueueHead = newNode;
    }
    else // Insert after the last VIP (prev points to the last VIP)
    {
      newNode->next = prev->next;
      prev->next = newNode;
      if (newNode->next == NULL) // If inserted at the end
      {
        ts->customerQueueTail = newNode;
      }
    }
  }
  ts->queueCount++;
}

#endif /* SIMULATION */