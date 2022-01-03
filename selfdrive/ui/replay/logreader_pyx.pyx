# distutils: language = c++
# cython: c_string_encoding=ascii, language_level=3

import numpy as np
cimport numpy as cnp
from libc.string cimport memcpy
from cereal import log as capnp_log
from libcpp cimport bool
from libcpp.vector cimport vector
from libcpp.string cimport string

cdef extern from "selfdrive/ui/replay/logreader.h":
  cdef cppclass cpp_Event "Event":
    cpp_Event()
    char *data()
    size_t size()
    unsigned long long monoTime()

  cdef cppclass cpp_LogReader "LogReader":
    cpp_LogReader()
    void setSortByTime(bool)
    bool load(string, bool)
    vector[cpp_Event *] &getEvents() nogil
    cpp_Event *at(int) nogil
    size_t size()
  
cdef class LogReader:
  cdef cpp_LogReader *lr

  def __cinit__(self, fn, sort_by_time=False, cache_to_local=False):
    self.lr = new cpp_LogReader()
    self.lr.setSortByTime(sort_by_time)
    self.lr.load(fn.encode(), cache_to_local)

  def __dealloc__(self):
    del self.lr

  def __getitem__(self, item):
    return self.__from_bytes(self.lr.at(item))

  def __iter__(self):
    with nogil:
      events = &self.lr.getEvents()
    for i in xrange(events.size()):
      yield self.__from_bytes(events.at(i))
  
  def __len__(self):
    return self.lr.size()

  def ts(self, idx):
    return self.lr.at(idx).monoTime()

  cdef __from_bytes(self, cpp_Event* e):
    cdef cnp.ndarray dat = np.empty(e.size(), dtype=np.uint8)
    cdef char[:] dat_view = dat
    memcpy(&dat_view[0], e.data(), e.size())
    return capnp_log.Event.from_bytes(dat_view)
