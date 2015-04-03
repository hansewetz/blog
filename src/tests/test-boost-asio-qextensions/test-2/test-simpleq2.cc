// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

#include <boost/asio_queue.hpp>
#include <boost/asio.hpp>
#include <boost/asio/posix/basic_descriptor.hpp>
#include <boost/log/trivial.hpp>
#include <string>
#include <memory>
using namespace std;
using namespace std::placeholders;

// asio io_service
boost::asio::io_service ios;

// variables enabling us to know when we are done
atomic<size_t>nlinesRemaining{0};
atomic<bool>done{false};

// queue stuff
using queue_t=boost::asio::simple_queue<string>;
shared_ptr<queue_t>q{new queue_t(3)};
boost::asio::queue_listener<queue_t>qlistener(::ios,q.get());
boost::asio::queue_sender<queue_t>qsender(::ios,q.get());

// posix stream
boost::asio::posix::stream_descriptor fd_read(::ios,0);
boost::asio::streambuf fd_data;

// handler for queue listener
template<typename T>
void qlistener_handler(boost::system::error_code const&ec,T item){
  if(!ec){
    BOOST_LOG_TRIVIAL(debug)<<"received item in queue: \""<<item<<"\"";
    qlistener.async_deq(std::bind(qlistener_handler<T>,_1,_2));
    --nlinesRemaining;
  }else{
    // ignore errors for right now
    // ...
  }
  // check if we should shut down queues
  if(done&&nlinesRemaining==0){
    q->disable_deq(true);
    q->disable_enq(true);
  }
}
// handler for fd from child process
void fd_handler(boost::system::error_code ec,size_t size){
  if(ec.value()==boost::asio::error::eof&&size==0){
    // we are done  - but can't shut down queues until they are empty
    done=true;
    fd_read.cancel();
    fd_read.close();
  }else
  if(ec.value()){
    // ignore errors for right now
    // ...
  }else{
    // get read line and enque it asynchronously into the queue
    istream is(&fd_data);
    string item;
    getline(is,item);
    ++nlinesRemaining;
    qsender.async_enq(item,[&](boost::system::error_code const&ec){});

    // continue reading from fd
    boost::asio::async_read_until(fd_read,fd_data,'\n',std::bind(fd_handler,_1,_2));
  }
}
// test program
int main(){
  try{
    // read line by line from stdin
    boost::asio::async_read_until(fd_read,fd_data,'\n',std::bind(fd_handler,_1,_2));

    // start listening to messages from queue
    qlistener.async_deq(std::bind(qlistener_handler<string>,_1,_2));

    // run asio 
    ::ios.run();
  }
  catch(exception const&e){
    BOOST_LOG_TRIVIAL(error)<<"cought exception: "<<e.what();
  }
}
