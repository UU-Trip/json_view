#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <iterator>
#include <ostream>
#include <string_view>
#include <type_traits>
#include <utility>

template <typename char_t> class json_view {
    constexpr inline static bool constexpr_iswspace(char_t c) {
        constexpr std::array wspace{' ', '\n', '\r', '\f', '\v', '\t'};
        return std::any_of(wspace.begin(), wspace.end(), [c](auto wc) { return c == wc; });
    }
    template <std::size_t n> struct literal_helper {
        constexpr literal_helper(const char (&in)[n]) : str{} {
            for (std::size_t i{0UL}; i < n; ++i) {
                str[i] = static_cast<char_t>(in[i]);
            }
        }
        constexpr operator const char_t*() const {
            return str.data();
        }
        std::array<char_t, n + 1UL> str;
    };
    enum class syntax : char_t {
        object_open      = '{',
        object_close     = '}',
        array_open       = '[',
        array_close      = ']',
        colon            = ':',
        comma            = ',',
        string_delimiter = '"',
        escape           = '\\'
    };
    struct fetch {
        constexpr fetch(std::basic_string_view<char_t> data, const std::basic_string_view<char_t>::iterator& it)
        : data{data}, iter{it}
        {}
        constexpr fetch(std::basic_string_view<char_t> data)
        : fetch{data, data.begin()}
        {}
        constexpr fetch first() const {
            return {data, std::find_if(iter, data.end(), [](char_t c){ return !constexpr_iswspace(c); })};
        }
        constexpr fetch next() const {
            return fetch(data, iter + 1UL).first();
        }
        constexpr fetch next(syntax s) const {
            return {data, std::find_if(iter + 1UL, data.end(), [s](char_t c){ return c == static_cast<char_t>(s); })};
        }
        constexpr fetch end_of_string() const {
            for (auto it = iter + 1UL; it <= data.end(); ++it) {
                if (*it == static_cast<char_t>(syntax::escape)) {
                    ++it;
                } else if (*it == static_cast<char_t>(syntax::string_delimiter)) {
                    return {data, it};
                }
            }
            return {data, data.end()};
        }
        constexpr fetch end_of_value() const {
            if (!data.empty()) {
                auto it = std::find_if(iter, data.end(), [](char_t c){
                    constexpr std::array end{syntax::array_close, syntax::object_close, syntax::comma};
                    return constexpr_iswspace(c) || std::any_of(end.begin(), end.end(), [c](auto s) { return c == static_cast<char_t>(s); });
                });
                return {data, (it - 1UL)};
            }
            return {data, data.end()};
        }
        constexpr fetch end_of_structure() const {
            int depth{0};
            for (auto it = iter; it <= data.end(); ++it) {
                switch(*it) {
                    case static_cast<char_t>(syntax::string_delimiter):
                        it = fetch(data, it).end_of_string();
                        break;
                    case static_cast<char_t>(syntax::object_open):
                    case static_cast<char_t>(syntax::array_open):
                        ++depth;
                        break;
                    case static_cast<char_t>(syntax::object_close):
                    case static_cast<char_t>(syntax::array_close):
                        if (--depth <= 0) {
                            return {data, it};
                        }
                        break;
                    case static_cast<char_t>(syntax::comma):
                        if (depth <= 0) {
                            return {data, it - 1UL};
                        }
                        break;
                }
            }
            return {data, data.end()};
        }
        constexpr fetch end_of_section() const {
            if (!data.empty()) {
                switch(*iter) {
                    case static_cast<char_t>(syntax::object_open):
                    case static_cast<char_t>(syntax::array_open):
                        return end_of_structure();
                    case static_cast<char_t>(syntax::string_delimiter):
                        return end_of_string();
                    default:
                        return end_of_value();
                }
            }
            return {data, data.end()};
        }
        constexpr bool operator==(syntax c) const {
            return (iter != data.end()) && (*iter == static_cast<char_t>(c));
        }
        constexpr bool operator!=(syntax c) const {
            return !operator==(c);
        }
        constexpr operator std::basic_string_view<char_t>::iterator() const {
            return iter;
        }
        std::basic_string_view<char_t> data;
        std::basic_string_view<char_t>::iterator iter;
    };
    struct iterator {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::size_t;
        using value_type = json_view;
        using reference = void;
        using pointer = void;
        constexpr iterator(std::basic_string_view<char_t> data, const std::basic_string_view<char_t>::iterator& it)
        : data{data}, iter{it}
        {}
        constexpr iterator(std::basic_string_view<char_t> data, const fetch& f)
        : data{data}, iter{static_cast<const std::basic_string_view<char_t>::iterator&>(f)}
        {}
        constexpr iterator(std::basic_string_view<char_t> data, std::size_t index)
        : data{data}, iter{std::min(data.begin() + index, data.end())}
        {}
        constexpr json_view operator*() const {
            auto next = (++(iterator(*this)));
            if (next.iter == (data.end() - 1UL)) {
                return std::basic_string_view<char_t>{iter, next.iter};
            }
            return std::basic_string_view<char_t>{iter, (next.iter - 1UL)};
        }
        constexpr bool operator!=(const iterator& rhs) const {
            return iter != rhs.iter;
        }
        constexpr iterator& operator++() {
            if (!data.empty()) {
                for (; iter <= (data.end() - 1UL); iter = fetch(data, iter).next()) {
                    switch(*iter) {
                        case static_cast<char_t>(syntax::object_close):
                        case static_cast<char_t>(syntax::array_close):
                        case static_cast<char_t>(syntax::colon):
                            break;
                        case static_cast<char_t>(syntax::comma):
                            iter = fetch(data, ++iter).first();
                            return *this;
                        case static_cast<char_t>(syntax::string_delimiter):
                        case static_cast<char_t>(syntax::object_open):
                        case static_cast<char_t>(syntax::array_open):
                        default:
                            iter = fetch(data, iter).end_of_section();
                            break;
                    }
                    if (iter >= (data.end() - 1UL)) {
                        break;
                    }
                }
            }
            return *this;
        }
        std::basic_string_view<char_t> data;
        std::basic_string_view<char_t>::iterator iter;
    };
public:
    constexpr json_view(std::basic_string_view<char_t> json)
    : data{static_cast<std::basic_string_view<char_t>::iterator>(fetch(json).first()), json.end()} {}
    constexpr json_view(json_view&& other)
    : data{std::move(other.data)} {}
    constexpr json_view(const json_view& other)
    : json_view(std::move(other)) {}
    constexpr json_view& operator=(json_view&& other) {
        data = std::move(other.data);
    }
    constexpr json_view& operator=(json_view other) {
        *this = std::move(other);
    }
    constexpr iterator begin() const {
        if (!data.empty()) {
            if ((data.front() == static_cast<char_t>(syntax::object_open)) || (data.front() == static_cast<char_t>(syntax::array_open))) {
                return {data, fetch(data).next()};
            }
        }
        return end();
    }
    constexpr iterator end() const {
        return {data, std::min(data.end(), data.end() - 1UL)};
    }
    constexpr std::size_t size() const {
        return std::distance(begin(), end());
    }
    constexpr json_view key() const {
        fetch last{fetch(data).end_of_section()};
        if (last.next(syntax::colon) != data.end()) {
            return substr(data.begin(), last);
        }
        return std::basic_string_view<char_t>{};
    }
    constexpr json_view value() const {
        fetch last{fetch(data).end_of_section()};
        if (last.next(syntax::colon) != data.end()) {
            fetch start{last.next(syntax::colon).next()};
            return substr(start, start.end_of_section());
        }
        return substr(data.begin(), last);
    }
    constexpr json_view at(std::basic_string_view<char_t> key) const {
        auto it = std::find_if(begin(), end(), [key](auto&& view){
            return view.key().string_view() == key;
        });
        if (it != end()) {
            return (*it).value();
        }
        return std::basic_string_view<char_t>{};
    }
    constexpr json_view at(std::size_t index) const {
        iterator it{begin()};
        for (std::size_t i{0UL}; i < index; ++i, ++it);
        return *it;
    }
    constexpr json_view operator[](std::basic_string_view<char_t> key) const {
        return at(key);
    }
    constexpr json_view operator[](std::size_t index) const {
        return at(index);
    }
    constexpr bool is_object() const {
        return (!data.empty()) && (data.front() == static_cast<char_t>(syntax::object_open)) && (data.back() == static_cast<char_t>(syntax::object_close));
    }
    constexpr bool is_array() const {
        return (!data.empty()) && (data.front() == static_cast<char_t>(syntax::array_open)) && (data.back() == static_cast<char_t>(syntax::array_close));
    }
    constexpr bool is_string() const {
        return (!data.empty()) && (key().data.empty()) && (data.front() == static_cast<char_t>(syntax::string_delimiter));
    }
    constexpr bool is_boolean() const {
        constexpr literal_helper true_value{"true"};
        constexpr literal_helper false_value{"false"};
        auto view = as_string_view();
        return (view == static_cast<const char_t*>(true_value)) ||
               (view == static_cast<const char_t*>(false_value));
    }
    template<typename T = int>
    constexpr bool is_number() const {
        if (!data.empty()) {
            if constexpr (std::is_floating_point_v<T>) {
                constexpr literal_helper allowed{"1234567890-+.eE"}; /// rough estimation
                return as_string_view().find_first_not_of(static_cast<const char_t*>(allowed)) == std::basic_string_view<char_t>::npos;
            } else {
                constexpr literal_helper allowed{"1234567890-+"}; /// rough estimation
                return as_string_view().find_first_not_of(static_cast<const char_t*>(allowed)) == std::basic_string_view<char_t>::npos;
            }
        }
        return false;
    }
    constexpr bool is_null() const {
        constexpr literal_helper null_value{"null"};
        return as_string_view() == static_cast<const char_t*>(null_value);
    }
    constexpr std::basic_string_view<char_t> as_string_view() const {
        if (!data.empty()) {
            return substr(data.begin(), fetch(data).end_of_section());
        }
        return std::basic_string_view<char_t>{};
    }
    constexpr std::basic_string_view<char_t> string_view() const {
        if (!data.empty() && is_string()) {
            return substr((data.begin() + 1UL), (static_cast<std::basic_string_view<char_t>::iterator>(fetch(data).end_of_section()) - 1UL));
        }
        return std::basic_string_view<char_t>{};
    }
    constexpr std::basic_string<char_t> string() const {
        return std::basic_string<char_t>(string_view());
    }
    constexpr bool boolean() const {
        constexpr literal_helper true_value{"true"};
        return as_string_view() == static_cast<const char_t*>(true_value);
    }
    template <typename T = int, typename = std::enable_if_t<std::is_integral_v<T> || std::is_floating_point_v<T>>>
    constexpr T number() const {
        T result{};
        auto&& [ptr, error] {std::from_chars(as_string_view().begin(), as_string_view().end(), result)};
        return ((error != std::errc::invalid_argument) && (ptr == data.end())) ? result : 0;
    }
    constexpr friend std::basic_ostream<char_t>& operator<<(std::basic_ostream<char_t>& os, const json_view& rhs) {
        return os << rhs.data;
    }
    template<std::size_t index> std::tuple_element_t<index, json_view> get() const { /// structured binding
        if constexpr (index == 0UL) {
            return key();
        }
        return value();
    }
private:
    constexpr std::basic_string_view<char_t> substr(const std::basic_string_view<char_t>::iterator& first,
                                                    const std::basic_string_view<char_t>::iterator& last) const {
        return {first, std::min(data.end(), last + 1U)};
    }
private:
    std::basic_string_view<char_t> data;
};
template<> template<>
inline int json_view<wchar_t>::number<int>() const {
    if ((!data.empty()) && is_number<int>()) {
        return std::stoi(std::basic_string<wchar_t>{as_string_view()});
    }
    return 0;
}
template<> template<>
inline float json_view<wchar_t>::number<float>() const {
    if ((!data.empty()) && is_number<float>()) {
        return std::stof(std::basic_string<wchar_t>{as_string_view()});
    }
    return 0;
}
namespace std { /// structured binding
template<typename char_t>
struct tuple_size<::json_view<char_t>>
: public std::integral_constant<std::size_t, 2UL>
{};
template<std::size_t i, typename char_t> struct tuple_element<i, ::json_view<char_t>>
: public conditional<i == 0UL, ::json_view<char_t>, ::json_view<char_t>> {
    static_assert(i < 2UL, "out of bounds");
};
}
using json_str_view = json_view<char>;
using json_wstr_view = json_view<wchar_t>;
