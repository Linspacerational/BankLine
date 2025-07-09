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
  Node *customerQueueHead; // Head of the teller's customer queue
  Node *customerQueueTail; // Tail of the teller's customer queue
  int queueCount;          // Current number of customers in the queue
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
  isVip ivs[10]; // Predefined VIP statuses (can be removed if VIP status is no longer relevant for logic)
  int ivsIndex;
};
typedef struct simulation Simulation;

// Function prototypes
int NextArrivalTime(Simulation *s);
int Get_ServiceTime(Simulation *);
int NextAvailableTeller(Simulation *s, isVip iv, int currentTime); // 'iv' parameter can be removed
void InitSimulation(Simulation *s);
void RunSimulation(Simulation *s);
void PrintSimulationResults(Simulation *s);
isVip GenerateRandomVipStatus(void); // Can be removed or simplified

// Helper functions for the new teller queue (linked list)
void EnqueueCustomer(TellerStats *ts, Event customerEvent);
Event DequeueCustomer(TellerStats *ts);
Event PeekCustomer(TellerStats *ts);
int IsTellerQueueEmpty(TellerStats *ts);
// void InsertVipIntoQueue(TellerStats *ts, Event vipEvent); // REMOVED: No longer needed

// Generates a random VIP status (1 in 5 chance is VIP) - Can be simplified to always notVip or removed
isVip GenerateRandomVipStatus(void)
{
  return (rand() % 5 == 0) ? Vip : notVip; // Kept for now, but VIP logic is ignored in queuing
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
  }
  s->nextCustomer = 1; // First customer ID
  s->ivsIndex = 0;     // Index for predefined VIP statuses

  // Predefined VIP statuses for initial customers - Can be removed if VIP status is no longer relevant
  isVip temp[10] = {notVip, notVip, Vip, Vip, Vip, notVip, Vip, notVip, notVip, Vip}; // Updated VIP sequence
  for (i = 0; i < 10; i++)
  {
    s->ivs[i] = temp[i];
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

  // Initialize and insert the first arrival event into the priority queue
  InitEvent(firstevent, 0, arrival, 1, 0, 0, 0, s->ivs[s->ivsIndex++]); // Keep VIP status for event, but it's ignored in queuing
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

// Determines the next available teller based on shortest queue and earliest finish time
// The 'iv' parameter is now redundant for decision making in this function
int NextAvailableTeller(Simulation *s, isVip iv, int currentTime)
{
  int bestTellerID = -1;

  // Find the teller with the shortest queue.
  // Tie-break by earliest finish service time.
  if(iv == Vip){
    for (int i = 1; i <= s->numTellers; i++) // Iterate through all tellers
    {
      // Prioritize idle tellers (finishService <= currentTime and empty queue)
      if (s->tstat[i].finishService <= currentTime)
      {
        return i; // Found an idle teller, assign immediately
      }
    }
    return 1;
  }
  else{
    for (int i = 2; i <= s->numTellers; i++) // Iterate through all tellers
    {
      if (s->tstat[i].finishService <= currentTime)
      {
        // If this teller is idle and has an empty queue, return them immediately
        return i;
      }
    }
    return rand() % (s->numTellers - 1) + 2; // Randomly select a teller if all are busy
  }
  return bestTellerID;
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
  isVip iv; // Keep for event creation, but not for queuing logic

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
      if (nexttime <= s->simulationLength && s->ivsIndex < 10) // ivsIndex limit can be removed if VIP predefinition is gone
      {
        s->nextCustomer++;
        InitEvent(newevent, nexttime, arrival, s->nextCustomer, 0, 0, 0, s->ivs[s->ivsIndex++]); // Keep VIP for stats, but not logic
        PQInsert(&(s->pq), *newevent);
      }

      servicetime = Get_ServiceTime(s);
      iv = GetCustomerType(e);                           // Still get VIP status, but it's not used for queuing
      tellerID = NextAvailableTeller(s, iv, GetTime(e)); // iv is ignored in NextAvailableTeller

      // DEBUG print: Show decision parameters
      printf("DEBUG: 客户 %d (VIP: %d) 到达时间 %d. 选择的出纳员: %d. 出纳员 %d 预计空闲时间: %d. 出纳员 %d 队列是否为空: %d. (当前队列长度: %d)\n",
             GetCustomerID(e), iv, GetTime(e), tellerID, tellerID, s->tstat[tellerID].finishService, tellerID, IsTellerQueueEmpty(&s->tstat[tellerID]), s->tstat[tellerID].queueCount);

      // Handle customer queuing and service
      // If the teller is currently free (finishService is in the past) and their queue is empty
      if (s->tstat[tellerID].finishService <= GetTime(e) && IsTellerQueueEmpty(&s->tstat[tellerID]))
      {
        waittime = 0; // Immediate service, no wait

        s->tstat[tellerID].totalCustomerWait += waittime;
        s->tstat[tellerID].totalCustomerCount++;
        s->tstat[tellerID].totalService += servicetime;

        // Schedule departure for this immediately served customer
        InitEvent(newevent, GetTime(e) + servicetime,
                  departure, GetCustomerID(e), tellerID,
                  waittime, servicetime, iv);
        PQInsert(&(s->pq), *newevent);
        s->tstat[tellerID].finishService = GetTime(e) + servicetime; // Update teller's next available time

        if (s->tstat[tellerID].timelineCount < MaxPQSize)
        {
          s->tstat[tellerID].timeline[s->tstat[tellerID].timelineCount] = *newevent;
          s->tstat[tellerID].timelineCount++;
        }
        printf("\t出纳员 %d\t等待时间 %d\t服务时间 %d (立即服务)\n", tellerID, waittime, servicetime);
      }
      else // Teller is busy or has a queue, customer needs to wait
      {
        // Always enqueue normally, no special VIP handling
        EnqueueCustomer(&s->tstat[tellerID], *e);
        printf("\t客户 %d 在出纳员 %d 排队 (正常入队). 队列头: %d, 队列尾: %d, 当前队列长度: %d\n",
               GetCustomerID(e), tellerID,
               (s->tstat[tellerID].customerQueueHead ? GetCustomerID(&s->tstat[tellerID].customerQueueHead->customerEvent) : 0),
               (s->tstat[tellerID].customerQueueTail ? GetCustomerID(&s->tstat[tellerID].customerQueueTail->customerEvent) : 0),
               s->tstat[tellerID].queueCount);
      }
    }
    else // GetEventType(e) == departure
    {
      printf("时间: %2d\t%s客户 %d 离开\n", GetTime(e), (GetCustomerType(e) == Vip) ? "VIP" : "普通", GetCustomerID(e));
      printf("\t出纳员 %d\t等待时间 %d\t服务时间 %d\n", GetTellerID(e), GetWaitTime(e), GetServiceTime(e));
      tellerID = GetTellerID(e);

      // DEBUG print: Check queue status when a teller becomes free
      printf("DEBUG: 出纳员 %d 离开时间 %d. 队列是否为空: %d. (当前队列长度: %d)\n", tellerID, GetTime(e), IsTellerQueueEmpty(&s->tstat[tellerID]), s->tstat[tellerID].queueCount);

      // Teller is now free (at GetTime(e)). Check if there are customers waiting in this teller's queue
      if (!IsTellerQueueEmpty(&s->tstat[tellerID]))
      {
        printf("DEBUG: 出纳员 %d 队列不为空。处理下一个客户。\n", tellerID);
        Event nextCustomerInLine = DequeueCustomer(&s->tstat[tellerID]);
        int serviceTimeForNext = Get_ServiceTime(s);

        // NEW DEBUG PRINT: After dequeuing, show the immediate queue state
        printf("DEBUG: Dequeued C%d from T%d. Post-dequeue Queue: Head:%d, Tail:%d, Count:%d\n",
               GetCustomerID(&nextCustomerInLine), tellerID,
               (s->tstat[tellerID].customerQueueHead ? GetCustomerID(&s->tstat[tellerID].customerQueueHead->customerEvent) : 0),
               (s->tstat[tellerID].customerQueueTail ? GetCustomerID(&s->tstat[tellerID].customerQueueTail->customerEvent) : 0),
               s->tstat[tellerID].queueCount);

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
        printf("DEBUG: 出纳员 %d 队列为空。出纳员变为空闲。\n", tellerID);
        s->tstat[tellerID].finishService = GetTime(e); // Teller is free from this time
      }
    }
  }

  // Ensure simulationLength reflects the actual last event time if it extends beyond initial.
  if (GetTime(e) > s->simulationLength)
  {
    s->simulationLength = GetTime(e);
  }

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
    // Ensure simulationLength is positive to avoid division by zero
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


#endif /* SIMULATION */