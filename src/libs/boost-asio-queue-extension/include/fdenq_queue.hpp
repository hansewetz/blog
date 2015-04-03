// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

/* TODO
IMPROVEMENTS:
	- we should have two timeouts, message timeout, byte timeout
	- read more than one character at a time ... must then buffer what we have read
	- maybe we should createa stream and serialise directoy into fd - we would have problems with tmos though ...
*/

#ifndef __FDENQ_QUEUE_H__
#define __FDENQ_QUEUE_H__
#include "detail/queue_empty_base.hpp"
#include "detail/queue_support.hpp"
#include "detail/fdqueue_support.hpp"
#include <string>
#include <utility>
#include <mutex>
#include <unistd.h>
#include <boost/asio/error.hpp>

namespace boost{
namespace asio{

// a simple queue based on receiving messages separated by '\n'
// (if recieving objects which are serialised, they should have been serialised and then encoded)
// (the tmo in ms is based on message timeout - if no message starts arriving within timeout, the function times out)
// (ones we have started to send a message, the message will never timeout)
// (the class is meant to be used in singele threaded mode and is not thread safe)
template<typename T,typename SERIAL,typename Base=detail::base::queue_empty_base<T>>
class fdenq_queue:public Base{
public:
  // default message separaor
  constexpr static char NEWLINE='\n';

  // ctors,assign,dtor
  fdenq_queue(int fdwrite,SERIAL serial,bool closeOnExit=false,char sep=NEWLINE):
      fdwrite_(fdwrite),serial_(serial),sep_(sep),closeOnExit_(closeOnExit),mtx_{std::make_unique<std::mutex>()}{
  }
  fdenq_queue(fdenq_queue const&)=delete;
  fdenq_queue(fdenq_queue&&other):
      fdwrite_(other.fdwrite_),serial_(std::move(other.serial_)),sep_(other.sep_),closeOnExit_(other.closeOnExit_),
      mtx_(std::move(other.mtx_)),enq_enabled_(other.enq_enabled_){
    other.closeOnExit_=false; // make sure we don't close twice
  }
  fdenq_queue&operator=(fdenq_queue const&)=delete;
  fdenq_queue&operator=(fdenq_queue&&other){
    fdwrite_=other.fdwrite_;
    serial_=std::move(other.serial_);
    sep_=other.sep_;
    closeOnExit_=other.closeOnExit_;
    other.closeOnExit_=false; // make sure we don't close twice
    mtx_=std::move(other.mtx_);
    enq_enabled_=other.enq_enabled_;
    return*this;
  }
  ~fdenq_queue(){if(closeOnExit_)detail::queue_support::eclose(fdwrite_,false);}
  
  // enqueue a message (return.first == false if enq() was disabled)
  bool enq(T t,boost::system::error_code&ec){
    std::unique_lock<std::mutex>lock(*mtx_);
    if(!enq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return false;
    }
    return detail::queue_support::sendwait<T,SERIAL>(fdwrite_,&t,0,ec,true,sep_,serial_);
  }
  // enqueue a message (return.first == false if enq() was disabled) - timeout if waiting too long
  bool timed_enq(T t,std::size_t ms,boost::system::error_code&ec){
    std::unique_lock<std::mutex>lock(*mtx_);
    if(!enq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return false;
    }
    return detail::queue_support::sendwait<T,SERIAL>(fdwrite_,&t,ms,ec,true,sep_,serial_);
  }
  // wait until we can retrieve a message from queue
  bool wait_enq(boost::system::error_code&ec){
    std::unique_lock<std::mutex>lock(*mtx_);
    if(!enq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return false;
    }
    return detail::queue_support::sendwait<T,SERIAL>(fdwrite_,nullptr,0,ec,false,sep_,serial_);
  }
  // wait until we can retrieve a message from queue -  timeout if waiting too long
  bool timed_wait_enq(std::size_t ms,boost::system::error_code&ec){
    std::unique_lock<std::mutex>lock(*mtx_);
    if(!enq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return false;
    }
    return detail::queue_support::sendwait<T,SERIAL>(fdwrite_,nullptr,ms,ec,false,sep_,serial_);
  }
  // cancel enq operations (will also release blocking threads)
  void disable_enq(bool disable){
    std::unique_lock<std::mutex>lock(*mtx_);
    enq_enabled_=!disable;
  }
  // get underlying file descriptor
  int getfd()const{
    return fdwrite_;
  }
private:
  // state of queue
  int fdwrite_;                          // file descriptors serialize object tpo
  SERIAL serial_;                        // serialise
  char sep_;                             // message separator
  bool closeOnExit_;                     // close fd on exit
  mutable std::unique_ptr<std::mutex>mtx_;// must be pointer since not movable
  bool enq_enabled_=true;                // is enqueing enabled
};
}
}
#endif
