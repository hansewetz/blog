// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

/*
this program has one queue with a thread doing sync_enq and an asio in main thread doing async_deq
this is the same program as test4.cc but using a polldir_queue instead of a simple_queue

*/

#include <boost/asio_queue.hpp>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <string>
#include <memory>
#include <thread>
#include <functional>
#include <iostream>
using namespace std;
using namespace std::placeholders;

namespace asio= boost::asio;
namespace fs=boost::filesystem;

// test queue item
class Message{
public:
  friend ostream&operator<<(ostream&os,Message const&junk){
    return os<<"["<<junk.s_<<","<<junk.i_<<"]";
  }
  Message()=default;
  Message(string const&s,int i):s_(s),i_(i){}
  Message(Message const&)=default;
  Message(Message&&)=default;
  Message&operator=(Message const&)=default;
  Message&operator=(Message&&)=default;

  // getters
  string const&gets()const{return s_;}
  int geti()const{return i_;}
private:
  string s_;
  int i_;
};
// timeout for deq()
size_t tmo_ms{3000};

// create sender and listener queues (could be the same queue)
function<Message(istream&)>deserialiser=[](istream&is){
  string line1;
  getline(is,line1);
  string line2;
  getline(is,line2);
  return Message(line1,boost::lexical_cast<int>(line2));
};
function<void(ostream&,Message const&)>serialiser=[](ostream&os,Message const&m){ 
  os<<m.gets()<<endl;
  os<<m.geti()<<endl;
};

// value type in queues
// (must work with operator<< and operator>>, and be default constructable)
using qval_t=Message;

string qname{"q1"};
fs::path qdir{"./q1"};
using queue_t=asio::polldir_queue<qval_t,decltype(deserialiser),decltype(serialiser)>;
queue_t qrecv{qname,0,qdir,deserialiser,serialiser,true};
queue_t qsend{qname,0,qdir,deserialiser,serialiser,true};

// setup asio object
asio::io_service ios;
asio::queue_listener<queue_t>qlistener(::ios,&qrecv);
asio::queue_sender<queue_t>qsender(::ios,&qsend);

// max #of messages to send/receive
constexpr size_t maxmsg{3};

// handler for queue listener
void qlistener_handler(boost::system::error_code const&ec,qval_t const&item){
  if(ec!=0){
    BOOST_LOG_TRIVIAL(debug)<<"deque() aborted (via asio), ec: "<<ec.message();
  }else{
    BOOST_LOG_TRIVIAL(debug)<<"received item in qlistener_handler (via asio), item: "<<item<<", ec: "<<ec;
    qlistener.timed_async_deq(qlistener_handler,tmo_ms);
  }
}
// thread function sending maxmsg messages
void thr_send_sync_messages(){
  for(int i=0;i<static_cast<int>(maxmsg);++i){
    qval_t item{Message{string("Hella World")+boost::lexical_cast<string>(i),i}};
    BOOST_LOG_TRIVIAL(debug)<<"sending item: "<<item;
    boost::system::error_code ec;
    qsender.sync_enq(item,ec);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}
// test program
int main(){
  try{
    // remove locks for queue
    qrecv.removeLockVariables(qrecv.qname());

    // listen for on messages on q1 (using asio)
    BOOST_LOG_TRIVIAL(debug)<<"starting async_deq() ...";
    qlistener.timed_async_deq(qlistener_handler,tmo_ms);

    // kick off sender thread
    BOOST_LOG_TRIVIAL(debug)<<"starting thread sender thread ...";
    thread thr(thr_send_sync_messages);

    // kick off io service
    BOOST_LOG_TRIVIAL(debug)<<"starting asio ...";
    ::ios.run();

    // join thread
    BOOST_LOG_TRIVIAL(debug)<<"joining with sender thread ...";
    thr.join();
  }
  catch(exception const&e){
    BOOST_LOG_TRIVIAL(error)<<"cought exception: "<<e.what();
  }
}
