#include "general-tools/stopwatch.h"
#include <iostream>
#include <thread>
using namespace std;
using namespace utils;

#include <sys/times.h>

// test program
int main(){
  thread_stopwatch sw;

  string unit="us";
  std::function<double()>elapsed=[&]()->double{return sw.getElapsedTimeUs();};

  // test starting clock ones then stopping it
  sw.click();
  for(int i=0;i<1000000;++i){
    ++i;
    --i;
  }
  sw.click();
  cerr<<"time in "<<unit<<": "<<elapsed()<<endl;       // read when clock is stopped
  sw.reset();

  sw.click();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  sw.reset();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  sw.click();
  cerr<<"time in "<<unit<<": "<<elapsed()<<endl;       // read when clock is stopped

  // test starting again and make sure we accumulate time
  sw.click();
  this_thread::sleep_for(std::chrono::seconds(1));
  cerr<<"time in "<<unit<<": "<<elapsed()<<endl;       // read while clock is ticking
  sw.click();
  cerr<<"time in "<<unit<<": "<<elapsed()<<endl;       // read when clock is stopped
  sw.click();
  this_thread::sleep_for(std::chrono::seconds(1));
  cerr<<"time in "<<unit<<": "<<elapsed()<<endl;       // read while clock is ticking
  sw.click();
  this_thread::sleep_for(std::chrono::seconds(1));
  cerr<<"time in "<<unit<<": "<<elapsed()<<endl;       // read while clock is stopped
}
