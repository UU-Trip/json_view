# json_view
create a json_str_view or json_wstr_view from any string containing a valid json string.  
  
access a value with at(key). access that value's values with at(key)!  
access array indexes with at(index).  
  
iterate through an object's or an array's parts with a range based loop.  
use key() to access the current iteration's key if iterating over an object.  
use value() to get a json_view of the current iteration's value.  
  
convert any json_view with string(), boolean() or integer().  
