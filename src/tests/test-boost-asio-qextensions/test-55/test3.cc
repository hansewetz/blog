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
namespace fs=boost::filesystem;

// asio io_service
boost::asio::io_service ios;

// setup queue data
int maxmsg{3};
string qname{"q1"};
fs::path qdir{"q1"};

function<int(istream&)>reader=[](istream&is){int ret;is>>ret;return ret;};
function<void(ostream&,int)>writer=[](ostream&os,int i){os<<i;};
using queue_t=boost::asio::polldir_queue<int,decltype(reader),decltype(writer)>;
queue_t q1{qname,10,qdir,reader,writer,true};

// asio stuff
boost::asio::queue_listener<queue_t>qlistener(::ios,&q1);
boost::asio::queue_sender<queue_t>qsender(::ios,&q1);
boost::asio::deadline_timer timer(::ios,boost::posix_time::milliseconds(5000));

// wait handler
void deq_wait(boost::system::error_code const&ec){
  BOOST_LOG_TRIVIAL(debug)<<"GOT WAIT MESSAGE - ec: "<<ec;
  for(int i=0;i<maxmsg;++i){
    boost::system::error_code ec;
    pair<bool,int>p=qlistener.sync_deq(ec);
    BOOST_LOG_TRIVIAL(debug)<<"GOT message: "<<boolalpha<<p.first<<", "<<p.second;
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
    // remove locks if they exist
    q1.removeLockVariables(q1.qname());

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
