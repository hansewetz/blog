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
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>

using namespace std;
using namespace std::placeholders;
namespace asio=boost::asio;
//namespace io=boost::iostreams;

// ----- some constants -----
namespace {
size_t msgcount{0};
constexpr size_t maxmsg{10};
constexpr size_t tmo_deq_ms{2000};
constexpr size_t tmo_enq_ms{1000};
constexpr size_t tmo_between_send{10};
}

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
  getline(is,line);
  return line;
};
// queue types
using enq_t=asio::fdenq_queue<qval_t,decltype(serialiser)>;
using deq_t=asio::fddeq_queue<qval_t,decltype(deserialiser)>;

//  ------ asio objects, sender, callback handler etc. ---

// handler for queue listener
template<typename T>
void qlistener_handler(boost::system::error_code const&ec,T msg,asio::queue_listener<deq_t>*ql){
  if(ec!=0){
    BOOST_LOG_TRIVIAL(debug)<<"deque() aborted (via asio), ec: "<<ec.message();
  }else{
    BOOST_LOG_TRIVIAL(debug)<<"received msg in qlistener_handler (via asio), msg: \""<<msg<<"\", ec: "<<ec;
    ql->timed_async_deq(std::bind(qlistener_handler<T>,_1,_2,ql),tmo_deq_ms);
  }
}
// handler for waiting for starting to listen to messages
template<typename T>
void qlistener_waiter_handler(boost::system::error_code const&ec,asio::queue_listener<deq_t>*ql){
  if(ec!=0){
    BOOST_LOG_TRIVIAL(debug)<<"deque-wait() aborted (via asio), ec: "<<ec.message();
  }else{
    BOOST_LOG_TRIVIAL(debug)<<"an asio message waiting ...";
    ql->timed_async_deq(std::bind(qlistener_handler<T>,_1,_2,ql),tmo_deq_ms);
  }
}
// handler for queue sender
void qsender_handler(boost::system::error_code const&ec,asio::queue_sender<enq_t>*qs){
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
void qsender_waiter_handler(boost::system::error_code const&ec,asio::queue_sender<enq_t>*qs){
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
    // create an fd-pipe with connected read/write fds
    int fd[2];
    if(pipe(fd)!=0){
      BOOST_LOG_TRIVIAL(error)<<"pipe failed";
      exit(1);
    }
    int fdread{fd[0]};
    int fdwrite{fd[1]};

    // create queues
    boost::system::error_code ec;
    bool closeFdsOnExit{true};
    enq_t qin{fdwrite,serialiser,closeFdsOnExit};
    deq_t qout{fdread,deserialiser,closeFdsOnExit};

    // make sure move ctor works
    enq_t qin1{std::move(qin)};
    deq_t qout1{std::move(qout)};

    // make sure move assign works
    qin=std::move(qin1);
    qout=std::move(qout1);

    // setup asio object
    asio::queue_sender<enq_t>qsender(::ios,&qin);
    asio::queue_listener<deq_t>qlistener(::ios,&qout);

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
