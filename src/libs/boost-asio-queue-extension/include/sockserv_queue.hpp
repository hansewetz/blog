// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

/* TODO
IMPROVEMENTS:
	- we should have two timeouts, message timeout, byte timeout
	- read/write more than one character at a time ... must then buffer what we have read

TESTING:
	- test with serializing real object and base64 encode them
*/

#ifndef __SOCK_SERV_QUEUE_H__
#define __SOCK_SERV_QUEUE_H__
#include "detail/queue_empty_base.hpp"
#include "detail/queue_support.hpp"
#include "detail/fdqueue_support.hpp"
#include "detail/sockqueue_support.hpp"
#include <string>
#include <utility>
#include <iostream>
#include <string>
#include <mutex>
#include <algorithm>
#include <string.h>

// socket stuff
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

namespace boost{
namespace asio{

// a socket based server queue listening on clients - full duplex queue - only 1 client can connect
// (if sending objects first serialise the object, the base64 encode it in the serialiser)
// (the tmo in ms is based on message timeout - if no message starts arriving within timeout, the function times out)
// (ones we have started to read a message, the message will never timeout)
// (the class is meant to be used in singele threaded mode and is not thread safe)
template<typename T,typename DESER,typename SERIAL,typename Base=detail::base::queue_empty_base<T>>
class sockserv_queue:public Base{
public:
  // default message separaor
  constexpr static char NEWLINE='\n';

  // ctor
  sockserv_queue(int port,DESER deser,SERIAL serial,char sep=NEWLINE):
      port_(port),deser_(deser),serial_(serial),sep_{sep},closeOnExit_(true),mtx_{std::make_unique<std::mutex>()}{
    // start listening on socket
    servsocket_=detail::sockqueue_support::createListenSocket(port_,serveraddr_,maxclients_,&yes_);
  }
  // copy ctor
  sockserv_queue(sockserv_queue const&)=delete;

  // move ctor
  sockserv_queue(sockserv_queue&&other):
      port_(other.port_), deser_(std::move(other.deser_)),serial_(std::move(other.serial_)),sep_(other.sep_),closeOnExit_(other.closeOnExit_),
      servsocket_(other.servsocket_),yes_(other.yes_),
      mtx_(std::move(other.mtx_)),deq_enabled_(other.deq_enabled_),enq_enabled_(other.enq_enabled_)
  {
    other.closeOnExit_=false; // make sure we don't close twice
    memcpy(static_cast<void*>(&serveraddr_),static_cast<void*>(&other.serveraddr_),sizeof(serveraddr_));
    memcpy(static_cast<void*>(&clientaddr_),static_cast<void*>(&other.clientaddr_),sizeof(clientaddr_));
  }
  // assign
  sockserv_queue&operator=(sockserv_queue const&)=delete;

  // move assign
  sockserv_queue&operator=(sockserv_queue&&other){
    port_=other.port_;
    deser_=std::move(other.deser_);
    serial_=std::move(other.serial_);
    sep_=other.sep_;
    closeOnExit_=other.closeOnExit_;
    other.closeOnExit_=false; // make sure we don't close twice

    // server socket stuff
    servsocket_=other.servsocket_;
    yes_=other.yes_;
    memcpy(static_cast<void*>(&serveraddr_),static_cast<void*>(&other.serveraddr_),sizeof(serveraddr_));
    memcpy(static_cast<void*>(&clientaddr_),static_cast<void*>(&other.clientaddr_),sizeof(clientaddr_));

    mtx_=std::move(other.mtx_);
    deq_enabled_=other.deq_enabled_;
    enq_enabled_=other.enq_enabled_;
    return*this;
  }
  // ctor
  ~sockserv_queue(){
    if(closeOnExit_){
      if(state_!=IDLE){
        if(state_==CONNECTED)detail::queue_support::eclose(clientsocket_,false);
        detail::queue_support::eclose(servsocket_,false);
      }
    }
  }
  // dequeue a message (return.first == false if deq() was disabled)
  std::pair<bool,T>deq(boost::system::error_code&ec){
    // if deq is disabled we are done
    std::unique_lock<std::mutex>lock(*mtx_);
    if(!deq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return std::make_pair(false,T{});
    }
    return deqAux(0,ec,true);
  }
  // dequeue a message (return.first == false if deq() was disabled) - timeout if waiting too long
  std::pair<bool,T>timed_deq(std::size_t ms,boost::system::error_code&ec){
    // if deq is disabled we are done
    std::unique_lock<std::mutex>lock(*mtx_);
    if(!deq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return std::make_pair(false,T{});
    }
    return deqAux(ms,ec,true);
  }
  // wait until we can retrieve a message from queue
  bool wait_deq(boost::system::error_code&ec){
    // if deq is disabled we are done
    std::unique_lock<std::mutex>lock(*mtx_);
    if(!deq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return false;
    }
    deqAux(0,ec,false);
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
    deqAux(ms,ec,false);
    if(ec==boost::asio::error::timed_out)return false;
    if(ec.value()!=0)return false;
    return true;
  }
  // put a message into queue
  bool enq(T t,boost::system::error_code&ec){
    std::unique_lock<std::mutex>lock(*mtx_);
    if(!enq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return false;
    }
    return enqAux(&t,0,ec,true);
  }
  // put a message into queue - timeout if waiting too long
  bool timed_enq(T t,std::size_t ms,boost::system::error_code&ec){
    std::unique_lock<std::mutex>lock(*mtx_);
    if(!enq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return false;
    }
    return enqAux(&t,ms,ec,true);
  }
  // wait until we can put a message in queue
  bool wait_enq(boost::system::error_code&ec){
    std::unique_lock<std::mutex>lock(*mtx_);
    if(!enq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return false;
    }
    return enqAux(nullptr,0,ec,false);
  }
  // wait until we can put a message in queue - timeout if waiting too long
  bool timed_wait_enq(std::size_t ms,boost::system::error_code&ec){
    std::unique_lock<std::mutex>lock(*mtx_);
    if(!enq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return false;
    }
    return enqAux(nullptr,ms,ec,false);
  }
  // cancel deq operations
  void disable_deq(bool disable){
    std::unique_lock<std::mutex>lock(*mtx_);
    deq_enabled_=!disable;
  }
  // cancel enq operations
  void disable_enq(bool disable){
    std::unique_lock<std::mutex>lock(*mtx_);
    enq_enabled_=!disable;
  }
private:
  // --------------------------------- state management functions
  // (all state is managed here)

