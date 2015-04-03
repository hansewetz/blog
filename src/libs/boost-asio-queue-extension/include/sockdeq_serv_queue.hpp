// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

#ifndef __SOCK_DEQ_SERV_QUEUE_H
#define __SOCK_DEQ_SERV_QUEUE_H
#include "detail/queue_empty_base.hpp"
#include "detail/sockqueue_support.hpp"
#include <boost/asio/error.hpp>
#include <string>
#include <utility>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <algorithm>
#include <atomic>
#include <string.h>

// socket stuff
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

// boost stuff
#include <boost/lexical_cast.hpp>

namespace boost{
namespace asio{

/*
  The class implements a socket server queue.
  Ones constructed the server queue runs a separate thread accepting and reading data from clients.
  The thread terminates when the destructor is executed.
  Currently there are no methods for stopping/starting the queue - even though it should not be difficult to implement

  The queue is not designed/implemented in a very clever way - it's more of a brute firce implementation
  Possibly the design and implementation should be re-thought.
*/
template<typename T,typename DESER,typename Base=detail::base::queue_empty_base<T>,typename Container=std::queue<T>>
class sockdeq_serv_queue:public Base{
public:
  // default message separaor
  constexpr static char NEWLINE='\n';

  // ctor
  sockdeq_serv_queue(int port,DESER deser,std::size_t maxclients,std::size_t tmo_poll_ms,char sep=NEWLINE):
      port_(port),deser_(deser),maxclients_(maxclients),tmo_poll_ms_(tmo_poll_ms),sep_{sep},
      mtx_{std::make_unique<std::mutex>()},cond_{std::make_unique<std::condition_variable>()},stop_server_(false){
    // spawn thread running socket io stuff
    serv_thr_=std::move(std::thread([&](){run_sock_serv();}));
  }
  // copy/move/assign - for simplicity, delete them
  sockdeq_serv_queue(sockdeq_serv_queue const&)=delete;
  sockdeq_serv_queue(sockdeq_serv_queue&&other)=delete;
  sockdeq_serv_queue&operator=(sockdeq_serv_queue const&)=delete;
  sockdeq_serv_queue&operator=(sockdeq_serv_queue&&other)=delete;

  // ctor
  ~sockdeq_serv_queue(){
    // flag to stop server loop and wait for server to stop, then close server socket
    stop_server_.store(true);
    if(serv_thr_.joinable())serv_thr_.join();
    detail::queue_support::eclose(servsocket_,false);
  }
  // dequeue a message
  std::pair<bool,T>deq(boost::system::error_code&ec){
    std::unique_lock<std::mutex>lock(*mtx_);
    cond_->wait(lock,[&](){return !deq_enabled_||!q_.empty();});
  
    // if deq is disabled or timeout
    if(!deq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return std::make_pair(false,T{});
    }
    // check if we have a message
    std::pair<bool,T>ret{std::make_pair(true,q_.front())};
    q_.pop();
    cond_->notify_all();
    ec=boost::system::error_code();
    return ret;
  }
  // dequeue a message (return.first == false if deq() was disabled) - timeout if waiting too long
  std::pair<bool,T>timed_deq(std::size_t ms,boost::system::error_code&ec){
    std::unique_lock<std::mutex>lock(*mtx_);
    bool tmo=!cond_->wait_for(lock,std::chrono::milliseconds(ms),[&](){return !deq_enabled_||!q_.empty();});
  
    // if deq is disabled or queue is empty return or timeout
    if(tmo){
      ec=boost::asio::error::timed_out;
       return std::make_pair(false,T{});
    }
    if(!deq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return std::make_pair(false,T{});
    }
    // check if we have a message
    std::pair<bool,T>ret{std::make_pair(true,q_.front())};
    q_.pop();
    cond_->notify_all();
    ec=boost::system::error_code();
    return ret;
  }
  // wait until we can retrieve a message from queue
  bool wait_deq(boost::system::error_code&ec){
    std::unique_lock<std::mutex>lock(*mtx_);
    cond_->wait(lock,[&](){return !deq_enabled_||q_.size()>0;});
    if(!deq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return false;
    }
    cond_->notify_all();
    ec=boost::system::error_code();
    return true;
  }
  // wait until we can retrieve a message from queue -  timeout if waiting too long
  bool timed_wait_deq(std::size_t ms,boost::system::error_code&ec){
    std::unique_lock<std::mutex>lock(*mtx_);
    bool tmo=!cond_->wait_for(lock,std::chrono::milliseconds(ms),[&](){return !deq_enabled_||q_.size()>0;});
    if(tmo){
      ec=boost::asio::error::timed_out;
      return false;
    }
    if(!deq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return false;
    }
    cond_->notify_all();
    ec=boost::system::error_code();
    return true;
  }
  // cancel deq operations (will also release blocking threads)
  void disable_deq(bool disable){
    std::unique_lock<std::mutex>lock(*mtx_);
    deq_enabled_=!disable;
    cond_->notify_all();
  }
private:
  // --------------------------------- private helper functions

  // callback function creating an object from an istream
  void createItem(std::istream&is){
    std::unique_lock<std::mutex>lock(*mtx_);
    q_.push(deser_(is));
    cond_->notify_all();
  }
  // function running select loop
  void run_sock_serv(){
    // create listening socket (server socket)
    servsocket_=detail::sockqueue_support::createListenSocket(port_,serveraddr_,maxclients_,&yes_);

    // accept client connections and dequeue messages
    detail::sockqueue_support::acceptClientsAndDequeue(servsocket_,sep_,tmo_poll_ms_,stop_server_,[&](std::istream&is){createItem(is);});
  }
  // --------------------------------- private data
  // user specified state for queue
  int port_;                             // port to listen on
  DESER const deser_;                    // de-serialiser
  std::size_t const maxclients_;         // max clients that can connect to this queue
  std::size_t const tmo_poll_ms_;        // ms poll intervall for checking if queue should be stopped
  char const sep_;                       // message separator

  // state of interface to queue
  bool deq_enabled_=true;                // is dequing enabled

  // server socket stuff
  std::thread serv_thr_;                 // thread handling dequeing messages
  int servsocket_;                       // socket on which we are listening
  struct sockaddr_in serveraddr_;        // server address
  int yes_=1;                            // used for setting socket options

  // variables shared across select loop and interface
  Container q_;                                          // queues waiting to be de-queued
  mutable std::unique_ptr<std::mutex>mtx_;               // protects queue where messages are stored
  mutable std::unique_ptr<std::condition_variable>cond_; // ...
  std::atomic<bool>stop_server_;                         // when set to true, then server will stop
};
}
}
#endif
