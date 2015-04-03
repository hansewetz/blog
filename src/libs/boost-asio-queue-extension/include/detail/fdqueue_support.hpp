// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

#ifndef __FDQUEUE_SUPPPORT_H__
#define __FDQUEUE_SUPPPORT_H__
#include <utility>
#include <iostream>
#include <string>
#include <sstream>
#include <unistd.h>
#include <sys/select.h>
#include <boost/asio/error.hpp>

namespace boost{
namespace asio{
namespace detail{
namespace queue_support{
namespace fs=boost::filesystem;
namespace io=boost::iostreams;
namespace{

// deserialise an object from an fd stream
// or wait until there is a message to read - in this case, a default cibstructed object is returned
template<typename T,typename DESER>
T recvwait(int fdread,std::size_t ms,boost::system::error_code&ec,bool getMsg,char sep,DESER deser){
  T ret;                            // return value from this function (default ctor if no error)
  std::stringstream strstrm;        // collect read chars in a stringstream

  // loop until we have a message (or until we timeout)
  while(true){
    // setup to listen on fd descriptor
    fd_set input;
    FD_ZERO(&input);
    FD_SET(fdread,&input);
    int maxfd=fdread;

    // setup for timeout (ones we get a message we don't timeout)
    struct timeval tmo;
    tmo.tv_sec=ms/1000;
    tmo.tv_usec=(ms%1000)*1000;
    
    // block on select - timeout if configured
    assert(maxfd!=-1);
    int n=::select(++maxfd,&input,NULL,NULL,ms>0?&tmo:NULL);

    // check for error
    if(n<0){
      ec=boost::system::error_code(errno,boost::system::get_posix_category());
      return T{};
    }
    // check for tmo
    if(n==0){
      ec=boost::asio::error::timed_out;
      return T{};
    }
    // check if we got some data
    if(FD_ISSET(fdread,&input)){
      // if we are only checking if we have a message we are done here
      if(!getMsg){
        ec= boost::system::error_code{};
        return T{};
      }
      // read up to '\n' inclusive
      while(true){
        // read next character in message
        char c;
        ssize_t stat;
        while((stat=::read(fdread,&c,1))==EINTR){}
        if(stat!=1){
          // check if we have a valid read error or simply that there are no more bytes in fd
          if(errno==EWOULDBLOCK)break;

          // we have a real read error
          ec=boost::system::error_code(errno,boost::system::get_posix_category());
          return T{};
        }
        // save character just read
        strstrm<<c;

        // if we reached newline, send message including newline)
        if(c==sep)return deser(strstrm);
      }
    }
    // restet tmo 0 zero ms since we don't timeout ones we start reading a message
    ms=0;
  }
  // dummy return value - will nver get here
  return T{};
}
// serialise an object from an fd stream or wait until we timeout
// (returns true we we could serialise object, false otherwise - error code will be non-zero if false)
template<typename T,typename SERIAL>
bool sendwait(int fdwrite,T const*t,std::size_t ms,boost::system::error_code&ec,bool sendMsg,char sep,SERIAL serial){
  std::stringstream strstrm;        // serialised object

  // serialise object and get it as a string
  // (no need if we don't need to send object)
  std::string str;
  if(sendMsg){
    serial(strstrm,*t);
    strstrm<<sep;
    str=strstrm.str();
  }
  std::string::iterator sbegin{str.begin()};
  std::string::iterator send{str.end()};

  // loop until we have written message or timed out
  while(true){
    // setup to listen on fd descriptor
    fd_set output;
    FD_ZERO(&output);
    FD_SET(fdwrite,&output);
    int maxfd=fdwrite;

    // setup for timeout (ones we start writing a message we won't timeout)
    struct timeval tmo;
    tmo.tv_sec=ms/1000;
    tmo.tv_usec=(ms%1000)*1000;
    
    // block on select - timeout if configured
    assert(maxfd!=-1);
    int n=::select(++maxfd,NULL,&output,NULL,ms>0?&tmo:NULL);

    // check for error
    if(n<0){
      ec=boost::system::error_code(errno,boost::system::get_posix_category());
      return false;
    }
    // check for tmo
    if(n==0){
      ec=boost::asio::error::timed_out;
      return false;
    }
    // check if we wrote some data
    if(FD_ISSET(fdwrite,&output)){
      // if we are only checking if we can send a message
      if(!sendMsg){
        ec= boost::system::error_code{};
        return true;
      }
      // write as much as we can
      while(sbegin!=send){
        // write next character in message
        char c{*sbegin};
        ssize_t stat;
        while((stat=::write(fdwrite,&c,1))==EINTR){}
        if(stat!=1){
          // check if we have a valid write error or simply that there isnot eneough capacity in fd
          if(errno==EWOULDBLOCK)break;

          // we have a real write error
          ec=boost::system::error_code(errno,boost::system::get_posix_category());
          return false;
        }
        // we wrote one byte
        ++sbegin;
      }
    }
    // check if we are done
    if(sbegin==send)return true;

    // restet tmo 0 zero ms since we don't timeout ones we start reading a message
    ms=0;
  }
}
}
}
}
}
}
#endif
