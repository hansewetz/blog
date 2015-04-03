// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

/*
the program has two queues.

there is a queue sender and a queue listener on each queue
there is a timer which triggers starting listening on messwges on the first queue - before this the queue is populated by a sender but blocks after 3 messages which is the queues capacity
this tests that enq() can block when the queue is full and trigger a callback when there is room to send a message in the queue

the second queue has a sender and a listener which contineously runs.
a timer runs which when popped disables enq and deq on the second queue

there is also a fast timer which pops every 100 ms and is disabled when th enq and deq on the second queue is disabled
*/

#include <boost/asio_queue.hpp>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <string>
#include <memory>
#include <thread>
using namespace std;
using namespace std::placeholders;

// ----------------- queue sending/receiving max #of messages
// asio queue stuff
using queue_t=boost::asio::simple_queue<int>;
shared_ptr<queue_t>q{new queue_t(3)};
boost::asio::io_service ios;
boost::asio::queue_listener<queue_t>qlistener(::ios,q.get());
boost::asio::queue_sender<queue_t>qsender(::ios,q.get());

// max #of messages to send/receive
constexpr size_t maxmsg{10};

// handler for queue listener
template<typename T>
void qlistener_handler(boost::system::error_code const&ec,T item, std::size_t nreceived){
  // print item if error code is OK
  if(ec)BOOST_LOG_TRIVIAL(debug)<<"queue listener interupted (via asio): ignoring callback queue item, ec: "<<ec;
  else{
    BOOST_LOG_TRIVIAL(debug)<<"received item in qlistener_handler (via asio), item: "<<item<<", ec: "<<ec;
    if(nreceived+1<maxmsg)qlistener.async_deq(std::bind(qlistener_handler<T>,_1,_2,nreceived+1));
  }
}
// handler for queue sender
template<typename T>
void qsender_handler(boost::system::error_code const&ec,int item,std::size_t nsent){
  // print item if error code is OK
  if(ec)BOOST_LOG_TRIVIAL(debug)<<"queue sender interupted (via asio): ignoring callback, ec: "<<ec;
  else{
    BOOST_LOG_TRIVIAL(debug)<<"sending item in qlistener_handler (via asio), item: "<<item<<", ec: "<<ec;
    if(nsent+1<maxmsg)qsender.async_enq(item+1,std::bind(qsender_handler<T>,_1,item+1,nsent+1));
  }
}

// ----------------- queue (q1) sending/receiving until a timer pops
//  asio queue stuff
shared_ptr<queue_t>q1{new queue_t(1000)};
boost::asio::queue_listener<queue_t>qlistener1(::ios,q1.get());
boost::asio::queue_sender<queue_t>qsender1(::ios,q1.get());

// handler for queue listener
template<typename T>
void qlistener1_handler(boost::system::error_code const&ec,T item){
  if(ec)BOOST_LOG_TRIVIAL(debug)<<"queue listener1 interupted (via asio): ignoring callback queue item, ec: "<<ec;
  else{
    BOOST_LOG_TRIVIAL(debug)<<"received item in qlistener1_handler (via asio), item: "<<item<<", ec: "<<ec;
    qlistener1.async_deq(std::bind(qlistener1_handler<T>,_1,_2));
  }
}
// handler for queue sender
template<typename T>
void qsender1_handler(boost::system::error_code const&ec,int item){
  // print item if error code is OK
  if(ec)BOOST_LOG_TRIVIAL(debug)<<"queue sender1 interupted (via asio): ignoring callback, ec: "<<ec;
  else{
    BOOST_LOG_TRIVIAL(debug)<<"sending item in qlistener1_handler (via asio), item: "<<item<<", ec: "<<ec;
    qsender1.async_enq(item+1,std::bind(qsender1_handler<T>,_1,item+1));
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }
}

// test program
int main(){
  try{
    // --------------- setup a timer ticking once per 100 ms, stop when tmo1 disables enq, deq
    std::atomic<bool>runticker{true};
    boost::asio::deadline_timer ticker(::ios,boost::posix_time::milliseconds(100));
    std::function<void(const boost::system::error_code&)>fticker=[&](const boost::system::system_error&ec){BOOST_LOG_TRIVIAL(debug)<<"TICK";if(runticker)ticker.async_wait(fticker);};
    ticker.async_wait(fticker);

    // --------------- setup queue which sends/receives a specific #if messages

    // setup timer to trigger in 3 seconds after which we start listening to messages
    boost::asio::deadline_timer tmo(::ios,boost::posix_time::seconds(1));
    tmo.async_wait([&](const boost::system::error_code&ec){qlistener.async_deq(std::bind(qlistener_handler<int>,_1,_2,0));});

    // start sending messages
    int startItem{1};
    qsender.async_enq(startItem,std::bind(qsender_handler<int>,_1,startItem,0));

    // --------------- setup queue which sends/receives messageses until a timer pops

    // setup timer for test - when timer pops, we disable enq and deq
    boost::asio::deadline_timer tmo1(::ios,boost::posix_time::seconds(2));
    tmo1.async_wait([&](const boost::system::error_code&ec){q1->disable_enq(true);q1->disable_deq(true);runticker=false;});

    // listen/send on messages on q1
    qlistener1.async_deq(std::bind(qlistener1_handler<int>,_1,_2));
    qsender1.async_enq(100,std::bind(qsender1_handler<int>,_1,100));

    // --------------- kick off io service
    ::ios.run();
  }
  catch(exception const&e){
    BOOST_LOG_TRIVIAL(error)<<"cought exception: "<<e.what();
  }
}
