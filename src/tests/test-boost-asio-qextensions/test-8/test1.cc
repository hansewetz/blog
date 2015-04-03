// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

/*
This program tests that 'socketserv_queue' works
*/

#include <boost/asio_queue.hpp>
#include <boost/log/trivial.hpp>
#include <functional>
#include <string>
#include <functional>
#include <iostream>
#include <thread>
#include <future>
using namespace std;
namespace asio=boost::asio;

// queue stuff
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
// send message on queue
template<typename Q>
void sendMsg(Q*q,qval_t const&msg){
  boost::system::error_code ec1;
  q->enq(msg,ec1);
  if(ec1!=boost::system::error_code()){
    BOOST_LOG_TRIVIAL(error)<<"enq() failed: "<<ec1.message();
    exit(1);
 }
}
// receive a message on a queue
template<typename Q>
qval_t recvMsg(Q*q){
  boost::system::error_code ec1;

  // wait until we can deq
  BOOST_LOG_TRIVIAL(info)<<"waiting until deq() possible ...";
  q->timed_wait_deq(100,ec1);
  if(ec1!=boost::system::error_code()){
    BOOST_LOG_TRIVIAL(error)<<"wait_deq() failed: "<<ec1.message();
    exit(1);
  }
  // deq item
  BOOST_LOG_TRIVIAL(info)<<"can now deq()";
  pair<bool,qval_t>ret1{q->deq(ec1)};
  if(ec1!=boost::system::error_code()){
    BOOST_LOG_TRIVIAL(error)<<"deq() failed: "<<ec1.message();
    exit(1);
  }
  return ret1.second;
}
// server info
constexpr int listenport=7787;
string const server{"localhost"};

// test program
int main(){
  try{
    // create queues
    // (after creation, the queue will accept client connections and act as a full duplex queue with each client)
    // (notice that we can create client first since it will not connect until an enq/deq method is invoked on it)
    asio::sockclient_queue<qval_t,decltype(deserialiser),decltype(serialiser)>qclient(server,listenport,deserialiser,serialiser);
    BOOST_LOG_TRIVIAL(info)<<"client queue created ...";
    asio::sockserv_queue<qval_t,decltype(deserialiser),decltype(serialiser)>qserv(listenport,deserialiser,serialiser);
    BOOST_LOG_TRIVIAL(info)<<"server queue created ...";

    // send a message
    string msg{"A message"};
    BOOST_LOG_TRIVIAL(info)<<"sending message: \""<<msg<<"\" through server queue";
    using qsend_t=decltype(qserv);
    std::function<void(qsend_t*,string const&)>fsend=[](qsend_t*q,string const&msg){sendMsg<qsend_t>(q,msg);};
    std::thread tsend{fsend,&qserv,msg};
    BOOST_LOG_TRIVIAL(info)<<"message sent on server queue";

    // listen for a message
    BOOST_LOG_TRIVIAL(info)<<"listening to message on client queue";
    using qrecv_t=decltype(qclient);
    std::packaged_task<qval_t(qrecv_t*)>task([](qrecv_t*q){return recvMsg(q);});
    std::future<qval_t>fut=task.get_future();
    std::thread trecv(std::move(task),&qclient);
    BOOST_LOG_TRIVIAL(info)<<"got message: \""<<fut.get()<<"\" on client queue";

    // join threads
    trecv.join();
    tsend.join();
  }
  catch(exception const&e){
    BOOST_LOG_TRIVIAL(error)<<"cought exception: "<<e.what();
  }
}
