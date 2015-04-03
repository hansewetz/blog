#ifndef __TPOOL_H__
#define __TPOOL_H__
#include "type-utils/type_utils.h"
#include <iosfwd>
#include <thread>
#include <atomic>
#include <queue>
#include <vector>
#include <tuple>
#include <mutex>
#include <future>
#include <chrono>
#include <type_traits>

namespace utils{

// ------------------- tpool --------------
class tpool{
private:
  // forward decl.
  template<typename F,typename R=typename std::result_of<F()>::type>struct fret_type;
  class task;
public:
  // return type for functions returning void
  struct void_t{};

  // ctor
  tpool(std::size_t nthreads):nthreads_(nthreads),done_(false){
    for(std::size_t i=0;i<nthreads_;++i){
      std::thread thr(&tpool::thread_func,this);
      threads_.push_back(std::move(thr));
    }
  }
  // dtor
  ~tpool(){
    done_=true;
    for(auto&t:threads_)t.join();
  }
  // get #of threads
  std::size_t nthreads()const{return nthreads_;}

  // submit task
  template<typename F,typename R=typename std::result_of<F()>::type>
  std::future<R>submit(F f){
    std::packaged_task<R()>pt(f);
    auto ret=std::move(pt.get_future());
    add_task(task(std::move(pt)));
    return ret;
  }
  // submit tuple of tasks and return a tuple containing results of tasks
  template<typename...F,typename RET=std::tuple<typename fret_type<F>::type...>>
  RET submitTaskTupleAndWait(std::tuple<F...>ftu){
    auto futs=submitTasksReturnFuts(std::forward<decltype(ftu)>(std::move(ftu)));
    RET*dummy=nullptr;
    return waitForFutures(dummy,std::move(futs));
  }
  // submit list of tasks and return a tuple containing results of tasks
  template<typename...F,typename RET=std::tuple<typename fret_type<F>::type...>>
  RET submitTasksAndWait(F...f){
    std::tuple<F...>ftu{std::move(f)...};
    return submitTaskTupleAndWait(std::move(ftu));
  }
private:
  // submit tasks and return futures in a tuple
  template<typename F,typename...G,typename RET=std::tuple<std::future<typename std::result_of<F()>::type>,std::future<typename std::result_of<G()>::type>...>>
  RET submitTasksReturnFuts(std::tuple<F,G...>ftu){
    auto tu1=make_tuple(std::move(submit(std::get<0>(ftu))));
    auto tu2=submitTasksReturnFuts(utils::popn_front_tuple<1>(ftu));
    return std::tuple_cat(std::move(tu1),std::move(tu2));
  }
  template<typename F,typename FUTS=std::tuple<std::future<typename std::result_of<F()>::type>>>
  FUTS submitTasksReturnFuts(std::tuple<F>ftu){
    return std::make_tuple(std::move(submit(std::get<0>(ftu))));
  }
  // wait for futures stored in a tuple and return results
  template<typename R1,typename...R2,typename F1,typename...F2>
  std::tuple<R1,R2...>waitForFutures(std::tuple<R1,R2...>*,std::tuple<F1,F2...>&&futs)const{
    // use references to futures
    auto&fu1=std::get<0>(futs);
    auto r1=std::make_tuple(get_fut(static_cast<R1*>(nullptr),fu1));
    auto t2=utils::popn_front_tuple<1>(futs);
    std::tuple<R2...>*dummy;
    auto r2=waitForFutures(dummy,std::move(t2));
    return std::tuple_cat(std::move(r1),std::move(r2));
  }
  template<typename R1,typename F1>
  std::tuple<R1>waitForFutures(std::tuple<R1>*,std::tuple<F1>&&fut)const{
    // use reference to future
    auto&fu1=std::get<0>(fut);
    return std::make_tuple(get_fut(static_cast<R1*>(nullptr),fu1));
  }
  // wait for future and return result
  template<typename R,typename FU>
  R get_fut(R*,FU&fu)const{
    return fu.get();
  }
  template<typename FU>
  void_t get_fut(void_t*,FU&fu)const{
    fu.get();
    return void_t();
  }
  // thread function
  void thread_func(){
    while(!done_){
      try{
        task tsk;
        if(try_get(tsk))tsk();
        else std::this_thread::yield();
      }
      catch(...){
        done_=true;
      }
    }
  }
  // get next task
  bool try_get(task&tsk){
    std::unique_lock<std::mutex>lock(qmtx_);
    if(taskq_.empty())return false;
    tsk=std::move(taskq_.front());
    taskq_.pop();
    return true;
  }
  // add task to queue
  void add_task(task&&tsk){
    std::unique_lock<std::mutex>lock(qmtx_);
    taskq_.push(std::move(tsk));
  }
  // meta function returning 'void_t' as return type if F returns void, else as std::result_of<F>::type
  template<typename F,typename R>
  struct fret_type{
    using type=typename std::conditional<std::is_same<R,void>::value,void_t,R>::type;
  };
  // task class (hides actual type of function to be executed)
  class task{
      struct base{
        virtual void call()=0;
      };
      template<typename F>
      struct derived:base{
        derived(F&&f):f_(std::move(f)){}
        void call(){f_();}
        F f_;
      };
      std::unique_ptr<base>impl_;
  public:
      // ctor, assign, dtor
      task()=default;
      template<typename F>task(F&&f):impl_(new derived<F>(std::move(f))){}
      task(task&&other):impl_(std::move(other.impl_)){}
      task&operator=(task&&tsk){impl_=std::move(tsk.impl_);}

      // call
      void operator()(){impl_->call();}
  };
  // state
  std::size_t nthreads_;
  std::vector<std::thread>threads_;
  std::atomic_bool done_;
  std::queue<task>taskq_;
  std::mutex qmtx_;
};
// print operator for tpool::void_t;
std::ostream&operator<<(std::ostream&os,tpool::void_t const&){return os<<'.';}
}
#endif
