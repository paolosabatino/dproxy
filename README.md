# dproxy
Simple DNS caching proxy

As a complete rewrite of original dproxy from Matthew Pratt (http://sourceforge.net/projects/dproxy/), this new dproxy offers pthread-based multithreading, binary-tree based cache for all DNS request types and fast request routing. Still it runs and compiles fine for either desktops and embedded machines.


To compile and install just run:
  
  make
  make install
  
A copy of a configuration file can be obtained invoking the executable with -P flag

  ./dproxy -P
  

