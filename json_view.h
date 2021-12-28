#pragma once

#include <algorithm>
#include <charconv>
#include <iterator>
#include <map>
#include <ostream>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
template <typename enum_class_t> constexpr auto e_val(enum_class_t val) {
    return static_cast<std::underlying_type_t<enum_class_t>>(val);
}
}
template <typename char_t> class json_view {
    enum class syntax : char {
        object_open      = '{',
        object_close     = '}',
        array_open       = '[',
        array_close      = ']',
        colon            = ':',
        comma            = ',',
        string_delimiter = '"',
        escape           = '\\'
    };
    struct iterator {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::size_t;
        using value_type = json_view;
        using reference = void;
        using pointer = void;
        iterator(std::basic_string_view<char_t> data) :
        data{data}, index{std::min(data.size(), static_cast<typename std::basic_string_view<char_t>::size_type>(1))}, depth{0} {}
        json_view operator*() const {
            return data.substr(index, ((((++(iterator(*this))).index - 1) - index)));
        }
        bool operator!=(const iterator& rhs) const {
            return (index != rhs.index);
        }
        iterator& operator++() {
            for (; index < data.size(); ++index) {
                switch(data.at(index)) {
                    case e_val(syntax::string_delimiter):
                        while ((data.at(++index) != e_val(syntax::string_delimiter)) && (index < data.size())) {
                            if (data.at(index) == e_val(syntax::escape)) {
                                ++index;
                            }
                        }
                        break;
                    case e_val(syntax::object_open):
                    case e_val(syntax::array_open):
                        ++depth;
                        break;
                    case e_val(syntax::object_close):
                    case e_val(syntax::array_close):
                        --depth;
                        break;
                    case e_val(syntax::comma):
                        if (!depth) {
                            ++index;
                            return *this;
                        }
                        break;
                    default:
                        break;
                }
            }
            return *this;
        }
        const std::basic_string_view<char_t> data;
        std::size_t index;
        std::size_t depth;
    };
public:
    json_view(std::basic_string_view<char_t> data)
    : data{data} {}
    json_view key() const {
        if (!data.empty() && (data.at(0) == e_val(syntax::string_delimiter)) && (next(syntax::colon) != data.size())) {
            return data.substr(0, next(syntax::colon));
        }
        return data.substr(0, 0);
    }
    json_view value() const {
        if (key()) {
            return data.substr(next(syntax::colon) + 1);
        } else {
            return data.substr(0);
        }
    }
    json_view at(std::basic_string_view<char_t> key) const {
        for (auto view : *this) {
            if (view.key().string() == key) {
                return view.value();
            }
        }
        return data.substr(0, 0);
    }
    json_view at(std::size_t index) const {
        iterator it{data};
        for (std::size_t i{0}; i < index; ++i) {
            ++it;
        }
        return *it;
    }
    json_view operator[](std::basic_string_view<char_t> key) const {
        return at(key);
    }
    json_view operator[](std::size_t index) const {
        return at(index);
    }
    std::size_t size() const {
        return std::distance(begin(), end());
    }
    iterator begin() const {
        return data;
    }
    iterator end() const {
        iterator it{data};
        it.index = data.size();
        return it;
    }
    std::basic_string_view<char_t> string() const {
        if (data.at(0) == e_val(syntax::string_delimiter)) {
            return data.substr(1, data.size() - 2);
        }
        return data;
    }
    bool boolean() const {
        return (string() == "true");
    }
    int64_t integer() const {
        int64_t result{0};
        if (!data.empty()) {
            auto str{string()};
            std::from_chars(str.begin(), str.end(), result);
        }
        return result;
    }
    std::vector<json_view> array() const {
        std::vector<json_view> vector{};
        for (auto view : *this) {
            vector.emplace_back(view);
        }
        return vector;
    }
    std::map<std::basic_string_view<char_t>, json_view> object() const {
        std::map<std::basic_string_view<char_t>, json_view> map;
        for (auto view : *this) {
            if (key()) {
                map.emplace(std::make_pair(view.key().string(), view.value()));
            } else {
                return {};
            }
        }
        return map;
    }
    friend std::basic_ostream<char_t>& operator<<(std::basic_ostream<char_t>& os, const json_view& rhs) {
        return os << rhs.data;
    }
private:
    explicit operator bool() const {
        return !data.empty();
    }
    std::size_t next(syntax c) const {
        std::size_t i{0};
        for(; (i < data.size()) && (data.at(i) != e_val(c)); ++i) {
            while(data.at(++i) != e_val(syntax::string_delimiter) && (i < data.size())) {
                if (data.at(i) == e_val(syntax::escape)) {
                    ++i;
                }
            }
        }
        return i;
    }
private:
    const std::basic_string_view<char_t> data;
};

using json_str_view = json_view<char>;
using json_wstr_view = json_view<wchar_t>;
