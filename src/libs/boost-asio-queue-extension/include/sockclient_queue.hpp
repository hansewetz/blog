// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

/* TODO
IMPROVEMENTS:
	- we should have two timeouts, message timeout, byte timeout
	- read/write more than one character at a time ... must then buffer what we have read

TESTING:
	- test with serializing real object and base64 encode them
*/

#ifndef __SOCK_CLIENT_QUEUE_H__
#define __SOCK_CLIENT_QUEUE_H__
#include "detail/queue_empty_base.hpp"
#include "detail/queue_support.hpp"
#include "detail/fdqueue_support.hpp"
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

// boost stuff
#include <boost/lexical_cast.hpp>

namespace boost{
namespace asio{

// a socket based client queue connecting to a server - full duplex queue - only 1 client can connect to the server
// (if sending objects first serialise the object, the base64 encode it in the serialiser)
// (the tmo in ms is based on message timeout - if no message starts arriving within timeout, the function times out)
// (ones we have started to read a message, the message will never timeout)
// (the class is meant to be used in singele threaded mode and is not thread safe)
template<typename T,typename DESER,typename SERIAL,typename Base=detail::base::queue_empty_base<T>>
class sockclient_queue:public Base{
public:
  // default message separaor
  constexpr static char NEWLINE='\n';

  // ctor
  sockclient_queue(std::string const&serverName,int port,DESER deser,SERIAL serial,char sep=NEWLINE):
      serverName_(serverName),port_(port),deser_(deser),serial_(serial),sep_{sep},closeOnExit_(true),mtx_{std::make_unique<std::mutex>()}{
    // create socket
    createSocket();
  }
  // copy ctor
  sockclient_queue(sockclient_queue const&)=delete;

  // move ctor
  sockclient_queue(sockclient_queue&&other):
      serverName_(other.serverName_),port_(other.port_),deser_(std::move(other.deser_)),serial_(std::move(other.serial_)),sep_(other.sep_),
      clientsocket_(other.clientsocket_),yes_(other.yes_),closeOnExit_(other.closeOnExit_),server_(other.server_),
      mtx_(std::move(other.mtx_)),deq_enabled_(other.deq_enabled_),enq_enabled_(other.enq_enabled_)
  {
    other.closeOnExit_=false; // make sure we don't close twice
    memcpy(static_cast<void*>(&servaddr_),static_cast<void*>(&other.servaddr_),sizeof(servaddr_));
  }
  // assign
  sockclient_queue&operator=(sockclient_queue const&)=delete;

