// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

#ifndef QUEUE_EMPTY_BASE_H__
#define QUEUE_EMPTY_BASE_H__
namespace boost{
namespace asio{
namespace detail{
namespace base{

// empty dummy class used for queues base class
template<typename T>
struct queue_empty_base{
  using value_type=T;
};
}
}
}
}
#endif
