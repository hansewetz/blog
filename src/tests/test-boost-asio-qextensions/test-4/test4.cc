// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

/*
this program has one queue with a thread doing sync_enq and an asio in main thread doing async_deq

*/

#include <boost/asio_queue.hpp>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <boost/lexical_cast.hpp>
#include <string>
#include <memory>
#include <thread>
#include <functional>
using namespace std;
using namespace std::placeholders;

// queue sending/receiving max #of messages
// asio queue stuff
using queue_t=boost::asio::simple_queue<int>;
shared_ptr<queue_t>q{new queue_t(3)};
boost::asio::io_service ios;
boost::asio::queue_listener<queue_t>qlistener(::ios,q.get());
boost::asio::queue_sender<queue_t>qsender(::ios,q.get());

// max #of messages to send/receive
constexpr size_t maxmsg{10};

// handler for queue listener
size_t nreceived{0};
template<typename T>
void qlistener_handler(boost::system::error_code const&ec,T item){
  BOOST_LOG_TRIVIAL(debug)<<"received item in qlistener_handler (via asio), item: "<<item<<", ec: "<<ec.message();
  if(++nreceived<maxmsg)qlistener.async_deq(qlistener_handler<T>);
}
// thread function sending maxmsg messages
void thr_send_sync_messages(){
  for(std::size_t i=0;i<maxmsg;++i){
    int item{boost::lexical_cast<int>(i)};
    BOOST_LOG_TRIVIAL(debug)<<"sending item: "<<item;
    boost::system::error_code ec;
    qsender.sync_enq(item,ec);
  }
}
// test program
int main(){
  try{
    // listen/send on messages on q1
    qlistener.async_deq(qlistener_handler<int>);

    // kick off sender thread
    thread thr(thr_send_sync_messages);

    // kick off io service
    ::ios.run();

    // join thread
    thr.join();
  }
  catch(exception const&e){
    BOOST_LOG_TRIVIAL(error)<<"cought exception: "<<e.what();
  }
}
