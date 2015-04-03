// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

#ifndef QUEUE_INTERFACE_DEQ_H__
#define QUEUE_INTERFACE_DEQ_H__
#include "queue_empty_base.hpp"
#include <utility>
#include <boost/asio/error.hpp>

namespace boost{
namespace asio{
namespace detail{
namespace base{

// empty dummy class used for queues base class
template<typename T>
struct queue_interface_deq:public virtual queue_empty_base<T>{
  virtual~queue_interface_deq(){}

  // deq functions
  virtual std::pair<bool,T>deq(boost::system::error_code&ec)=0;
  virtual std::pair<bool,T>timed_deq(std::size_t ms,boost::system::error_code&ec)=0;
  virtual bool wait_deq(boost::system::error_code&ec)=0;
  virtual bool timed_wait_deq(std::size_t ms,boost::system::error_code&ec)=0;
  virtual  void disable_deq(bool disable)=0;
};
}
}
}
}
#endif
