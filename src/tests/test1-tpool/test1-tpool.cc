// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

#include "type-utils/tpool.h"
#include <functional>
#include <iostream>

using namespace std;
using namespace utils;

struct Foo{
  Foo(){cout<<"ctor"<<endl;}
  Foo(string const&str){cout<<"string ctor"<<endl;}
  Foo(Foo const&f){cout<<"copy ctor"<<endl;}
  Foo(Foo&&f){cout<<"move ctor"<<endl;}
  Foo&operator=(Foo const&f){cout<<"copy assignment"<<endl;return*this;}
  Foo&operator=(Foo&&f){cout<<"move assign"<<endl;return*this;}
  ~Foo(){cout<<"dtor"<<endl;}
};
ostream&operator<<(ostream&os,Foo const&foo){return cout<<"foo ";}

// main test program
int main(){
  // create function returning another function
  auto f=[]()->Foo{return Foo("Hello");};
  auto g=[]()->void{cout<<"world"<<endl;};
  function<Foo()>ff=f;

  // create and submit tasks
  {
    tpool tp(2);
    auto res=tp.submitTasksAndWait(ff,f,[]()->int{return 6;},g);
    cout<<transform_tuple(type2string_helper(),res)<<endl;
    cout<<"res: "<<res<<endl;
  }
  {
    tpool tp(2);
    auto tu=make_tuple(ff,f,[]()->int{return 6;},g);
    auto res=tp.submitTaskTupleAndWait(tu);
    cout<<transform_tuple(type2string_helper(),res)<<endl;
    cout<<"res: "<<res<<endl;
  }
}
