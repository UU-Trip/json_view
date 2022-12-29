#pragma once

#include <algorithm>
#include <charconv>
#include <iterator>
#include <ostream>
#include <string_view>
#include <type_traits>
#include <utility>

template <typename char_t> class json_view {
    template <typename enum_class_t>
    constexpr static auto e_val(enum_class_t&& val) {
        return static_cast<std::underlying_type_t<std::decay_t<enum_class_t>>>(std::forward<enum_class_t>(val));
    }
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
        constexpr fetch(std::basic_string_view<char_t> data, std::size_t index = 0)
        : data{data}, index{index}
        {}
        constexpr fetch first() const {
            std::size_t pos{index};
            while ((pos != data.size()) && std::iswspace(data.at(pos))) {
                ++pos;
            }
            return {data, pos};
        }
        constexpr fetch next() const {
            return fetch(data, std::min(index + 1, data.size())).first();
        }
        constexpr fetch next(syntax c) const {
            fetch r{next()};
            while ((r != data.size()) && (r != c)) {
                r = r.next();
            }
            return r;
        }
        constexpr fetch end_of_section() const {
            std::size_t pos{index};
            if (!data.empty()) {
                if (data.at(pos) == e_val(syntax::string_delimiter)) {
                    while(data.at(++pos) != e_val(syntax::string_delimiter)) {
                        if (data.at(pos) == e_val(syntax::escape)) {
                            ++pos;
                        }
                    };
                    return {data, pos};
                } else {
                    std::size_t depth{0};
                    while(pos != data.size()) {
                        switch(data.at(pos)) {
                            case e_val(syntax::string_delimiter):
                                pos = fetch(data, pos).end_of_section();
                                break;
                            case e_val(syntax::object_open):
                            case e_val(syntax::array_open):
                                ++depth;
                                break;
                            case e_val(syntax::object_close):
                            case e_val(syntax::array_close):
                                if (!(--depth)) {
                                    return {data, pos};
                                }
                                break;
                        }
                        ++pos;
                    }
                }
            }
            return {data};
        }
        constexpr bool operator==(syntax c) const {
            return (index < data.size()) && (data.at(index) == e_val(c));
        }
        constexpr bool operator!=(syntax c) const {
            return !operator==(c);
        }
        constexpr bool operator==(std::size_t pos) const {
            return index == pos;
        }
        constexpr bool operator!=(std::size_t pos) const {
            return !operator==(pos);
        }
        constexpr operator std::size_t() const {
            return index;
        }
        std::basic_string_view<char_t> data;
        std::size_t index;
    };
    struct iterator {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::size_t;
        using value_type = json_view;
        using reference = void;
        using pointer = void;
        constexpr iterator(std::basic_string_view<char_t> data)
        : data{data}, index{std::min(data.size(), fetch(data).first() + 1)}
        {}
        constexpr json_view operator*() const {
            return data.substr(std::min(data.size(), index), ((((++(iterator(*this))).index - 1) - index)));
        }
        constexpr bool operator!=(const iterator& rhs) const {
            return (index != rhs.index);
        }
        constexpr iterator& operator++() {
            for (; index < data.size(); ++index) {
                switch(data.at(index)) {
                    case e_val(syntax::string_delimiter):
                    case e_val(syntax::object_open):
                    case e_val(syntax::array_open):
                        index = fetch(data, index).end_of_section();
                        break;
                    case e_val(syntax::comma):
                        ++index;
                        return *this;
                    default:
                        break;
                }
            }
            return *this;
        }
        std::basic_string_view<char_t> data;
        std::size_t index;
    };
public:
    constexpr json_view(std::basic_string_view<char_t> data)
    : data{data} {}
    constexpr iterator begin() const {
        return data;
    }
    constexpr iterator end() const {
        iterator it{data};
        it.index = data.size();
        return it;
    }
    constexpr std::size_t size() const {
        return std::distance(begin(), end());
    }
    constexpr json_view key() const {
        fetch first{fetch(data).first()};
        fetch last{first.end_of_section()};
        if (last.next(syntax::colon) != data.size()) {
            return substr(first, last);
        }
        return data.substr(0, 0);
    }
    constexpr json_view value() const {
        fetch first{fetch(data).first()};
        fetch last{first.end_of_section()};
        if (last.next(syntax::colon) != data.size()) {
            fetch start{last.next(syntax::colon).next()};
            return substr(start, start.end_of_section());
        }
        return substr(first, last);
    }
    constexpr json_view at(std::basic_string_view<char_t> key) const {
        for (auto view : *this) {
            if (view.key().string_view() == key) {
                return view.value();
            }
        }
        return data.substr(0, 0);
    }
    constexpr json_view at(std::size_t index) const {
        iterator it{data};
        for (std::size_t i{0}; i < index; ++i) {
            ++it;
        }
        return *it;
    }
    constexpr json_view operator[](std::basic_string_view<char_t> key) const {
        return at(key);
    }
    constexpr json_view operator[](std::size_t index) const {
        return at(index);
    }
    constexpr std::basic_string_view<char_t> string_view() const {
        if (!data.empty()) {
            fetch first{fetch(data).first()};
            fetch last{first.end_of_section()};
            if (last.next() == data.size()) {
                if (data.at(first) == e_val(syntax::string_delimiter)) {
                    return substr(first + 1, last - 1);
                }
                return substr(first, last);
            }
        }
        return data;
    }
    constexpr std::basic_string<char_t> string() const {
        return std::basic_string<char_t>(string_view());
    }
    constexpr bool boolean() const {
        return (string_view() == "true");
    }
    constexpr int64_t integer() const {
        int64_t result{0};
        if (!data.empty()) {
            auto str{string_view()};
            std::from_chars(str.begin(), str.end(), result);
        }
        return result;
    }
    constexpr float floating_point() const {
        try {
            return std::stof(string());
        } catch(std::exception& e) {
            return 0;
        }
    }
    constexpr friend std::basic_ostream<char_t>& operator<<(std::basic_ostream<char_t>& os, const json_view& rhs) {
        return os << rhs.data;
    }
    constexpr operator std::basic_string<char_t>() const {
        return string();
    }
private:
    constexpr std::basic_string_view<char_t> substr(std::size_t first, std::size_t last) const {
        return data.substr(first, std::min(data.size(), last + 1) - first);
    }
private:
    std::basic_string_view<char_t> data;
};
using json_str_view = json_view<char>;
using json_wstr_view = json_view<wchar_t>;
