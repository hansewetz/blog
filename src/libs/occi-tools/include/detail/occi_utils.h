// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

#ifndef __OCCI_UTILS_H__
#define __OCCI_UTILS_H__
#include <occiData.h>
#include <string>
#include <tuple>
namespace occi_utils{

// typedef for authentication data
typedef std::tuple<std::string,std::string,std::string>occi_auth;

// deleter for environment.
struct occi_env_deleter{
  void operator()(oracle::occi::Environment*env)const{
    if(env)oracle::occi::Environment::terminateEnvironment(env);
  }
};
// deleter for stateless connection pool
class occi_stateless_pool_deleter{
public:
  explicit occi_stateless_pool_deleter(oracle::occi::Environment*env):env_(env){}
  void operator()(oracle::occi::StatelessConnectionPool*connpool)const{
    if(env_&&connpool)env_->terminateStatelessConnectionPool(connpool);
  }
private:
  oracle::occi::Environment*env_;
};
// deleter for connection.
class occi_conn_deleter{
public:
  explicit occi_conn_deleter(oracle::occi::Environment*env):env_(env){}
  void operator()(oracle::occi::Connection*conn)const{
    if(env_&&conn)env_->terminateConnection(conn);
  }
private:
  oracle::occi::Environment*env_;
};
// deleter for statement.
class occi_stmt_deleter{
public:
  explicit occi_stmt_deleter(oracle::occi::Connection*conn):conn_(conn){}
  void operator()(oracle::occi::Statement*stmt)const{
    if(conn_&&stmt)conn_->terminateStatement(stmt);
  }
private:
  oracle::occi::Connection*conn_;
};
// deleter for result set.
class occi_rs_deleter{
public:
  explicit occi_rs_deleter(oracle::occi::Statement*stmt):stmt_(stmt){}
  void operator()(oracle::occi::ResultSet*rs)const{
    if(stmt_&&rs)stmt_->closeResultSet(rs);
  }
private:
  oracle::occi::Statement*stmt_;
};
}
#endif
