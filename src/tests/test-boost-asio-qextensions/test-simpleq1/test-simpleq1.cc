// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

/*
This program tests that simple_queue::timed_enq() works
*/

#include <boost/asio_queue.hpp>
#include <boost/log/trivial.hpp>
#include <string>
using namespace std;
namespace asio=boost::asio;

// test program
int main(){
  try{
    // create queue
    std::size_t tmo{5000};
    size_t maxsize{1};
    asio::simple_queue<int>q{maxsize};

    // insert an element in queue
    BOOST_LOG_TRIVIAL(debug)<<"enqing one item ...";
    boost::system::error_code ec;
    bool stat1=q.timed_enq(17,tmo,ec);
    BOOST_LOG_TRIVIAL(debug)<<"enq(): "<<boolalpha<<stat1<<", ec: "<<ec.message();

    // kick off thread popping an element afetr 2 second
    std::function<void(void)>ft=[&](){
        // sleep 2 seconds before deq()
        std::this_thread::sleep_for(std::chrono::milliseconds(6000));
        boost::system::error_code ec;
        pair<bool,int>p=q.deq(ec);
        BOOST_LOG_TRIVIAL(debug)<<"enq(): "<<boolalpha<<p.first<<", "<<p.second<<", ec: "<<ec.message();
      };
    std::thread thr{ft};

    BOOST_LOG_TRIVIAL(debug)<<"enq wait second item ...";
    bool stat2=q.timed_wait_enq(tmo,ec);
    BOOST_LOG_TRIVIAL(debug)<<"enq_wait(): "<<boolalpha<<stat2<<", ec: "<<ec.message();

    // join thread
    thr.join();
  }
  catch(exception const&e){
    BOOST_LOG_TRIVIAL(error)<<"cought exception: "<<e.what();
  }
}
