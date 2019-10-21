#include <sys/mman.h>
#include <linux/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "sem.h"

//Add your struct cs1550_sem type declaration below

typedef struct cs1550_sem semaphore;

void down(semaphore *sem) {
  syscall(__NR_cs1550_down, sem);
}

void up(semaphore *sem) {
  syscall(__NR_cs1550_up, sem);
}

/*
SEMAPHORES
----------
 1. enter   ->  This is the semaphore to enter the apartment. Down by tenants,
                up by the agent to let the tenants in.
 2. tenants ->  This is the semaphore to let the agent know that there's a
                tenant waiting for him. Down by Agent, Up by tenants.
 3. agent   ->  This is a binary semaphore to lock the apartment when the agent
                arrives. Only one agent is allowed to access it at a time.
 4. leave   ->  This is a semaphore to indicate the agent to wait for the tenants.
                Down by the agent to wait, Up by the last tenant in the apartment
                to let the agent exit.
 5. mutex   ->  Binary semaphore to protect the critical regions.
 6. printex ->  Binary semaphore to protect the print statements.
 ---------
*/

semaphore *enter;
semaphore *tenants;
semaphore *agent;
semaphore *leave;
semaphore *mutex;
semaphore *printex;


int *tenants_waiting; //The number of tenants who are waiting to enter.
int *tenants_agent_has_seen; //The number of tenants the current agent has seen.
int *tenants_inside; //The number of tenants currently inspecting the apartment.
int *agent_inside; // 0 or 1 depending on if there's an agent currently inside.

/*
  agentArrives - Agent Arrival
*/
void* agentArrives(int p_num, time_t* start_time){

  //printf("There's an agent in the agentArrives method.\n");

  // if its the first agent, he gets access, else other agent sleeps until
  // the agent inside ups
  down(agent);

  //printf("Agent %d is the current agent now.\n", p_num);

  // sleep if no tenants
  down(tenants);

  //printf("Agent %d wakes up because there are tenants waiting for him.\n", p_num);

  // agent is inside the apartment now.
  down(mutex);
  *agent_inside = 1;
  up(mutex);

  //print the time the agent arrives
  time_t end_time = time(NULL);
  time_t elapsed_time = end_time - *start_time;
  down(printex);
  printf("Agent %d arrives at time %d\n", p_num, (int) elapsed_time);
  up(printex);

}

/*
  tenantArrives - Tenant Arrival
*/
void* tenantArrives(int p_num, time_t* start_time){

  //printf("Tenant %d arrives in the tenantArrives method.\n", p_num);

  //update tenants_waiting counter
  down(mutex);
  *tenants_waiting = *tenants_waiting + 1;
  up(mutex);
  /*
   If there's no agent inside, wake the agent up, and go to sleep until
   the agent lets the tenant in.
   If there's an agent inside already, and there's occupancy available,
   enter the apartment directly.
   If there's an agent inside already but no space, wait on the semaphore.
  */

  if(*agent_inside == 0){

    //printf("Agent not arrived yet, so need to wake him up\n");
    //printf("Tenant %d proceeds to arrive\n");

    // print the arrival time
    time_t end_time = time(NULL);
    time_t elapsed_time = end_time - *start_time;
    down(printex);
    printf("Tenant %d arrives at time %d\n", p_num, (int)elapsed_time);
    up(printex);

    // Only wake up the agent once, i.e. here the first tenant will wake him up.
    if(*tenants_waiting ==  1){
      up(tenants);
    }

    // Wait for the agent to be let in the apartment.
    down(enter);
  }else{

    //printf("New tenant has arrived, and the agent has already arrived\n");

    time_t end_time = time(NULL);
    time_t elapsed_time = end_time - *start_time;
    down(printex);
    printf("Tenant %d arrives at time %d\n", p_num, (int)elapsed_time);
    up(printex);

    // Check if there's any space available.
    down(mutex);
    if(*tenants_agent_has_seen < 10){
      // There's space available. Update the counters and enter directly.
      *tenants_inside = *tenants_inside + 1;
      *tenants_agent_has_seen = *tenants_agent_has_seen + 1;
      // Tenant does not have to wait.
      *tenants_waiting = *tenants_waiting - 1;
      up(mutex);
    }else{
      up(mutex);
      // No space available, so need to wait.
      down(enter);
    }
  }
}

