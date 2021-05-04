from libcpp.string cimport string
from libcpp cimport bool

cdef extern from "selfdrive/common/params.cc":
  pass

cdef extern from "selfdrive/common/util.cc":
  pass

cdef extern from "selfdrive/common/params.h":
  cpdef enum ParamKeyType:
    PERSISTENT
    CLEAR_ON_MANAGER_START
    CLEAR_ON_PANDA_DISCONNECT
    ALL

  cdef cppclass Params:
    Params(bool)
    Params(string)
    string get(string, bool) nogil
    bool getBool(string)
    int remove(string)
    int put(string, string)
    int putBool(string, bool)
    bool check_key(string)
    void clear_all(ParamKeyType)
