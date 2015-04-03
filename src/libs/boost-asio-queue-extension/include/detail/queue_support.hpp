// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

#ifndef __QUEUE_SUPPPORT_H__
#define __QUEUE_SUPPPORT_H__

#include <algorithm>
#include <string>
#include <map>
#include <list>
#include <iostream>
#include <utility>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>

namespace boost{
namespace asio{
namespace detail{
namespace queue_support{

namespace fs=boost::filesystem;
namespace io=boost::iostreams;

namespace{
// helper function for serialising an object
// (lock must be held when calling this function)
template<typename T,typename SERIAL>
void write(T const&t,fs::path const&dir,SERIAL serial){
  // create a unique filename, open file for writing and serialise object to file (user defined function)
  // (serialization function is a user supplied function - see ctor)
  std::string const id{boost::lexical_cast<std::string>(boost::uuids::random_generator()())};
  fs::path fullpath{dir/id};
  std::ofstream os{fullpath.string(),std::ofstream::binary};
  if(!os)throw std::runtime_error(std::string("asio::detail::dirqueue_support::::write: could not open file: ")+fullpath.string());
  serial(os,t);
  os.close();
}
// helper function for deserialising an object
// (lock must be held when calling this function)
template<typename T,typename DESER>
T read(fs::path const&fullpath,DESER deser){
  // open input stream, deserialize stream into an object and remove file
  // (deserialization function is a user supplied function - see ctor)
  std::ifstream is{fullpath.string(),std::ifstream::binary};
  if(!is)throw std::runtime_error(std::string("asio::detail::dirqueue_support::read: could not open file: ")+fullpath.string());
  T ret{deser(is)};
  is.close();
  std::remove(fullpath.string().c_str());
  return ret;
}
// close a file descriptor
int eclose(int fd,bool throwExcept){
  while(close(fd)<0&&errno==EINTR);
  if(errno&&throwExcept){
    std::string err{strerror(errno)};
    char buf[1024];
    ::strerror_r(errno,buf,1024);
    throw std::runtime_error(std::string("eclose: failed closing fd:" )+buf);
  }
  return errno;
}
// set fd to non-blocking
int setFdNonblock(int fd){
  int flags=fcntl(fd,F_GETFL,0);
  if(fcntl(fd, F_SETFL,flags|O_NONBLOCK)<0)return errno;
  return 0;
}

}
}
}
}
}
#endif