  // dequeue a message (return.first == false if deq() was disabled)
  std::pair<bool,T>deqAux(std::size_t ms,boost::system::error_code&ec,bool getMsg){
    boost::system::error_code ec1;

    // wait for client connection if needed
    if(state_==IDLE){
      clientsocket_=detail::sockqueue_support::waitForClientConnect(servsocket_,serveraddr_,clientaddr_,ms,ec1);
      if(ec1!=boost::system::error_code()){
        if(ec1!=boost::asio::error::timed_out)detail::queue_support::eclose(servsocket_,false);
        ec=ec1;
        return std::make_pair(false,T{});
      }
      state_=CONNECTED;
    }
    // client connected - read message
    if(state_==CONNECTED){
      state_=READING;
      T ret{detail::queue_support::recvwait<T,DESER>(clientsocket_,ms,ec1,getMsg,sep_,deser_)};
      if(ec1!=boost::system::error_code()){
        if(ec1!=boost::asio::error::timed_out){
          detail::queue_support::eclose(servsocket_,false);
          state_=IDLE;
        }else{
          state_=CONNECTED;
        }
        ec=ec1;
        return std::make_pair(false,T{});
      }
      state_=CONNECTED;
      return make_pair(true,ret);
    }
    // dummy return - will never reach
    return  std::make_pair(false,T{});
  }
  // enq a message - return false if enq failed
  bool enqAux(T const*t,std::size_t ms,boost::system::error_code&ec,bool sendMsg){
    boost::system::error_code ec1;

    // wait for client connection if needed
    if(state_==IDLE){
      clientsocket_=detail::sockqueue_support::waitForClientConnect(servsocket_,serveraddr_,clientaddr_,ms,ec1);
      if(ec1!=boost::system::error_code()){
        if(ec1!=boost::asio::error::timed_out)detail::queue_support::eclose(servsocket_,false);
        ec=ec1;
        return false;
      }
      state_=CONNECTED;
    }
    // client connected - write message
    if(state_==CONNECTED){
      state_=WRITING;
      detail::queue_support::sendwait<T,SERIAL>(clientsocket_,t,0,ec1,sendMsg,sep_,serial_);
      if(ec1!=boost::system::error_code()&&ec1!=boost::asio::error::timed_out){
        if(ec1!=boost::asio::error::timed_out){
          detail::queue_support::eclose(servsocket_,false);
          state_=IDLE;
        }else{
          state_=CONNECTED;
        }
        ec=ec1;
        return false;
      }
      state_=CONNECTED;
      return true;
    }
    // dummy return - will never reach here
    return false;
  }
  // --------------------------------- private attributes
  // state object is in
  enum state_t{IDLE=0,CONNECTED=1,READING=2,WRITING=3};
  state_t state_=IDLE;

  // state of queue
  int port_;                             // port to listen on
  DESER deser_;                          // de-serialiser
  SERIAL serial_;                        // serialiser
  std::size_t maxclients_=1;             // max clients that can connect to this queue - always equal to 1
  char sep_;                             // message separator
  bool closeOnExit_;                     // close fd on exit (if we have been moved we don;t close)

  // server socket stuff
  int servsocket_;                       // socket on which we are listening
  struct sockaddr_in serveraddr_;        // server address
  int yes_=1;                            // used for setting socket options

  // client stuff
  int clientsocket_=-1;                  // socket for client
  struct sockaddr_in clientaddr_;        // client address

  mutable std::unique_ptr<std::mutex>mtx_;                  // must be pointer since not movable
  bool deq_enabled_=true;
  bool enq_enabled_=true;
};
}
}
#endif
