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

// server constants
constexpr int listenport=7787;
string const server{"localhost"};
constexpr std::size_t maxclients{5};
constexpr std::size_t tmoPollMs{1000};

// ----- queue types -----

// aios io service
asio::io_service ios;

// some typedefs
using qval_t=string;

// de-serialize an object (notice: message boundaries are on '\n' characters)
std::function<qval_t(istream&)>deserialiser=[](istream&is){
  string line;
  getline(is,line);
  return line;
};

// queue type
using qbase_t=boost::asio::detail::base::queue_interface_deq<qval_t>;
using queue_t=boost::asio::sockdeq_serv_queue<qval_t,decltype(deserialiser),qbase_t>;

// #ms to wait for a message
constexpr std::size_t tmo_deq_ms{10000};

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
// ------ test program
int main(){
  try{
    // create queues
    queue_t qserv0(listenport,deserialiser,maxclients,tmoPollMs);
    qbase_t*qserv=&qserv0;
    BOOST_LOG_TRIVIAL(debug)<<"server queue created ...";

    // wait for at most tmo_deq_ms ms for a message to become available
    BOOST_LOG_TRIVIAL(debug)<<"starting waiting for asio message ...";
    asio::queue_listener<qbase_t>qlistener(::ios,qserv);
    qlistener.timed_async_deq(std::bind(qlistener_handler<qval_t>,_1,_2,&qlistener),tmo_deq_ms);

    BOOST_LOG_TRIVIAL(debug)<<"starting asio ...";
    ::ios.run();
    BOOST_LOG_TRIVIAL(debug)<<"asio done ...";
  }
  catch(exception const&e){
    BOOST_LOG_TRIVIAL(error)<<"cought exception: "<<e.what();
  }
}
