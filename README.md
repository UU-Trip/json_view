# json_view
Single header implementation for reading json strings.

Create a json_str_view or json_wstr_view from any string containing a valid json string.
```
json_str_view view{
    "{\"pi\":3.14,\"digits\":[1,4,1,5,9,2,6,5]}"
};
```
Access a value with at(key) or operator[key]. Access that value's values with at(key) or operator[key]!
Access array indexes with at(index) or operator[index].
Convert any json_view with string_view(), string(), boolean(), integer() or floating_point().
```
float pi = view.at("pi").floating_point();
std::string first = view["digits"][0].string();
```
Iterate through an object's or an array's parts with a range based for loop.
Use key() to access the current iteration's key if iterating over an object.
Use value() to get a json_view of the current iteration's value.
```
for (auto&& kvp : view) {
    std::cout << kvp.key().string_view() << " = " << kvp.value().string_view() << std::endl;
}
for (auto&& digit : view["digits"]) {
    std::cout << digit.integer();
}
```

Faster than rapidjson!*

<sub>*Might not actually be faster than rapidjson.</sub>
