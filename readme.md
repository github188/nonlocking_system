####
 The  project's global is to set up a non-locked system within the environment of  multi-thread.Year,you maybe have read  the codes of libevent ,etc.To a certain extent,they all build on the single thread.
 
###Ideas
 
####
 
A non-locked system's realise often make full use of the automic operation.such as:
```
__sync_bool_compare_and_swap(lock,old,set)
__sync_fetch_and_add(value,add)
__sync_fetch_and_sub(value,sub)	

``` 




###Issue

1. "Too many open files "
type the "ulimit -a " to check the system params,If it is too small,use "ulimit -n " to correct it




###Others

####
jweih
####
1989428128@qq.com 
  
 