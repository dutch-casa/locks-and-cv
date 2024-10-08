/*
 * Synchronization primitives.
 * See synch.h for specifications of the functions.
 */

#include <types.h>
#include <lib.h>
#include <synch.h>
#include <thread.h>
#include <curthread.h>
#include <machine/spl.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *namearg, int initial_count)
{
	struct semaphore *sem;

	sem = kmalloc(sizeof(struct semaphore));
	if (sem == NULL) {
		return NULL;
	}

	sem->name = kstrdup(namearg);
	if (sem->name == NULL) {
		kfree(sem);
		return NULL;
	}

	sem->count = initial_count;
	return sem;
}

void
sem_destroy(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);

	spl = splhigh();
	assert(thread_hassleepers(sem)==0);
	splx(spl);

	/*
	 * Note: while someone could theoretically start sleeping on
	 * the semaphore after the above test but before we free it,
	 * if they're going to do that, they can just as easily wait
	 * a bit and start sleeping on the semaphore after it's been
	 * freed. Consequently, there's not a whole lot of point in 
	 * including the kfrees in the splhigh block, so we don't.
	 */

	kfree(sem->name);
	kfree(sem);
}

void 
P(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);

	/*
	 * May not block in an interrupt handler.
	 *
	 * For robustness, always check, even if we can actually
	 * complete the P without blocking.
	 */
	assert(in_interrupt==0);

	spl = splhigh();
	while (sem->count==0) {
		thread_sleep(sem);
	}
	assert(sem->count>0);
	sem->count--;
	splx(spl);
}

void
V(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);
	spl = splhigh();
	sem->count++;
	assert(sem->count>0);
	thread_wakeup(sem);
	splx(spl);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
	struct lock *lock;

	lock = kmalloc(sizeof(struct lock));
	if (lock == NULL) {
		return NULL;
	}

	lock->name = kstrdup(name);
	if (lock->name == NULL) {
		kfree(lock);
		return NULL;
	}
	
	//initialize lock state
	lock->lk_holder = NULL;	
	
	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);

	
	kfree(lock->name);
	kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
	//disable interrupts
	int spl;
	assert(lock != NULL);
	spl = splhigh();

	while(lock->lk_holder != NULL) {
		//We can't use the lock if another thread has it, so we'll sleep
		thread_sleep(lock);
	}

	//once the current thread is holding the lock we can continue
	
	lock->lk_holder = curthread;

	//reenable interrupts
	splx(spl);
	
	(void)lock;  // suppress warning until code gets written
}

void
lock_release(struct lock *lock)
{
	//init spl and make sure lock isn't null
	int spl;
	assert(lock != NULL);

	//disable interrupts
	spl = splhigh();

	// let go of the lock
	lock -> lk_holder = NULL;

	//wakeup any threads that were waiting on this lock
	thread_wakeup(lock);

	//reenable interrupts
	splx(spl);


	(void)lock;  // suppress warning until code gets written
}

int
lock_do_i_hold(struct lock *lock)
{
	//returns true or false depending if thread is holding a lock.
	return(lock->lk_holder == curthread);
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
	struct cv *cv;

	cv = kmalloc(sizeof(struct cv));
	if (cv == NULL) {
		return NULL;
	}

	cv->name = kstrdup(name);
	if (cv->name==NULL) {
		kfree(cv);
		return NULL;
	}
	
	
	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);
	
	kfree(cv->name);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
   	assert(lock != NULL);
    	assert(lock_do_i_hold(lock));

    	int spl = splhigh();
   	lock_release(lock);
   	thread_sleep(cv);
   	splx(spl);

   	lock_acquire(lock);
	(void)cv;    // suppress warning until code gets written
	(void)lock;  // suppress warning until code gets written
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	int spl;
	assert(cv != NULL);
	assert(lock != NULL);
	assert(lock_do_i_hold(lock));
	thread_wakeone(cv);	
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);
	assert(lock_do_i_hold(lock));

	// This whole operation should be atomic
	int spl = splhigh();
	thread_wakeup(cv);
	splx(spl);
	(void)cv;    // suppress warning until code gets written
	(void)lock;  // suppress warning until code gets written
}