/*
  viewApt - Tenant views the apartment here.
*/

void* viewApt(int p_num, time_t* start_time){
  down(printex);
  time_t end_time = time(NULL);
  time_t elapsed_time = end_time - *start_time;
  printf("Tenant %d inspects the apartment at time %d\n", p_num, (int)elapsed_time);
  up(printex);

  // Tenant takes 2 seconds to inspect.
  sleep(2);
}

/*
  openApt - Agent opens the apartment for tenants to inspect and lets them in if they
  arrive before him and are waiting to enter.
*/

void* openApt(int p_num, time_t* start_time){
  int i = 0;
  //printf("Agent %d is going to open the apartment now.\n", p_num);

  // print the time agent opens the  apartment
  time_t end_time = time(NULL);
  time_t elapsed_time = end_time - *start_time;
  down(printex);
  printf("Agent %d opens the apartment for inspection at time %d.\n", p_num, (int)elapsed_time);
  up(printex);

  // Check if there are people waiting, and let them in until there's space available.

  //printf("Agent %d checks who to let in.\n", p_num);
  if(*tenants_waiting > 0){

    //printf("%d Tenants are waiting, letting them in until space runs out. \n", *tenants_waiting);
    down(mutex);
    while(*tenants_inside < 10 && *tenants_agent_has_seen < 10 && *agent_inside == 1){
      //update the counters, and open the door
      *tenants_agent_has_seen = *tenants_agent_has_seen + 1;
      *tenants_inside = *tenants_inside + 1;
      *tenants_waiting = *tenants_waiting - 1;
      up(enter);

      // If no more tenants are waiting, break the loop and leave.
      if(*tenants_waiting == 0){
        //printf("leaving openApt\n", *tenants_waiting);
        break;
      }
    }
    up(mutex);
  }

  // Wait until all the tenants have left.

  //printf("agent being blocked until all tenants leave.\n", p_num);
  down(leave);
  //printf("Agent is going to leave now.\n");
}

/*
  tenantLeaves - Tenant leaves here.
*/
void* tenantLeaves(int p_num, time_t* start_time){

  // Update the counter

  //printf("Tenant %d going to leave.\n", p_num);
  down(mutex);
  *tenants_inside = *tenants_inside - 1;
  up(mutex);

  // Print exit time for the tenant.
  time_t end_time = time(NULL);
  time_t elapsed_time = end_time - *start_time;
  down(printex);
  printf("Tenant %d leaves the apartment at time %d.\n", p_num, (int)elapsed_time);
  up(printex);

  // The Last Tenant tells the Agent to leave.
  if(*tenants_inside == 0 && (*tenants_waiting == 0 || *tenants_agent_has_seen%10 == 0)){
    // If there are tenants still waiting, let the next agent know about it.
    if(*tenants_waiting > 0){
      up(tenants);
    }
    // Tell the agent to leave now.
    up(leave);
  }
}

/*
  agentLeaves - Agent leaves here.
*/
void* agentLeaves(int p_num, time_t* start_time){
  //printf("Agent %d is going to leave now.", p_num);

  // Update the counters so that the next agent can use them.
  down(mutex);
  *tenants_agent_has_seen = 0;
  *agent_inside = 0;
  up(mutex);

  // Print the time the agent leaves.
  down(printex);
  time_t end_time = time(NULL);
  time_t elapsed_time = end_time - *start_time;
  printf("Agent %d leaves the apartment at time %d.\n", p_num, (int)elapsed_time);
  up(printex);

  // Give up the agent semaphore. Lets a new agent secure it.
  up(agent);
}

// Method Process structure.

