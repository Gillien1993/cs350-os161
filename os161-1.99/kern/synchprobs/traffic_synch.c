#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
//static struct semaphore *intersectionSem;

//Number of cars going from ori to des
static int NumCars[4][4];
//The lock of intercection that work with all the cvs
static struct lock *InterLock;
//Each ori to des has a cv that will check if the car is safe to go(ie satisfies the 3 conditions so that will not collide with other cars in the intersection)
static struct cv *ODcv[4][4];

/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */


bool
right_turn(Direction origin, Direction destination);
bool
no_collision(Direction o, Direction d);

//helper function: determine if the car is makeing a right turn

bool
right_turn(Direction origin, Direction destination) {
  if (((origin == west) && (destination == south)) ||
      ((origin == south) && (destination == east)) ||
      ((origin == east) && (destination == north)) ||
      ((origin == north) && (destination == west))) {
    return true;
  } else {
    return false;
  }
}

//helper function: determine if the car is safe to go

bool
no_collision(Direction o, Direction d){
	for (unsigned int i=0; i<4; ++i){
		for (unsigned int j=0; j<4; ++j){
			//compare current car's ori and des with the cars in the intersection
			if(NumCars[i][j] > 0){
				// checking the 3 conditions given for safty
				if((o == i) || 
                                   (o == j && d == i) || 
		                   (d != j && right_turn(o,d)) ||
				   (d != j && i-j == 1) ||
				   (d != j && i == 0 && j == 3)){
					continue;}
				else{ return false;}

			}
		}
	}
	return true;
}



void
intersection_sync_init(void)
{


  /* replace this default implementation with your own implementation */
	InterLock = lock_create("InterLock");
	if(InterLock == NULL){panic("InterLock init");}
	
	for (int i=0; i<4; ++i){
		for (int j=0; j<4; ++j){
			NumCars[i][j] = 0;

			ODcv[i][j] = cv_create("ODcv");
			if(ODcv[i][j] == NULL){panic("ODcv init");}
		}
	}
	return;

}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
	KASSERT(InterLock != NULL);
	lock_destroy(InterLock);

	for (int i=0; i<4; ++i){
		for (int j=0; j<4; ++j){
			
			KASSERT(ODcv[i][j] != NULL);
			cv_destroy(ODcv[i][j]);
		
		}
	}
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
	
	//kprintf("%d%d\n",origin,destination);

  KASSERT(InterLock != NULL);
	lock_acquire(InterLock);

	//block until it is safe to pass
	while(1){
		if(no_collision(origin,destination)){
			++NumCars[origin][destination];
			lock_release(InterLock);
			break;
		}
		else{
			cv_wait(ODcv[origin][destination],InterLock);
		}
	}
	//let the car pass
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
	KASSERT(InterLock != NULL);
	
	lock_acquire(InterLock);

	--NumCars[origin][destination];

	for (int i=0; i<4; ++i){
		for (int j=0; j<4; ++j){
			
			cv_broadcast(ODcv[i][j],InterLock);	
		}
	}
	
	lock_release(InterLock);

}
