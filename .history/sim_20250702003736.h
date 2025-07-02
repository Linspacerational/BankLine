#ifndef SIMULATION
#define SIMULATION

#include <stdio.h>
#include <stdlib.h>
#include "event.h"      // Assumed to contain Event structure and InitEvent, GetTime, GetEventType, etc.
typedef Event DataType; // PQueue uses Event as its DataType

#include "apqueue.h" // Assumed to contain PQueue structure and InitPQueue, PQInsert, PQDelete, PQEmpty

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
  TellerStats tstat[11];
  PQueue pq;        // Main event priority queue
  PQueue vipPQueue; // NEW: Global priority queue for high-priority VIPs
  isVip ivs[10];    // Predefined VIP statuses
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
    for (int i = 1; i <= s->numTellers; i++)
    {
      if (s->tstat[i].finishService <= currentTime && IsTellerQueueEmpty(&s->tstat[i]))
      {
        return i; // Found an idle teller, assign immediately
      }
    }

    // If no idle tellers, prioritize based on user's rule:
    // "非空 VIP 窗口 > 非空普通窗口 (随机选择)"
    // Teller 1 is implicitly the "VIP window" in original rule.
    // If Teller 1 is available (i.e., not empty, but taking VIPs)
    if ((!IsTellerQueueEmpty(&s->tstat[1]) || s->tstat[1].finishService > currentTime))
    {
      // If teller 1 is busy but has a queue or is serving, consider it.
      // This is a simplified logic as it doesn't consider other VIPs in line at T1 specifically.
      // The overall VIP priority based on waitHigh handles global priority.
      // So here, it's about initial assignment.
      return 1;
    }

    // Try to find busy regular tellers that a VIP could join (randomly)
    int busyRegularTellers[10]; // To store IDs of busy regular tellers (tellers 2 to numTellers)
    int numBusyRegularTellers = 0;

    for (int i = 2; i <= s->numTellers; i++)
    {
      // A teller is considered "non-empty" if it's currently serving or has a queue.
      // Since we already checked for idle tellers, any remaining teller is "non-empty".
      // This applies to any teller other than the main VIP one.
      if (!IsTellerQueueEmpty(&s->tstat[i]) || s->tstat[i].finishService > currentTime)
      {
        busyRegularTellers[numBusyRegularTellers++] = i;
      }
    }

    if (numBusyRegularTellers > 0)
    {
      // Randomly select one busy regular teller
      return busyRegularTellers[rand() % numBusyRegularTellers];
    }
    else
    {
      // Fallback: If no other suitable tellers found, and Teller 1 wasn't returned,
      // it means all tellers are either idle (already picked), or just Teller 1 exists and it's busy.
      // If numTellers is 1, it must be Teller 1.
      // This part ensures a VIP always gets assigned, even to a busy Teller 1.
      return 1;
    }
  }
  else // notVip (Ordinary Customer)
  {
    // 1. Look for any completely idle REGULAR teller (exclude Teller #1)
    for (int i = 2; i <= s->numTellers; i++)
    {
      if (s->tstat[i].finishService <= currentTime && IsTellerQueueEmpty(&s->tstat[i]))
      {
        return i; // Found an idle regular teller, assign immediately
      }
    }

    // 2. If no idle regular tellers, find the regular teller with the shortest queue.
    // Tie-break by earliest finish service time.
    bestTellerID = -1; // Reset for this specific search
    minQueueCount = 999999;
    minFinishTime = 999999;

    for (int i = 2; i <= s->numTellers; i++)
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

    // Fallback: If no regular tellers were found (e.g., only Teller 1 exists, or numTellers is too small for >=2)
    if (bestTellerID == -1)
    {
      // This case implies that numTellers might be 1, or something is wrong.
      // If the system is set up for non-VIPs to *only* go to regular tellers,
      // this might mean the customer cannot be served.
      // Assuming numTellers >= 2 for non-VIPs to have regular tellers.
      if (s->numTellers >= 2)
        return 2; // Default to teller 2 if it exists
      else
        return 1; // If only one teller, it must be teller 1
    }
    return bestTellerID;
  }
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
        if (iv == Vip && tellerID != 1)
        { // VIP assigned to a regular teller
          InsertVipIntoQueue(&s->tstat[tellerID], *e);
          printf("\tVIP客户 %d 在出纳员 %d 排队 (插队). 队列头: %d, 队列尾: %d, 当前队列长度: %d\n",
                 GetCustomerID(e), tellerID,
                 (s->tstat[tellerID].customerQueueHead ? GetCustomerID(&s->tstat[tellerID].customerQueueHead->customerEvent) : 0),
                 (s->tstat[tellerID].customerQueueTail ? GetCustomerID(&s->tstat[tellerID].customerQueueTail->customerEvent) : 0),
                 s->tstat[tellerID].queueCount);
        }
        else
        {
          // Normal enqueue (for non-VIPs, or VIPs going to VIP teller)
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
      printf("时间: %2d\t%s客户 %d 离开\n", GetTime(e), (GetCustomerType(e) == Vip) ? "VIP" : "普通", GetCustomerID(e));
      printf("\t出纳员 %d\t等待时间 %d\t服务时间 %d\n", GetTellerID(e), GetWaitTime(e), GetServiceTime(e));
      tellerID = GetTellerID(e);

      printf("DEBUG: 出纳员 %d 离开时间 %d. 队列是否为空: %d. (当前队列长度: %d)\n", tellerID, GetTime(e), IsTellerQueueEmpty(&s->tstat[tellerID]), s->tstat[tellerID].queueCount);

      // --- NEW LOGIC FOR VIP WAITING POLICY (Requirements 1 & 2) ---
      // Step 1: Recalculate waiting times for ALL VIPs in ALL queues and populate global vipPQueue
      // Clear the vipPQueue before repopulating to ensure only current high-priority VIPs are considered.
      // This is a simple approach assuming PQEmpty and PQDelete don't free memory of events.
      // If PQDelete frees, we'd need a way to clear without freeing, or re-Init.
      InitPQueue(&(s->vipPQueue)); // Re-initialize to effectively clear it.

      for (int i = 1; i <= s->numTellers; i++)
      {
        Node *current = s->tstat[i].customerQueueHead;
        while (current != NULL)
        {
          Event *queuedEvent = &current->customerEvent;
          if (GetCustomerType(queuedEvent) == Vip)
          {
            int current_wait_time = GetTime(e) - GetTime(queuedEvent); // GetTime(e) is current simulation time
            if (current_wait_time >= s->waitHigh)
            {
              // Create a temporary event to put into vipPQueue
              // Use 'time' field for priority (negative wait time for max-heap, longer wait = higher priority)
              // Use 'tellerID' field to store original teller ID from which the VIP is waiting
              // Use 'waitTime' field to store the actual current wait time for printing later
              Event temp_vip_event;
              InitEvent(&temp_vip_event, -current_wait_time, departure, // type is arbitrary
                        GetCustomerID(queuedEvent), i,                  // Store original teller ID
                        current_wait_time, 0, Vip);                     // Store actual wait time in waitTime field

              PQInsert(&(s->vipPQueue), temp_vip_event);
            }
          }
          current = current->next;
        }
      }

      // Step 2: Decide next customer to serve based on VIP priority
      Event nextCustomerToServe;
      int originalTellerID_of_vip = -1; // To store the teller from which the VIP is removed

      if (!PQEmpty(&(s->vipPQueue)))
      {
        // Serve highest priority VIP from global list (Requirement 2)
        nextCustomerToServe = PQDelete(&(s->vipPQueue));
        originalTellerID_of_vip = GetTellerID(&nextCustomerToServe); // Retrieve original teller ID
        int vipCustomerID_to_remove = GetCustomerID(&nextCustomerToServe);

        // Remove the VIP from their original teller's queue (Requirement 2: "可以更换柜台服务")
        RemoveCustomerByID(&s->tstat[originalTellerID_of_vip], vipCustomerID_to_remove);
        printf("DEBUG: VIP客户 %d (来自出纳员 %d 的队列) 等待时间 %d 达到最长等待时间 %d，优先服务。\n", vipCustomerID_to_remove, originalTellerID_of_vip, GetWaitTime(&nextCustomerToServe), s->waitHigh);

        // Assign this VIP to be served by the current tellerID
        waittime = GetWaitTime(&nextCustomerToServe); // This is the total wait time for the VIP
        servicetime = Get_ServiceTime(s);             // Generate new service time for this customer

        s->tstat[tellerID].totalCustomerWait += waittime;
        s->tstat[tellerID].totalCustomerCount++;
        s->tstat[tellerID].totalService += servicetime;

        // Schedule departure for this VIP customer by the current teller
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
        // No high-priority VIPs globally, serve from this teller's own queue (original logic)
        printf("DEBUG: 出纳员 %d 队列不为空。处理下一个客户。\n", tellerID);
        Event nextCustomerInLine = DequeueCustomer(&s->tstat[tellerID]); // This is the original logic
        int serviceTimeForNext = Get_ServiceTime(s);

        // DEBUG print: After dequeuing, show the immediate queue state
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

#endif /* SIMULATION */