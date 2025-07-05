#ifndef SIMULATION
#define SIMULATION

#include <stdio.h>
#include <stdlib.h>
#include "event.h"      // Assumed to contain Event structure and InitEvent, GetTime, GetEventType, etc.
typedef Event DataType; // PQueue uses Event as its DataType

#include "apqueue.h" // Assumed to contain PQueue structure and InitPQueue, PQInsert, PQDelete, PQEmpty

#define MAXCUSTLENGTH 1000
#define MAXTELLERLENGTH 11

int VIP_WINDOWS = 1; // Assuming Teller 1 is the VIP window

// Define a Node structure for the linked list (for teller-specific queues)
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

  // NEW: To track who is currently being served for interruption logic
  Event currentServingCustomerData; // Stores the event data of the currently served customer
  int isServingCustomer;            // 1 if serving, 0 if not
  int currentServingStartTime;      // When service for current customer began
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
  PQueue pq;                // Main event priority queue
  PQueue vipPQueue;         // Global priority queue for high-priority VIPs (for waitHigh logic)
  isVip ivs[MAXCUSTLENGTH]; // Predefined VIP statuses for customers
  int ivsIndex;

  // Metrics for performance evaluation
  int totalCutInTime;             // Total time ordinary customers were interrupted (service time they lost)
  int totalVipWaitTime;           // Total waiting time for all VIP customers
  int totalTellerIdleTime;        // Total idle time across all tellers
  int totalVipCustomerCount;      // Total number of VIP customers served
  int totalOrdinaryCustomerCount; // Total number of ordinary customers served
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
int GetServiceTimeByCustomerID(TellerStats *ts, int customerID); // Get service time by customer ID

