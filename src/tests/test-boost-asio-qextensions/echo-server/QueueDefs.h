// (C) Copyright Hans Ewetz 2010,2011,2012,2013,2014,2015. All rights reserved.

#ifndef __QUEUE_H__
#define __QUEUE_H__
#include <boost/asio_queue.hpp>
#include <string>
#include <iosfwd>

// queue value type
using QueueValueType=std::string;

// message separator
constexpr char sep='|';

// serialiser/de-serialiser
auto msgDeserializer=[](std::istream&is)->QueueValueType{std::string line;getline(is,line,sep);return line;};
auto msgSerializer=[](std::ostream&os,QueueValueType msg)->void{os<<msg;};

// typedefs for base classes for msg queues
using QueueBase=boost::asio::detail::base::queue_interface_base<QueueValueType>;
using QueueDeqBase=boost::asio::detail::base::queue_interface_deq<QueueValueType>;
using QueueEnqBase=boost::asio::detail::base::queue_interface_enq<QueueValueType>;

// typedefs for concrete queues 
using MemQueue=boost::asio::simple_queue<QueueValueType,QueueBase>;
using SockServQueue=boost::asio::sockserv_queue<QueueValueType,decltype(msgDeserializer),decltype(msgSerializer),QueueBase>;
using SockClientQueue=boost::asio::sockclient_queue<QueueValueType,decltype(msgDeserializer),decltype(msgSerializer),QueueBase>;

// typedef for qsender/qlistener based on queue base classes
using QueueSender=boost::asio::queue_sender<QueueEnqBase>;
using QueueListener=boost::asio::queue_listener<QueueDeqBase>;
#endif
