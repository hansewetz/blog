// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

/*
This program tests the wait_enq() functionality on a queue_sender.
Notice that ones a wait_enq() call has been made, there could be a posibility that no event triggers the handler callback.
For example, if all messages have been dequeued before a the wait_enq() handler has been invoked, asio still has outstanding work.
One way to break this is to issue a disable_enq() directly on the queue.

The code is somehat complicated:
	- insert 3 messages asynchtronously into a queue
	- setup a timer for 5 seconds
	- when all messages handler callback have been called, it sets up a wait_enq() handler
	- when timer pops, it sets up a listener (deq() async handler
	- when the last message (we count) in the deq() callback has been received, we issue a disable_enq on the queue
*/

#include <boost/asio_queue.hpp>
#include <boost/asio.hpp>
#include <boost/asio/posix/basic_descriptor.hpp>
#include <boost/log/trivial.hpp>
#include <string>
#include <utility>
using namespace std;
using namespace std::placeholders;

// asio io_service
boost::asio::io_service ios;

// asio stuff
std::size_t maxmsg{3};
using queue_t=boost::asio::simple_queue<string>;
queue_t q1{maxmsg};

// make sure move ctor works
queue_t q{std::move(q1)};

boost::asio::queue_listener<queue_t>qlistener(::ios,&q);
boost::asio::queue_sender<queue_t>qsender(::ios,&q);
boost::asio::deadline_timer timer(::ios,boost::posix_time::milliseconds(1));

// count #of outstanding messages
std::size_t nmsg{0};

// handler for queue listener
void flisten(boost::system::error_code const&ec,string s){
  // reload listener only if we have more messages
  // (if no more messages, we might still have a wait_enq outstanting)
  if(ec){
    BOOST_LOG_TRIVIAL(debug)<<"dequed message, ec: "<<ec.message();
    return;
  }
  BOOST_LOG_TRIVIAL(debug)<<"dequed message";
  if(--nmsg>0){
    qlistener.async_deq(flisten);
  }else{
    BOOST_LOG_TRIVIAL(debug)<<"disabling enq";
    q.disable_enq(true);
    BOOST_LOG_TRIVIAL(debug)<<"disabling deq";
    q.disable_deq(true);
  }
}
// wait handler
void fwait(boost::system::error_code const&ec){
  BOOST_LOG_TRIVIAL(debug)<<"GOT WAIT MESSAGE - ec: "<<ec.message();
  if(nmsg>0)qlistener.async_deq(flisten);
}
// timer handler
void ftimer(boost::system::error_code const&ec){
  BOOST_LOG_TRIVIAL(debug)<<"TICK - starting async read of 1 message ..., ec: "<<ec.message();
  if(nmsg>0)qlistener.async_deq(flisten);
}
// sender handler
void fsender(boost::system::error_code const&ec){
  // if queue is full, then wait
  if(q.full()){
    BOOST_LOG_TRIVIAL(debug)<<"starting async_wait_enq() ..., ec: "<<ec.message();
    qsender.async_wait_enq(fwait);
  }
}
// test program
int main(){
// NOTE!
queue_t q3{std::move(q1)};
  try{
    // kick off a batch of async message enques
    string item{"Hello"};
    for(;nmsg<maxmsg;++nmsg){
      BOOST_LOG_TRIVIAL(debug)<<"enqueued message ...";
      qsender.async_enq(item,fsender);
    }
    // set timer for then start deq() so wait will trigger on queue
    BOOST_LOG_TRIVIAL(debug)<<"starting timer ...";
    timer.async_wait(ftimer);

    // run asio 
    ::ios.run();
  }
  catch(exception const&e){
    BOOST_LOG_TRIVIAL(error)<<"cought exception: "<<e.what();
  }
}