// Helper functions for the new teller queue (linked list)
void EnqueueCustomer(TellerStats *ts, Event customerEvent);
Event DequeueCustomer(TellerStats *ts);
Event PeekCustomer(TellerStats *ts);
int IsTellerQueueEmpty(TellerStats *ts);
void InsertVipIntoQueue(TellerStats *ts, Event vipEvent);          // Function for VIP insertion (front of VIPs)
void PrependCustomerToQueue(TellerStats *ts, Event customerEvent); // NEW: Inserts customer at the absolute head of the queue
void RemoveCustomerByID(TellerStats *ts, int customerID);          // For VIP changing queues

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
    s->tstat[i].isServingCustomer = 0; // Initialize no customer being served
    s->tstat[i].currentServingStartTime = 0;
  }
  s->nextCustomer = 1; // First customer ID
  s->ivsIndex = 0;     // Index for predefined VIP statuses

  s->totalCutInTime = 0;
  s->totalVipWaitTime = 0;
  s->totalTellerIdleTime = 0;

  s->totalVipCustomerCount = 0;
  s->totalOrdinaryCustomerCount = 0;

  // Predefine VIP statuses for initial customers or generate randomly
  for (i = 0; i < MAXCUSTLENGTH; i++)
  {
    s->ivs[i] = GenerateRandomVipStatus();
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
  printf("Enter the longest waitting time the customer can tolerate in minutes: ");
  scanf("%d", &s->waitHigh); // User provides the VIP wait tolerance here

  // Initialize and insert the first arrival event into the priority queue
  InitEvent(firstevent, 0, arrival, 1, 0, 0, 0, s->ivs[s->ivsIndex++]);
  InitPQueue(&(s->pq));
  InitPQueue(&(s->vipPQueue)); // Initialize global VIP priority queue
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
// This function prioritizes idle tellers, then shorter queues.
// VIPs can consider all tellers. Ordinary customers only consider regular tellers (not VIP_WINDOWS).
int NextAvailableTeller(Simulation *s, isVip iv, int currentTime)
{
  int bestTellerID = -1;
  int minFinishTime = 999999;
  int minQueueCount = 999999;
  int startTeller = 1;
  int endTeller = s->numTellers;

  // For ordinary customers, exclude the VIP window (Teller 1)
  if (iv == notVip && VIP_WINDOWS >= 1)
  {
    startTeller = VIP_WINDOWS + 1;
    if (startTeller > s->numTellers)
    {           // No regular tellers available
      return 1; // Fallback to Teller 1 if no other tellers, though design expects regular tellers
    }
  }

  // 1. Look for any completely idle teller
  for (int i = startTeller; i <= endTeller; i++)
  {
    if (s->tstat[i].isServingCustomer == 0 && IsTellerQueueEmpty(&s->tstat[i]))
    {
      return i; // Found an idle teller, assign immediately
    }
  }

  // 2. If no idle tellers, find the teller with the shortest queue.
  // Tie-break by earliest finish service time.
  bestTellerID = -1;
  minQueueCount = 999999;
  minFinishTime = 999999; // Initialize with a large value

  for (int i = startTeller; i <= endTeller; i++)
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

  if (bestTellerID != -1)
  {
    return bestTellerID;
  }
  else
  {
    // Fallback: If no suitable teller found based on criteria (e.g., only Teller 1 exists for ordinary customer and it's busy)
    // This case ideally shouldn't happen in a well-designed simulation with sufficient tellers.
    // Default to Teller 1 as a last resort, or indicate an error.
    if (s->numTellers >= 1)
      return 1;
    return -1; // Should not happen in a valid simulation setup
  }
}

// Runs the simulation
void RunSimulation(Simulation *s)
{
  Event e;                                          // Current event
  Event *newevent = (Event *)malloc(sizeof(Event)); // New event to be scheduled
  int nexttime;
  int tellerID;
  int servicetime;
  int waittime;
  isVip iv;

  while (!PQEmpty(&(s->pq)))
  {
    e = PQDelete(&(s->pq)); // Get the next event from the priority queue

    // If the event time exceeds simulation length, process only departure events
    // New arrival events beyond simulationLength are discarded.
    if (GetTime(&e) > s->simulationLength && GetEventType(&e) == arrival)
    {
      continue; // Discard new arrival events after simulation length
    }

    if (GetEventType(&e) == arrival)
    {
      iv = GetCustomerType(&e);
      printf("时间: %2d\t%s客户 %d 到达\n", GetTime(&e), (iv == Vip) ? "VIP" : "普通", GetCustomerID(&e));

      // Schedule the next arrival if within simulation limits and predefined VIP list limits
      nexttime = GetTime(&e) + NextArrivalTime(s);
      if (nexttime <= s->simulationLength && s->ivsIndex < MAXCUSTLENGTH)
      {
        s->nextCustomer++;
        InitEvent(newevent, nexttime, arrival, s->nextCustomer, 0, 0, 0, s->ivs[s->ivsIndex++]);
        PQInsert(&(s->pq), *newevent);
      }

      servicetime = Get_ServiceTime(s);                   // Service time for the arriving customer
      tellerID = NextAvailableTeller(s, iv, GetTime(&e)); // Find the best teller for this customer

      printf("DEBUG: 客户 %d (VIP: %d) 到达时间 %d. 选择的出纳员: %d. 出纳员 %d 正在服务: %d. (当前队列长度: %d)\n",
             GetCustomerID(&e), iv, GetTime(&e), tellerID, s->tstat[tellerID].isServingCustomer, s->tstat[tellerID].queueCount);

      // VIP Interruption Logic
      if (iv == Vip)
      {
        // Check if the chosen teller is currently serving an ORDINARY customer
        if (s->tstat[tellerID].isServingCustomer == 1 && GetCustomerType(&s->tstat[tellerID].currentServingCustomerData) == notVip)
        {
          // INTERRUPT SCENARIO: VIP cuts in on an ordinary customer
          Event interruptedCustomer = s->tstat[tellerID].currentServingCustomerData; // Copy the interrupted customer's data
          int elapsedServiceTime = GetTime(&e) - s->tstat[tellerID].currentServingStartTime;
          int originalServiceTime = GetServiceTime(&interruptedCustomer);
          int remainingServiceTime = originalServiceTime - elapsedServiceTime;

          if (remainingServiceTime < 0)
            remainingServiceTime = 0; // Safety check for small remaining times

          // 1. Re-queue the interrupted ordinary customer at the absolute front of the queue
          Event requeuedEvent; // Use a local stack variable for the new event data
          InitEvent(&requeuedEvent,
                    GetTime(&e), // Time when they are re-queued (current simulation time)
                    arrival,     // Event type can remain arrival or be a custom 'requeued' type
                    GetCustomerID(&interruptedCustomer),
                    tellerID,             // Teller they were at
                    0,                    // Waittime initially 0 for this specific event, total wait will accumulate later
                    remainingServiceTime, // Remaining service time
                    notVip);
          PrependCustomerToQueue(&s->tstat[tellerID], requeuedEvent); // Add to the front of teller's queue

          // 2. Update totalCutInTime (this is the key metric the user wants)
          // The remaining service time is added to totalCutInTime.
          s->totalCutInTime += remainingServiceTime;

          printf("\t!!! VIP客户 %d 插队出纳员 %d. 普通客户 %d 被中断 (剩余服务时间 %d). 普通客户已放回队列头部.\n",
                 GetCustomerID(&e), tellerID, GetCustomerID(&interruptedCustomer), remainingServiceTime);

          // 3. VIP starts service immediately
          waittime = 0; // VIP waits 0 time due to cut-in
          s->tstat[tellerID].totalCustomerWait += waittime;
          s->tstat[tellerID].totalCustomerCount++;
          s->tstat[tellerID].totalService += servicetime; // Add VIP's full service time

          int vipDepartureTime = GetTime(&e) + servicetime;
          s->tstat[tellerID].finishService = vipDepartureTime; // Teller busy until VIP finishes

          // Update current serving customer for this teller to the VIP
          s->tstat[tellerID].currentServingCustomerData = e; // Store the VIP's event data
          s->tstat[tellerID].isServingCustomer = 1;
          s->tstat[tellerID].currentServingStartTime = GetTime(&e);

          // Schedule departure for the VIP
          InitEvent(newevent, vipDepartureTime, departure, GetCustomerID(&e), tellerID, waittime, servicetime, Vip);
          PQInsert(&(s->pq), *newevent);

          // Add to timeline
          if (s->tstat[tellerID].timelineCount < MaxPQSize)
          {
            s->tstat[tellerID].timeline[s->tstat[tellerID].timelineCount] = *newevent;
            s->tstat[tellerID].timelineCount++;
          }
          printf("\tVIP客户 %d 开始立即服务.\n", GetCustomerID(&e));
        }
        else // No interruption needed (teller idle, serving a VIP, or VIP window)
        {
          // If teller is free, serve immediately
          if (s->tstat[tellerID].isServingCustomer == 0 && IsTellerQueueEmpty(&s->tstat[tellerID]))
          {
            waittime = 0;
            s->tstat[tellerID].totalCustomerWait += waittime;
            s->tstat[tellerID].totalCustomerCount++;
            s->tstat[tellerID].totalService += servicetime;

            int departureTime = GetTime(&e) + servicetime;
            InitEvent(newevent, departureTime, departure, GetCustomerID(&e), tellerID, waittime, servicetime, iv);
            PQInsert(&(s->pq), *newevent);
            s->tstat[tellerID].finishService = departureTime;

            s->tstat[tellerID].currentServingCustomerData = e;
            s->tstat[tellerID].isServingCustomer = 1;
            s->tstat[tellerID].currentServingStartTime = GetTime(&e);

            if (s->tstat[tellerID].timelineCount < MaxPQSize)
            {
              s->tstat[tellerID].timeline[s->tstat[tellerID].timelineCount] = *newevent;
              s->tstat[tellerID].timelineCount++;
            }
            printf("\t出纳员 %d\t等待时间 %d\t服务时间 %d (立即服务)\n", tellerID, waittime, servicetime);
          }
          else // Teller is busy or has a queue, VIP needs to wait/queue
          {
            InsertVipIntoQueue(&s->tstat[tellerID], e); // Inserts at the front, after existing VIPs
            printf("\tVIP客户 %d 在出纳员 %d 排队 (插入VIP队列). 队列头: %d, 队列尾: %d, 当前队列长度: %d\n",
                   GetCustomerID(&e), tellerID,
                   (s->tstat[tellerID].customerQueueHead ? GetCustomerID(&s->tstat[tellerID].customerQueueHead->customerEvent) : 0),
                   (s->tstat[tellerID].customerQueueTail ? GetCustomerID(&s->tstat[tellerID].customerQueueTail->customerEvent) : 0),
                   s->tstat[tellerID].queueCount);
          }
        }
      }
      else // Ordinary Customer Arrival
      {
        // If the teller is currently free and their queue is empty
        if (s->tstat[tellerID].isServingCustomer == 0 && IsTellerQueueEmpty(&s->tstat[tellerID]))
        {
          waittime = 0; // Immediate service, no wait

          s->tstat[tellerID].totalCustomerWait += waittime;
          s->tstat[tellerID].totalCustomerCount++;
          s->tstat[tellerID].totalService += servicetime;

          // Schedule departure for this immediately served customer
          InitEvent(newevent, GetTime(&e) + servicetime,
                    departure, GetCustomerID(&e), tellerID,
                    waittime, servicetime, notVip);
          PQInsert(&(s->pq), *newevent);
          s->tstat[tellerID].finishService = GetTime(&e) + servicetime;

          s->tstat[tellerID].currentServingCustomerData = e;
          s->tstat[tellerID].isServingCustomer = 1;
          s->tstat[tellerID].currentServingStartTime = GetTime(&e);

          if (s->tstat[tellerID].timelineCount < MaxPQSize)
          {
            s->tstat[tellerID].timeline[s->tstat[tellerID].timelineCount] = *newevent;
            s->tstat[tellerID].timelineCount++;
          }
          printf("\t出纳员 %d\t等待时间 %d\t服务时间 %d (立即服务)\n", tellerID, waittime, servicetime);
        }
        else // Teller is busy or has a queue, customer needs to wait
        {
          EnqueueCustomer(&s->tstat[tellerID], e); // Normal enqueue for ordinary customers
          printf("\t客户 %d 在出纳员 %d 排队 (正常入队). 队列头: %d, 队列尾: %d, 当前队列长度: %d\n",
                 GetCustomerID(&e), tellerID,
                 (s->tstat[tellerID].customerQueueHead ? GetCustomerID(&s->tstat[tellerID].customerQueueHead->customerEvent) : 0),
                 (s->tstat[tellerID].customerQueueTail ? GetCustomerID(&s->tstat[tellerID].customerQueueTail->customerEvent) : 0),
                 s->tstat[tellerID].queueCount);
        }
      }
    }
    else // GetEventType(e) == departure
    {
      tellerID = GetTellerID(&e); // Get the teller ID from the departing event

      // Clear current serving status for this teller
      s->tstat[tellerID].isServingCustomer = 0;
      s->tstat[tellerID].currentServingStartTime = 0;

      // Update customer type counts
      if (GetCustomerType(&e) == Vip)
        s->totalVipCustomerCount++;
      else
        s->totalOrdinaryCustomerCount++;

      printf("时间: %2d\t%s客户 %d 离开\n", GetTime(&e), (GetCustomerType(&e) == Vip) ? "VIP" : "普通", GetCustomerID(&e));
      printf("\t出纳员 %d\t等待时间 %d\t服务时间 %d\n", GetTellerID(&e), GetWaitTime(&e), GetServiceTime(&e));

      printf("DEBUG: 出纳员 %d 离开时间 %d. 队列是否为空: %d. (当前队列长度: %d)\n", tellerID, GetTime(&e), IsTellerQueueEmpty(&s->tstat[tellerID]), s->tstat[tellerID].queueCount);

      // Step 1: Recalculate waiting times for ALL VIPs in ALL queues and populate global vipPQueue
      InitPQueue(&(s->vipPQueue)); // Re-initialize to effectively clear it.

      for (int i = 1; i <= s->numTellers; i++)
      {
        Node *current = s->tstat[i].customerQueueHead;
        while (current != NULL)
        {
          Event queuedEvent = current->customerEvent; // Copy event data
          if (GetCustomerType(&queuedEvent) == Vip)
          {
            int current_wait_time = GetTime(&e) - GetTime(&queuedEvent); // Current simulation time - VIP arrival time
            if (current_wait_time >= s->waitHigh)
            {
              // Create a temporary event to put into vipPQueue for priority comparison
              Event temp_vip_event;
              InitEvent(&temp_vip_event, -current_wait_time, departure,        // Use negative wait time for max-heap priority
                        GetCustomerID(&queuedEvent), i,                        // Store original teller ID in tellerID field
                        current_wait_time, GetServiceTime(&queuedEvent), Vip); // Store actual wait time and service time
              PQInsert(&(s->vipPQueue), temp_vip_event);
            }
          }
          current = current->next;
        }
      }

      // Step 2: Decide next customer to serve for this teller (tellerID)
      Event nextCustomerToServe;
      int nextServiceTime;
      int nextWaitTime;
      isVip nextCustomerType;

      if (!PQEmpty(&(s->vipPQueue)))
      {
        // Serve highest priority VIP from global list (Requirement 2)
        Event highPriorityVip = PQDelete(&(s->vipPQueue));
        int vipCustomerID_to_remove = GetCustomerID(&highPriorityVip);
        int originalTellerID_of_vip = GetTellerID(&highPriorityVip);

        // Remove the VIP from their original teller's queue (if they were waiting there)
        RemoveCustomerByID(&s->tstat[originalTellerID_of_vip], vipCustomerID_to_remove);

        nextCustomerToServe = highPriorityVip;
        nextServiceTime = Get_ServiceTime(s);         // Generate new service time
        nextWaitTime = GetWaitTime(&highPriorityVip); // Accumulated wait time for VIP
        nextCustomerType = Vip;

        printf("DEBUG: VIP客户 %d (来自出纳员 %d 的队列) 等待时间 %d 达到最长等待时间 %d，优先服务出纳员 %d。\n",
               vipCustomerID_to_remove, originalTellerID_of_vip, nextWaitTime, s->waitHigh, tellerID);
      }
      else if (!IsTellerQueueEmpty(&s->tstat[tellerID]))
      {
        // No high-priority VIPs globally, serve from this teller's own queue (original logic)
        printf("DEBUG: 出纳员 %d 队列不为空。处理下一个客户。\n", tellerID);
        nextCustomerToServe = DequeueCustomer(&s->tstat[tellerID]); // Get the next customer from this teller's queue
        nextServiceTime = Get_ServiceTime(s);                       // Generate new service time
        nextWaitTime = GetTime(&e) - GetTime(&nextCustomerToServe); // Wait time for this customer
        if (nextWaitTime < 0)
          nextWaitTime = 0; // Safety check
        nextCustomerType = GetCustomerType(&nextCustomerToServe);

        printf("DEBUG: Dequeued C%d from T%d. Post-dequeue Queue: Head:%d, Tail:%d, Count:%d\n",
               GetCustomerID(&nextCustomerToServe), tellerID,
               (s->tstat[tellerID].customerQueueHead ? GetCustomerID(&s->tstat[tellerID].customerQueueHead->customerEvent) : 0),
               (s->tstat[tellerID].customerQueueTail ? GetCustomerID(&s->tstat[tellerID].customerQueueTail->customerEvent) : 0),
               s->tstat[tellerID].queueCount);
      }
      else
      {
        // Teller's queue is empty and no high-priority VIPs to serve, teller becomes idle
        printf("DEBUG: 出纳员 %d 队列为空。出纳员变为空闲。\n", tellerID);
        s->tstat[tellerID].finishService = GetTime(&e); // Teller becomes idle at current time
        // The totalTellerIdleTime will be calculated in PrintSimulationResults based on totalService
        continue; // No new event to schedule for this teller
      }

      // Schedule departure for the next customer to be served by this teller
      s->tstat[tellerID].totalCustomerWait += nextWaitTime;
      s->tstat[tellerID].totalCustomerCount++;
      s->tstat[tellerID].totalService += nextServiceTime;

      int nextCustomerDepartureTime = GetTime(&e) + nextServiceTime;
      s->tstat[tellerID].finishService = nextCustomerDepartureTime; // Teller busy until this customer finishes

      s->tstat[tellerID].currentServingCustomerData = nextCustomerToServe; // Store the event data
      s->tstat[tellerID].isServingCustomer = 1;
      s->tstat[tellerID].currentServingStartTime = GetTime(&e);

      InitEvent(newevent, nextCustomerDepartureTime, departure,
                GetCustomerID(&nextCustomerToServe), tellerID,
                nextWaitTime, nextServiceTime, nextCustomerType);
      PQInsert(&(s->pq), *newevent);

      if (s->tstat[tellerID].timelineCount < MaxPQSize)
      {
        s->tstat[tellerID].timeline[s->tstat[tellerID].timelineCount] = *newevent;
        s->tstat[tellerID].timelineCount++;
      }
      printf("\t出纳员 %d 开始服务排队的 %s客户 %d\n", tellerID, (nextCustomerType == Vip) ? "VIP" : "普通", GetCustomerID(&nextCustomerToServe));
    }
  }

  // Ensure simulationLength reflects the actual last event time if it extends beyond initial.
  if (GetTime(&e) > s->simulationLength)
  {
    s->simulationLength = GetTime(&e);
  }

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

  // Calculate total teller idle time
  for (i = 1; i <= s->numTellers; i++)
  {
    // Idle time is total simulation length minus total service time for the teller.
    // Assuming tellers are idle when not serving.
    s->totalTellerIdleTime += (s->simulationLength - s->tstat[i].totalService);
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
      Event te = s->tstat[i].timeline[j]; // Copy event data
      int iv_type = GetCustomerType(&te);
      int waitTime = GetWaitTime(&te);
      // Accumulate total VIP wait time only for VIP departure events
      if (iv_type == Vip)
      {
        // s->totalVipWaitTime is already accumulated in RunSimulation based on GetWaitTime in departure events
        // so no need to sum up again from timeline.
      }
      printf("\t\t时间 %2d：%s客户 %d，服务时间 %d，等待时间 %d\n",
             GetTime(&te), (iv_type == Vip) ? "VIP" : "普通",
             GetCustomerID(&te), GetServiceTime(&te), waitTime);
    }
  }
  printf("\n");
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
    exit(1);
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
    // Return an invalid event to indicate error
    InitEvent(&emptyEvent, -1, arrival, -1, -1, -1, -1, notVip);
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

// Inserts a VIP customer into the queue according to priority rules (after existing VIPs, before ordinary)
void InsertVipIntoQueue(TellerStats *ts, Event vipEvent)
{
  Node *newNode = (Node *)malloc(sizeof(Node));
  if (newNode == NULL)
  {
    fprintf(stderr, "Error: Memory allocation failed for VIP new node.\n");
    exit(1);
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

// NEW: Inserts a customer (likely an interrupted ordinary customer) at the absolute head of the queue.
void PrependCustomerToQueue(TellerStats *ts, Event customerEvent)
{
  Node *newNode = (Node *)malloc(sizeof(Node));
  if (newNode == NULL)
  {
    fprintf(stderr, "Error: Memory allocation failed for new node (prepend).\n");
    exit(1);
  }
  newNode->customerEvent = customerEvent;
  newNode->next = ts->customerQueueHead;
  ts->customerQueueHead = newNode;
  if (ts->customerQueueTail == NULL) // If queue was empty before prepending
  {
    ts->customerQueueTail = newNode;
  }
  ts->queueCount++;
}

// NEW: Function to remove a customer by ID from a teller's queue
// Used when a VIP is pulled from a specific teller's queue due to waitHigh policy.
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

// 根据customerID查找其服务时间 (This function assumes the customer is currently in the queue for a given teller)
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
  // Also check if the customer is currently being served by this teller
  if (ts->isServingCustomer && GetCustomerID(&ts->currentServingCustomerData) == customerID)
  {
    return GetServiceTime(&ts->currentServingCustomerData);
  }
  return 0; // Not found, return 0
}

#endif /* SIMULATION */