  // move assign
  sockclient_queue&operator=(sockclient_queue&&other){
    serverName_=other.serverName_;
    port_=other.port_;
    deser_=std::move(other.deser_);
    serial_=std::move(other.serial_);
    sep_=other.sep_;
    clientsocket_=other.clientsocket_;
    yes_=other.yes_;
    closeOnExit_=other.closeOnExit_;
    server_=other.server_;
    other.closeOnExit_=false; // make sure we don't close twice
    memcpy(static_cast<void*>(&servaddr_),static_cast<void*>(&other.servaddr_),sizeof(servaddr_));
    mtx_=std::move(other.mtx_);
    deq_enabled_=other.deq_enabled_;
    enq_enabled_=other.enq_enabled_;
    return*this;
  }
  // dtor
  ~sockclient_queue(){
    if(closeOnExit_){
      if(state_!=IDLE){
        if(state_==CONNECTED)detail::queue_support::eclose(clientsocket_,false);
        detail::queue_support::eclose(clientsocket_,false);
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
  // enqueue a message (return.first == false if enq() was disabled)
  bool enq(T t,boost::system::error_code&ec){
    std::unique_lock<std::mutex>lock(*mtx_);
    if(!enq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return false;
    }
    return enqAux(&t,0,ec,true);
  }
  // enqueue a message (return.first == false if enq() was disabled) - timeout if waiting too long
  bool timed_enq(T t,std::size_t ms,boost::system::error_code&ec){
    std::unique_lock<std::mutex>lock(*mtx_);
    if(!enq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return false;
    }
    return enqAux(&t,ms,ec,true);
  }
  // wait until we can retrieve a message from queue
  bool wait_enq(boost::system::error_code&ec){
    std::unique_lock<std::mutex>lock(*mtx_);
    if(!enq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return false;
    }
    return enqAux(nullptr,0,ec,false);
  }
  // wait until we can retrieve a message from queue -  timeout if waiting too long
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

    // if we are in IDLE state it means we are disconnected
    if(state_==IDLE){
      connect2server(ec1);
      if(ec1!=boost::system::error_code()){
        if(ec1!=boost::asio::error::timed_out)detail::queue_support::eclose(clientsocket_,false);
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
          detail::queue_support::eclose(clientsocket_,false);
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
    // dummy return - will never reach here
    return std::make_pair(false,T{});
  }
  // enqueue a message
  bool enqAux(T const*t,std::size_t ms,boost::system::error_code&ec,bool sendMsg){
    boost::system::error_code ec1;

    // wait for client connection if needed
    if(state_==IDLE){
      connect2server(ec1);
      if(ec1!=boost::system::error_code()){
        if(ec1!=boost::asio::error::timed_out)detail::queue_support::eclose(clientsocket_,false);
        ec=ec1;
        return false;
      }
      state_=CONNECTED;
    }
    // client connected - write message
    if(state_==CONNECTED){
      detail::queue_support::sendwait<T,SERIAL>(clientsocket_,t,0,ec1,sendMsg,sep_,serial_);
      state_=WRITING;
      if(ec1!=boost::system::error_code()&&ec1!=boost::asio::error::timed_out){
        if(ec1!=boost::asio::error::timed_out){
          detail::queue_support::eclose(clientsocket_,false);
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
  // --------------------------------- helper functions
  // (no state is managed here)
  // create socket to connect to server with (throws exception if failure)
  void createSocket(){
    // we must be in state IDLE to be here
    assert(state_==IDLE);

    // get socket 
    if((clientsocket_=socket(AF_INET,SOCK_STREAM,0))==-1){
      throw std::runtime_error(std::string("sockclient_queue::connect2Server: failed creating socket, errno: ")+boost::lexical_cast<std::string>(errno));
    }
    // set socket options 
    if(setsockopt(clientsocket_,SOL_SOCKET,SO_REUSEADDR,&yes_,sizeof(int))==-1){
      throw std::runtime_error(std::string("sockclient_queue::connect2Server: failed setting socket options, errno: ")+boost::lexical_cast<std::string>(errno));
    }
    // get server host
    server_=gethostbyname(serverName_.c_str());
    if (server_==NULL) {
      throw std::runtime_error(std::string("sockclient_queue::connect2Server: failed converting ")+
                               serverName_+" to hostname, errno: "+boost::lexical_cast<std::string>(errno));
    }
    // setup addresses so we can connect later
    bzero((char *)&servaddr_,sizeof(servaddr_));
    servaddr_.sin_family=AF_INET;
    bcopy((char *)server_->h_addr,(char*)&servaddr_.sin_addr.s_addr,server_->h_length);
    servaddr_.sin_port=htons(port_);
  }
  // connect to server
  void connect2server(boost::system::error_code&ec){
    // connect to server
    ec=boost::system::error_code();
    if(connect(clientsocket_,(const struct sockaddr*)&servaddr_,sizeof(servaddr_))<0) {
      ec=boost::system::error_code(errno,boost::system::get_posix_category());
    }
    state_=CONNECTED;
  }
  // --------------------------------- private attributes
  // state object is in
  enum state_t{IDLE=0,CONNECTED=1,READING=2,WRITING=3};
  state_t state_=IDLE;

  // server socket stuff
  std::string serverName_;               // server to connect to as a name
  int port_;                             // port to listen on
  DESER deser_;                          // de-serialiser
  SERIAL serial_;                        // serialiser
  char sep_;                             // message separator
  int clientsocket_=-1;                    // socket used by client
  struct sockaddr_in servaddr_;          // server address
  int yes_=1;                            // used for setting socket options
  bool closeOnExit_;                     // close fd on exit (if we have been moved we don;t close)
  struct hostent*server_;                // server when converting name to host

  mutable std::unique_ptr<std::mutex>mtx_;                  // must be pointer since not movable
  bool deq_enabled_=true;
  bool enq_enabled_=true;
};
}
}
#endif