void* tenant_process(int i, time_t* start_time){
  tenantArrives(i, start_time);
  viewApt(i, start_time);
  tenantLeaves(i, start_time);
  exit(0);
}

// Agent Process structure.

void* agent_process(int i, time_t* start_time){
  agentArrives(i, start_time);
  openApt(i, start_time);
  agentLeaves(i, start_time);
  exit(0);
}

int main(int argc, char *argv[]){

  // Variables for for loops, the arguments passed in and time.
  int i, j, k, m, n, pt, dt, st, pa, da, sa;
  time_t start_time;

  //tenants
  enter = mmap(NULL,sizeof(semaphore), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  leave = mmap(NULL,sizeof(semaphore), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

  //agents
  agent = mmap(NULL,sizeof(semaphore), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  tenants = mmap(NULL,sizeof(semaphore), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

  //mutex
  mutex = mmap(NULL,sizeof(semaphore), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  printex = mmap(NULL,sizeof(semaphore), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

  //int counters
  tenants_waiting = mmap(NULL, (sizeof(int)), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  tenants_agent_has_seen = mmap(NULL, (sizeof(int)), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  tenants_inside = mmap(NULL, (sizeof(int)), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  agent_inside = mmap(NULL, (sizeof(int)), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

  *tenants_waiting = 0;
  *tenants_agent_has_seen = 0;
  *tenants_inside = 0;
  *agent_inside = 0;

  // This is the start time of the program.
  start_time = time(NULL);

  /*
    m   is the number of tenants
    k   is the number of agents
    pt  is the probability of continous next tenant arrival
    dt  is the delay time if the next tenant does not arrive straight away
    st  is the seed for generating the random number used to calculate the
        probability for tenant process's arrival
    pa  is the probability of continous next agent arrival
    da  is the delay time if the next agent does not arrive straight away
    sa  is the seed for generating the random number used to calculate the
        probability for agent process's arrival
  */

  m = atoi(argv[2]);
  k = atoi(argv[4]);
  pt = atoi(argv[6]);
  dt = atoi(argv[8]);
  st = atoi(argv[10]);
  pa = atoi(argv[12]);
  da = atoi(argv[14]);
  sa = atoi(argv[16]);

  // initialize the semaphores

  enter->value  = 0;
  enter->head   = NULL;
  enter->tail   = NULL;

  //binary
  tenants->value = 1;
  tenants->head = NULL;
  tenants->tail = NULL;

  //binary
  agent->value  = 1;
  agent->head   = NULL;
  agent->tail   = NULL;

  leave->value  = 0;
  leave->head   = NULL;
  leave->tail   = NULL;

  //binary
  printex->value = 1;
  printex->head = NULL;
  printex->tail = NULL;

  //binary
  mutex->value = 1;
  mutex->head = NULL;
  mutex->tail = NULL;

  int pid = fork();
  if(pid == 0){
    // Print once in the beginning.
    printf("The apartment is now empty.\n");

    // Tenant Process
    // create m tenants.
    for(i=0; i<m; i++){
      pid = fork();
      if(pid == 0){
        //Tenant Algorithm
        tenant_process(i+1, &start_time);
      }else{
        srand(st);
        int prob = rand() % 10;
        // delay if probability is less than pt
        if(prob < (pt/10)){
        }else{
          sleep(dt);
        }
      }
    }
    // Wait for all the children processes
    for(i=0; i<m; i++){
      wait(NULL);
    }
    return 0;
  }else{

    // Agent Process
    // create k agents
    for(i=0; i<k; i++){
      pid = fork();
      if(pid == 0){
        //Agent Algorithm
        agent_process(i+1, &start_time);
      }else{
        srand(sa);
        int prob = rand() % 10;
        // delay if probability is less than pa
        if(prob < (pa/10)){
        }else{
          sleep(da);
        }
      }
    }
    // Wait for all the children processes
    for(i=0; i<k; i++){
      wait(NULL);
    }
    return 0;
  }
  // Wait for the main's child process
  wait(NULL);
  return 0;
}
