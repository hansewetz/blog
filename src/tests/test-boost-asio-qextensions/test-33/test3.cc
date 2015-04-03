// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

/*
This program tests the wait_deq() functionality on a queue_sender.
*/

#include <boost/asio_queue.hpp>
#include <boost/asio.hpp>
#include <boost/asio/posix/basic_descriptor.hpp>
#include <boost/log/trivial.hpp>
#include <string>
#include <memory>
using namespace std;
using namespace std::placeholders;

// asio io_service
boost::asio::io_service ios;

// asio stuff
int maxmsg{3};
using queue_t=boost::asio::simple_queue<int>;
shared_ptr<queue_t>q{new queue_t(maxmsg)};
boost::asio::queue_listener<queue_t>qlistener(::ios,q.get());
boost::asio::queue_sender<queue_t>qsender(::ios,q.get());
boost::asio::deadline_timer timer(::ios,boost::posix_time::milliseconds(5000));

// wait handler
void deq_wait(boost::system::error_code const&ec){
  BOOST_LOG_TRIVIAL(debug)<<"GOT WAIT MESSAGE - ec: "<<ec.message();
  for(int i=0;i<maxmsg;++i){
    boost::system::error_code ec;
    pair<bool,int>p=qlistener.sync_deq(ec);
    BOOST_LOG_TRIVIAL(debug)<<"GOT message: "<<boolalpha<<p.first<<", "<<p.second<<", ec: "<<ec.message();
  }
}
// timer handler
void ftimer(boost::system::error_code const&ec){
  BOOST_LOG_TRIVIAL(debug)<<"TICK - sending messages ...";
  for(int i=0;i<maxmsg;++i){
    boost::system::error_code ec;
    qsender.sync_enq(i,ec);
  }
}
// test program
int main(){
  try{
    // setup to wait for deq
    BOOST_LOG_TRIVIAL(debug)<<"WAITING for messages ... ";
    qlistener.async_wait_deq(deq_wait);

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
