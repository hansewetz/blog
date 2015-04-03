// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

/*
This program tests that simple_queue::timed_deq() works
*/

#include <boost/asio_queue.hpp>
#include <boost/log/trivial.hpp>
#include <string>
#include <chrono>
using namespace std;
namespace asio=boost::asio;

// test program
int main(){
  try{
    // create queue
    std::size_t tmo{5000};
    size_t maxsize{1};
    asio::simple_queue<int>q{maxsize};

    // kick off a thread which enqueues a message
    std::function<void()>f=[&](){
      // sleep 2 seconds before deq()
      std::this_thread::sleep_for(std::chrono::milliseconds(6000));
      boost::system::error_code ec;
      q.enq(17,ec);
      BOOST_LOG_TRIVIAL(debug)<<"enqueued an item, ec: "<<ec.message();
    };
    std::thread thr(f);

    // insert an element in queue
    BOOST_LOG_TRIVIAL(debug)<<"timed deq() ...";
    boost::system::error_code ec;
    bool res=q.timed_wait_deq(tmo,ec);
    BOOST_LOG_TRIVIAL(debug)<<"deq_wait(): "<<boolalpha<<res<<", ec: "<<ec.message();

    // join thread
    thr.join();
  }
  catch(exception const&e){
    BOOST_LOG_TRIVIAL(error)<<"cought exception: "<<e.what();
  }
}
