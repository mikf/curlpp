/*
 * curl++.h
 *
 * Copyright 2014 Mike Fährmann <mike_faehrmann@web.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CURLPP_H
#define CURLPP_H

#include <curl/curl.h>
#include <exception>
#include <utility>
#include <string>
#include <iosfwd>
#include <map>
#include <cstdio>

namespace curl
{

////////////////////////////////////////////////////////////////////////////////
// RAII for curl_global_init/_cleanup
class init
{
public:
    explicit init(long flags = CURL_GLOBAL_ALL);
    ~init();

private:
    init(const init& other) = delete;
    init(init&& other) = delete;
    init& operator = (const init& other) = delete;
    init& operator = (init&& other) = delete;
};


////////////////////////////////////////////////////////////////////////////////
// easy-handle wrapper
class easy
{
public:
    easy();
    explicit easy(CURL * handle);
    ~easy();

    // Only allow move semantics, as it is impossible to create an exact copy
    // of curl_easy handles.
    // Use the `duplicate` method to get a copy created by curl_easy_duphandle.
    // (http://curl.haxx.se/libcurl/c/curl_easy_duphandle.html)
    easy(const easy& other) = delete;
    easy(easy&& other) = default;
    easy& operator = (const easy& other) = delete;
    easy& operator = (easy&& other) = default;

    // Generic methods to invoke curl_easy_setopt.
    // (http://curl.haxx.se/libcurl/c/curl_easy_setopt.html).
    void set(CURLoption option, long value);
    void set(CURLoption option, const std::string& value);
    template <typename T>
    void set(CURLoption option, const T * value);
    template <typename Ret, typename... Args>
    void set(CURLoption option, Ret (*value)(Args...));

    void add_cookie(const char * cookie);
    void add_cookie(const std::string& cookie);
    void add_cookie(const std::map<std::string, std::string> & cookies);

    //
    easy duplicate() const;
    void pause(int bitmask);
    void perform();
    void reset();

    //
    std::string get();
    void recv_into(std::string&);
    void recv_into(std::ostream&);
    void recv_into(FILE *);

    //
    std::string escape(const char * str, int len = 0);
    std::string escape(const std::string& str);
    std::string escape(std::string&& str);
    std::string unescape(const char * str, int len = 0);
    std::string unescape(const std::string& str);
    std::string unescape(std::string&& str);

    //
    bool operator == (const easy& other) const;
    bool operator != (const easy& other) const;
    bool operator <  (const easy& other) const;
    bool operator >  (const easy& other) const;
    bool operator <= (const easy& other) const;
    bool operator >= (const easy& other) const;

    inline void swap(easy& other)
    { std::swap(handle_, other.handle_); }

    inline CURL * handle()
    { return handle_; }

private:
    CURL * handle_;

    static size_t recv_into_writefunc_string_(char *, size_t, size_t, std::string *);
    static size_t recv_into_writefunc_stream_(char *, size_t, size_t, std::ostream *);
    static size_t recv_into_writefunc_file_  (char *, size_t, size_t, FILE *);
};

inline void swap(easy& lhs, easy& rhs)
{ lhs.swap(rhs); }



////////////////////////////////////////////////////////////////////////////////
// curl_slist wrapper

class list
{
public:
    list();
    explicit list(curl_slist * sl);
    ~list();

    list(const list& other) = delete;
    list(list&& other) = default;
    list& operator = (const list& other) = delete;
    list& operator = (list&& other) = default;

    void append(const char *);
    void append(std::string&);

    list& operator += (const char *);
    list& operator += (const std::string&);
    curl_slist * operator * () const;

    inline void swap(list& other)
    { std::swap(list_, other.list_); }

private:
    curl_slist * list_;
};

inline void swap(list& lhs, list& rhs)
{ lhs.swap(rhs); }



////////////////////////////////////////////////////////////////////////////////
// default exception class
class error
    : public std::exception
{
public:
    explicit error(const char * msg);
    error(const char * msg, const char * file, int line);
    error(const char * msg, CURLcode code, const char * file, int line);
    virtual ~error();

    virtual const char * what() const noexcept;

private:
    char * buf_;
};



////////////////////////////////////////////////////////////////////////////////
// constants

constexpr const char * firefox27 = "Mozilla/5.0 (X11; Linux x86_64; rv:27.0) Gecko/20100101 Firefox/27.0";



////////////////////////////////////////////////////////////////////////////////
// Template method implementations

#define CHECK(x) \
    CURLcode code = (x); \
    if(code != CURLE_OK) throw error(#x, code, __FILE__, __LINE__);

template <typename T>
void easy::set(CURLoption option, const T * value)
{
    CHECK(curl_easy_setopt(handle_, option, value));
}
template <typename Ret, typename... Args>
void easy::set(CURLoption option, Ret (*value)(Args...))
{
    CHECK(curl_easy_setopt(handle_, option, value));
}

#undef CHECK

}

#endif /* CURLPP_H */
