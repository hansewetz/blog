// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

#include <boost/asio_queue.hpp>
#include <boost/log/trivial.hpp>
#include <string>
#include <thread>
#include <iostream>
#include <functional>
#include <memory>
#include <unistd.h>

using namespace std;
using namespace std::placeholders;
namespace asio=boost::asio;

// controll if client or server is sender/receiver
//#define SERVER_SENDER 

// ----- some constants -----
namespace {
size_t msgcount{0};
constexpr size_t maxmsg{10};
constexpr size_t tmo_deq_ms{2000};
constexpr size_t tmo_enq_ms{100};
constexpr size_t tmo_between_send{10};
}
// server debug
constexpr int listenport=7787;
string const server{"localhost"};

// message separator
constexpr char sep='|';

// ----- queue types -----

// aios io service
asio::io_service ios;

// queue type
using qval_t=string;

// serialize an object (notice: message boundaries are on '\n' characters)
std::function<void(std::ostream&,qval_t const&)>serialiser=[](std::ostream&os,qval_t const&s){
  os<<s;
};
// de-serialize an object (notice: message boundaries are on '\n' characters)
std::function<qval_t(istream&)>deserialiser=[](istream&is){
  string line;
  getline(is,line,sep);
  return line;
};
// queue types
using qbase_t=boost::asio::detail::base::queue_interface_base<qval_t>;

//  ------ asio objects, sender, callback handler etc. ---
// handler for queue listener
template<typename T>
void qlistener_handler(boost::system::error_code const&ec,T msg,asio::queue_listener<qbase_t>*ql){
  if(ec!=0){
    BOOST_LOG_TRIVIAL(debug)<<"deque() aborted (via asio), ec: "<<ec.message();
  }else{
    BOOST_LOG_TRIVIAL(debug)<<"received msg in qlistener_handler (via asio), msg: \""<<msg<<"\", ec: "<<ec;
    ql->timed_async_deq(std::bind(qlistener_handler<T>,_1,_2,ql),tmo_deq_ms);
  }
}
// handler for waiting for starting to listen to messages
template<typename T>
void qlistener_waiter_handler(boost::system::error_code const&ec,asio::queue_listener<qbase_t>*ql){
  if(ec!=0){
    BOOST_LOG_TRIVIAL(debug)<<"deque-wait() aborted (via asio), ec: "<<ec.message();
  }else{
    BOOST_LOG_TRIVIAL(debug)<<"an asio message waiting ...";
    ql->timed_async_deq(std::bind(qlistener_handler<T>,_1,_2,ql),tmo_deq_ms);
  }
}
// handler for queue sender
void qsender_handler(boost::system::error_code const&ec,asio::queue_sender<qbase_t>*qs){
  // print item if error code is OK
  if(ec)BOOST_LOG_TRIVIAL(debug)<<"queue sender interupted (via asio): ignoring callback, ec: "<<ec;
  else{
    // check if we are done
    if(msgcount==maxmsg)return;

    // sent next message asynchrounously
    qval_t newmsg{boost::lexical_cast<string>(msgcount++)};
    BOOST_LOG_TRIVIAL(debug)<<"sending message: \""<<newmsg<<"\"";
    qs->timed_async_enq(newmsg,std::bind(qsender_handler,_1,qs),tmo_enq_ms);
    std::this_thread::sleep_for(std::chrono::milliseconds(tmo_between_send));
  }
}
// handler for waiting for starting sending messages
void qsender_waiter_handler(boost::system::error_code const&ec,asio::queue_sender<qbase_t>*qs){
  if(ec!=0){
    BOOST_LOG_TRIVIAL(debug)<<"enq-wait() aborted (via asio), ec: "<<ec.message();
  }else{
    BOOST_LOG_TRIVIAL(debug)<<"it's now possible to send messages ...";

    // kick off async message sender
    qval_t newmsg{boost::lexical_cast<string>(msgcount++)};
    BOOST_LOG_TRIVIAL(debug)<<"sending message: \""<<newmsg<<"\"";
    qs->timed_async_enq(newmsg,std::bind(qsender_handler,_1,qs),tmo_enq_ms);
  }
}
// ------ test program
int main(){
  try{
    // create queues
    asio::sockclient_queue<qval_t,decltype(deserialiser),decltype(serialiser),qbase_t>qclient0(server,listenport,deserialiser,serialiser,sep);
    BOOST_LOG_TRIVIAL(debug)<<"client queue created ...";
    asio::sockserv_queue<qval_t,decltype(deserialiser),decltype(serialiser),qbase_t>qserv0(listenport,deserialiser,serialiser,sep);
    BOOST_LOG_TRIVIAL(debug)<<"server queue created ...";

    // test using base classes
    qbase_t*qclient=&qclient0;
    qbase_t*qserv=&qserv0;

    // setup asio object
#ifdef SERVER_SENDER
    asio::queue_sender<qbase_t>qsender(::ios,qserv);
    asio::queue_listener<qbase_t>qlistener(::ios,qclient);
#else
    asio::queue_listener<qbase_t>qlistener(::ios,qserv);
    asio::queue_sender<qbase_t>qsender(::ios,qclient);
#endif
    // wait tmo_enq_ms ms until we can send a message
    qval_t msg{boost::lexical_cast<string>(msgcount++)};
    BOOST_LOG_TRIVIAL(debug)<<"waiting until we can send messages ... ";
    qsender.timed_async_wait_enq(std::bind(qsender_waiter_handler,_1,&qsender),tmo_enq_ms);

    // wait for at most tmo_deq_ms ms for a message to become available
    BOOST_LOG_TRIVIAL(debug)<<"starting waiting for asio message ...";
    qlistener.timed_async_wait_deq(std::bind(qlistener_waiter_handler<qval_t>,_1,&qlistener),tmo_deq_ms);

    BOOST_LOG_TRIVIAL(debug)<<"starting asio ...";
    ::ios.run();
  }
  catch(exception const&e){
    BOOST_LOG_TRIVIAL(error)<<"cought exception: "<<e.what();
  }
}
