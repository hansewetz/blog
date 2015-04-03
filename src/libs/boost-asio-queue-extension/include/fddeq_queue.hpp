// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

/* TODO
IMPROVEMENTS:
	- we should have two timeouts, message timeout, byte timeout
	- read more than one character at a time ... must then buffer what we have read
*/

#ifndef __FDDEQ_QUEUE_H__
#define __FDDEQ_QUEUE_H__
#include "detail/queue_empty_base.hpp"
#include "detail/queue_support.hpp"
#include "detail/fdqueue_support.hpp"
#include <utility>
#include <iostream>
#include <string>
#include <mutex>
#include <sstream>
#include <unistd.h>
#include <boost/asio/error.hpp>

namespace boost{
namespace asio{

// a simple queue based on sending messages separated by '\n'
// (if sending objects first serialise the object, the base64 encode it in the serialiser)
// (the tmo in ms is based on message timeout - if no message starts arriving within timeout, the function times out)
// (ones we have started to read a message, the message will never timeout)
// (the class is meant to be used in singele threaded mode and is not thread safe)
template<typename T,typename DESER,typename Base=detail::base::queue_empty_base<T>>
class fddeq_queue:public Base{
public:
  // default message separaor
  constexpr static char NEWLINE='\n';

  // ctors,assign,dtor
  fddeq_queue(int fdread,DESER deser,bool closeOnExit=false,char sep=NEWLINE):
      fdread_(fdread),deser_(deser),sep_{sep},closeOnExit_(closeOnExit),mtx_{std::make_unique<std::mutex>()}{
  }
  fddeq_queue(fddeq_queue const&)=delete;
  fddeq_queue(fddeq_queue&&other):
      fdread_(other.fdread_),deser_(std::move(other.deser_)),sep_(other.sep_),closeOnExit_(other.closeOnExit_),
      mtx_(std::move(other.mtx_)),deq_enabled_(other.deq_enabled_){
    other.closeOnExit_=false; // make sure we don't close twice
  }
  fddeq_queue&operator=(fddeq_queue const&)=delete;
  fddeq_queue&operator=(fddeq_queue&&other){
    fdread_=other.fdread_;
    deser_=std::move(other.deser_);
    sep_=other.sep_;
    closeOnExit_=closeOnExit_;
    other.closeOnExit_=false; // make sure we don't close twice
    mtx_=std::move(other.mtx_);
    deq_enabled_=other.deq_enabled_;
    return*this;
  }
  ~fddeq_queue(){if(closeOnExit_)detail::queue_support::eclose(fdread_,false);}
  
  // dequeue a message (return.first == false if deq() was disabled)
  std::pair<bool,T>deq(boost::system::error_code&ec){
    // if deq is disabled we are done
    std::unique_lock<std::mutex>lock(*mtx_);
    if(!deq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return std::make_pair(false,T{});
    }
    T ret{detail::queue_support::recvwait<T,DESER>(fdread_,0,ec,true,sep_,deser_)};
    if(ec!=boost::system::error_code())return std::make_pair(false,ret);
    return make_pair(true,ret);
  }
  // dequeue a message (return.first == false if deq() was disabled) - timeout if waiting too long
  std::pair<bool,T>timed_deq(std::size_t ms,boost::system::error_code&ec){
    // if deq is disabled we are done
    std::unique_lock<std::mutex>lock(*mtx_);
    if(!deq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return std::make_pair(false,T{});
    }
    T ret{detail::queue_support::recvwait<T,DESER>(fdread_,ms,ec,true,sep_,deser_)};
    if(ec!=boost::system::error_code())return std::make_pair(false,ret);
    return make_pair(true,ret);
  }
  // wait until we can retrieve a message from queue
  bool wait_deq(boost::system::error_code&ec){
    // if deq is disabled we are done
    std::unique_lock<std::mutex>lock(*mtx_);
    if(!deq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return false;
    }
    detail::queue_support::recvwait<T,DESER>(fdread_,0,ec,false,sep_,deser_);
    if(ec.value()!=0)return false;
    return true;
  }
  // wait until we can retrieve a message from queue -  timeout if waiting too long
  bool timed_wait_deq(std::size_t ms,boost::system::error_code&ec){
    // if deq is disabled we are done
    std::unique_lock<std::mutex>lock(*mtx_);
    if(!deq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return false;
    }
    detail::queue_support::recvwait<T,DESER>(fdread_,ms,ec,false,sep_,deser_);
    if(ec==boost::asio::error::timed_out)return false;
    if(ec.value()!=0)return false;
    return true;
  }
  // cancel deq operations
  void disable_deq(bool disable){
    std::unique_lock<std::mutex>lock(*mtx_);
    deq_enabled_=!disable;
  }
  // get underlying file descriptor
  int getfd()const{
    return fdread_;
  }
private:
  // state of queue
  int fdread_;                           // file descriptors to read from from
  DESER deser_;                          // de-serialiser
  char sep_;                             // message separator
  bool closeOnExit_;                     // close fd on exit
  mutable std::unique_ptr<std::mutex>mtx_;// mutex protected enable flag
  bool deq_enabled_=true;                // is dequing enabled
};
}
}
#endif
