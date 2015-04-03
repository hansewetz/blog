// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

/*
  test program for polldir_queue.
  program kicks off a sender thread and a receiver thread and terminates when a specified #of messages has been received
*/
#include <boost/asio_queue.hpp>
#include <string>
#include <thread>
#include <functional>
#include <iostream>

using namespace std;
namespace asio= boost::asio;
namespace fs=boost::filesystem;

// sender
template<typename Q>
void sender(Q&q,size_t maxmsg){
  for(std::size_t i=0;i<maxmsg;++i){
    boost::system::error_code ec;
    q.enq(i,ec);
    std::chrono::milliseconds tmo(1000);
    std::this_thread::sleep_for(tmo);
  }
}
// receiver
template<typename Q>
void receiver(Q&q,size_t maxmsg){
  while(maxmsg!=0){
    boost::system::error_code ec;
    pair<bool,int>p{q.deq(ec)};
    cout<<"deq: "<<boolalpha<<"["<<p.first<<","<<p.second<<"]"<<endl;
    --maxmsg;
  }
}
// test main program
int main(int argc,char*argv[]){
  try{
    // name and directory for queue
    string qname{"q1"};
    fs::path qdir{"./q1"};

    // setup queue
    function<int(istream&)>reader=[](istream&is){int ret;is>>ret;return ret;};
    function<void(ostream&,int)>writer=[](ostream&os,int i){os<<i;};
    asio::polldir_queue<int,decltype(reader),decltype(writer)>q1{qname,10,qdir,reader,writer,true};

    // make sure move ctor works
    asio::polldir_queue<int,decltype(reader),decltype(writer)>q2{std::move(q1)};

    // make sure move assignment works
    q1=std::move(q2);

    // remove locks if they exist
    q1.removeLockVariables(q1.qname());

    // kick off threads for sender/receiver
    size_t maxmsg{10};
    std::thread tsender{[&](){sender(q1,maxmsg);}};
    std::thread treceiver{[&](){receiver(q1,maxmsg);}};

    // join threads
    treceiver.join();
    tsender.join();
  }
  catch(exception const&e){
    cerr<<"caught exception: "<<e.what()<<endl;
    exit(1);
  }
}
