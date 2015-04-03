#include "type-utils/type_utils.h"
#include <tuple>
#include <string>
#include <sstream>
#include <functional>
using namespace std;
using namespace utils;

// concatenate arguments into a string
struct Concatenate2{
  template<typename T1,typename T2>
  string operator()(T1&&t1,T2&&t2){
    stringstream str;
    str<<t1<<t2;
    return str.str();
  }
};
// test function for tuple
struct PrintNValues{
  template<typename...T>
  void operator()(T&&...t){
    auto tu=make_tuple(std::forward<T>(t)...);
    cout<<tu<<endl;
  }
};
// print 1 values
struct Print1Value{
  template<typename T>
  void operator()(T&&t){cout<<t<<endl;}
};
// sum int values meta function
template<std::size_t i,std::size_t j>
struct sum{
  constexpr static std::size_t value=i+j;
};
template<std::size_t i,std::size_t j>
struct mul{
  constexpr static std::size_t value=i*j;
};
// main.
int main(int argc,char**argv){
  // transform a tuple using a meta function template (on indlist)
  auto tu=make_tuple(1,"two",3,"four",5);
  cout<<"xform pop_front template: "<<transform_tuple<pop_front>(tu)<<endl;

  // transform a tuple using an indlist (pick elemnts based on an indlist)
  auto tv=make_tuple(1,"two",3,"four",5);
  using IL=indlist<0,4>;
  cout<<"xform using indlist: "<<transform_tuple(IL(),tv)<<endl;

  // reverse a tuple
  auto tz=make_tuple(1,"two",3,"four",5);
  cout<<"reverse tuple1: "<<reverse_tuple(tz)<<endl;
  cout<<"reverse tuple2: "<<transform_tuple<reverse>(tz)<<endl;

  // transform tuples using an indlist by picking elemnst from the tuples
  auto t1=make_tuple(1,"one","ONE");
  auto t2=make_tuple(2,"two","TWO");
  auto t3=make_tuple(3,"three","THREE");
  cout<<"xform multiple tuples: "<<transform_tuples(indlist<0,1,2>(),t1,t2,t3)<<endl;

  // apply a function to ith elemnt of a set of tuples
  auto u1=make_tuple(1,"one","ONE");
  auto u2=make_tuple(2,"two","TWO");
  cout<<"sum of first elemnt of several tuples: "<<apply_tuple(std::plus<int>(),transform_tuples<0>(u1,u2))<<endl;

  // pop 2 elements from fron of tuple
  auto a1=make_tuple(1,2,3,4);
  cout<<"popn_front<2>: "<<popn_front_tuple<2>(a1)<<endl;

  // fold tuple
  auto b=make_tuple("one=",1," two=",2," five=",5);
  cout<<"fold to strings: "<<foldl_tuple(Concatenate2(),b)<<endl;

  auto c=make_tuple(1,2,3,4,5,6);
  cout<<"six factorial: "<<foldl_tuple(multiplies<int>(),c)<<endl;

  // create a tuple containing the type of each tuple element as a string
  auto d=make_tuple(1,"hello",string("world"),type2string_helper());
  cout<<"transformed tuple: "<<transform_tuple(type2string_helper(),d)<<endl;

  // multiply elements in a tuple
  auto e=make_tuple(1,2,3,4,5);
  cout<<"5!="<<foldl_tuple(std::multiplies<int>(),e)<<endl;

  // check predicates
  using IL1=indlist<0,1,2>;
  auto f=make_tuple(1);
  cout<<IL()<<" is an indlist: "<<boolalpha<<is_indlist<IL>::value<<endl;
  cout<<f<<" is an indlist: "<<boolalpha<<is_indlist<decltype(e)>::value<<endl;
  cout<<IL()<<" is tuple: "<<boolalpha<<is_tuple<IL>::value<<endl;
  cout<<f<<" is a tuple: "<<boolalpha<<is_tuple<decltype(e)>::value<<endl;

  // do a inner product using foldl (1^4+2^4+...)
  auto w1=make_tuple(1,2,3,4);
  cout<<tuple_inner_product(w1,w1,w1,w1)<<endl;

  // print tuple
  auto z1=make_tuple(1,"hello",2,std::plus<int>());
  cout<<type2string(z1)<<endl;

  // print the types of a tuple
  auto z2=std::make_tuple(1,"hello",2,std::plus<int>());
  cout<<transform_tuple(type2string_helper(),z2)<<endl;

  // rearange elements
  auto z3=make_tuple(0,1,2,3,4);
  cout<<"rearanged tuple: "<<transform_tuple(indlist<4,0,2,3,2>(),z3)<<endl;

  // add tuple elemnts together
  auto z5=make_tuple(1,2.3,7);
  cout<<apply_tuple(adder(),z5)<<endl;

  // reverse tuple elements
  auto ttu1=make_tuple(1,2,3,4,5);
  cout<<transform_tuple<reverse>(ttu1)<<endl;

  // call a function for each element in a tuple in reverse order
  auto z6=make_tuple(1,2,"three");
  using IL6=make_indlist_from_tuple<decltype(z6)>::type;
  apply_tuple_ntimes_with_indlist(reverse<IL6>::type(),Print1Value(),z6);

  // transform each element in a tuple generating a new tuple
  auto z7=make_tuple(1,2,3,4);
  cout<<"mulyiply by 2: "<<transform_tuple([](int val){return val*2;},z7)<<endl;

  // build a tuple by taking the first element from a set of tuples
  auto x1=make_tuple(1,2,3,4,5,6);
  auto x2=make_tuple(3,4,5,6,7);
  auto x3=make_tuple(10,11);
  cout<<apply_nth_elem_tuples<1>(maketuple(),x1,x2,x3)<<endl;

  // call a function by reversing arguments
  using IL8=make_indlist_from_range<0,5>::type;
  cout<<"apply_with_indlist (reverse) ";
  apply_with_indlist(reverse<IL8>::type(),PrintNValues(),0,1,2,3,4,5);
  cout<<endl;

  // build a tuple by picking the second element from each tuple
  auto xx1=make_tuple(1,2,3,4,5,6);
  auto xx2=make_tuple(3,4,5,6,7);
  auto xx3=make_tuple(10,11);
  cout<<apply_nth_elem_tuples<1>(maketuple(),xx1,xx2,xx3)<<endl;

  // call a function for each tuple element passing (index, tuple-element) to function
  auto z8=make_tuple("one","two","thre");
  apply_ind_tuple_ntimes_with_indlist(make_indlist_from_tuple<decltype(z8)>::type(),PrintNValues(),z8);
}
