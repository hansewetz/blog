#include <boost/chrono.hpp>
#include <boost/ratio.hpp>
namespace utils{

#ifdef BOOST_CHRONO_HAS_PROCESS_CLOCKS 
#include <boost/chrono/process_cpu_clocks.hpp> 
#endif

#ifdef BOOST_CHRONO_HAS_THREAD_CLOCK 
#include <boost/chrono/thread_clock.hpp>
#endif

// old fashioned stopwatch
template<typename C,typename R=double>
class stopwatch{
public:
  // ctor, assign, dtor
  stopwatch()=default;
  stopwatch(stopwatch const&)=default;
  stopwatch(stopwatch&&)=default;
  stopwatch&operator=(stopwatch const&)=default;
  stopwatch&operator=(stopwatch&&)=default;
  ~stopwatch()=default;

  // start/top and reset
  void click(){fsm_(Event::click);}
  void reset(){fsm_(Event::reset);}

  // get elapsed time
  R getElapsedTimeNs()const{return getElapsedTime<boost::chrono::duration<R,boost::nano>>();}
  R getElapsedTimeUs()const{return getElapsedTime<boost::chrono::duration<R,boost::micro>>();}
  R getElapsedTimeMs()const{return getElapsedTime<boost::chrono::duration<R,boost::milli>>();}
  R getElapsedTimeSec()const{return getElapsedTime<boost::chrono::duration<R,boost::ratio<1>>>();}
  R getElapsedTimeMin()const{return getElapsedTime<boost::chrono::duration<R,boost::ratio<60>>>();}
  R getElapsedTimeHours()const{return getElapsedTime<boost::chrono::duration<R,boost::ratio<3600>>>();}
  R getElapsedTimeDays()const{return getElapsedTime<boost::chrono::duration<R,boost::ratio<3600*24>>>();}
  R getElapsedTimeWeeks()const{return getElapsedTime<boost::chrono::duration<R,boost::ratio<3600*24*7>>>();}

  // get state
  bool isRunning()const{return state_==State::running;}
  bool isStopped()const{return !isRunning();}
private:
  // typedefs and states
  using TP=typename C::time_point;
  using DURATION=typename C::duration;
  enum class Event:int{click=0,reset=1};
  enum class State:int{running=0,stopped=1};

  // state of stop watch
  C clock_;
  TP startTime_=TP();
  DURATION accumDuration_=DURATION::zero();
  State state_=State::stopped;

  // general method to read elapsed time expressed in type R units of duration D
  template<typename D>
  R getElapsedTime()const{
    DURATION currDuration{accumDuration_};
    if(state_==State::running)currDuration+=clock_.now()-startTime_;
    return boost::chrono::duration_cast<D>(currDuration).count();
  }
  // state machine driving clock - returns current duration
  void fsm_(Event evnt){
    if(state_==State::running){
      if(evnt==Event::click){
        accumDuration_+=(clock_.now()-startTime_);
        state_=State::stopped;
      }else{
        accumDuration_=DURATION::zero();
        startTime_=clock_.now();
      }
    }else{
      if(evnt==Event::click){
        state_=State::running;
        startTime_=clock_.now();
      }else{
        accumDuration_=DURATION::zero();
        startTime_=TP();
      }
    }
  }
};  
// commonly used stop watches
using system_stopwatch= stopwatch<boost::chrono::system_clock,double>;
using hr_stopwatch=stopwatch<boost::chrono::high_resolution_clock,double>;
using steady_stopwatch=stopwatch<boost::chrono::steady_clock,double>;

// Cpu clocks
#ifdef BOOST_CHRONO_HAS_PROCESS_CLOCKS
using proc_real_cpu_stopwatch=stopwatch<boost::chrono::process_real_cpu_clock,double>;
using proc_user_cpu_stopwatch=stopwatch<boost::chrono::process_user_cpu_clock,double>;
using proc_system_cpu_stopwatch=stopwatch<boost::chrono::process_system_cpu_clock,double>;
using proc_cpu_stopwatch=stopwatch<boost::chrono::process_cpu_clock,double>;
#endif
#ifdef BOOST_CHRONO_HAS_THREAD_CLOCK 
using thread_stopwatch=stopwatch<boost::chrono::thread_clock,double>;
#endif
}
