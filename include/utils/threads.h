#ifndef UTILS_THREADS_H_
#define UTILS_THREADS_H_
/**
 * Wrappers for thread support routines,
 * including thread manipulation and synchronization primitives.
 *
 * Use c++11 primitives and routines.
 */
#include <condition_variable>
#include <mutex>
#include <thread>

using thread_t = std::thread;
using lock_t = std::mutex;
using cond_t = std::condition_variable;


/* Threads */
/*  Use constructor to create a thread and execute the function:
 *  template< class Function, class... Args >
 *  explicit thread( Function&& f, Args&&... args );
 *
 *  Use member function join() to join.
 */


/* Mutexes */
/*  Use member functions lock(), try_lock(), and unlock().
 */

/*  General-purpose mutex ownership wrapper.
 */
#define mutex_begin(uniq_lk, lk) \
    { std::unique_lock<lock_t> uniq_lk(lk);

#define mutex_end() \
    }


/* Condition variables */
/*  Use member functions:
 *  void wait( std::unique_lock<std::mutex>& lock );
 *  template< class Predicate >
 *  void wait( std::unique_lock<std::mutex>& lock, Predicate pred );
 *  void notify_one();
 *  void notify_all();
 */

#endif // UTILS_THREADS_H_

