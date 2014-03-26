/*
 * curl++.cpp
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

#include "curl++.h"
#include <algorithm>
#include <ostream>
#include <cstdio>
#include <cstring>

#define CHECK(x) \
    CURLcode code = (x); \
    if(code != CURLE_OK) throw error(#x, code, __FILE__, __LINE__);

#define ERROR(x) \
    error(x, __FILE__, __LINE__)

namespace curl
{

////////////////////////////////////////////////////////////////////////////////
init::init(long flags)
{
    curl_global_init(flags);
}

init::~init()
{
    curl_global_cleanup();
}



////////////////////////////////////////////////////////////////////////////////
easy::easy()
    : handle_(curl_easy_init())
{
    if(handle_ == nullptr)
        throw ERROR("Failed to aquire CURL easy handle");
}

easy::easy(CURL * handle)
    : handle_(handle)
{
    if(handle_ == nullptr)
        throw ERROR("Failed to aquire CURL easy handle");
}

easy::~easy()
{
    curl_easy_cleanup(handle_);
}

void easy::set(CURLoption option, long value)
{
    CHECK(curl_easy_setopt(handle_, option, value));
}

void easy::set(CURLoption option, const std::string & value)
{
    CHECK(curl_easy_setopt(handle_, option, value.c_str()));
}

void easy::add_cookie(const char * cookie)
{
    CHECK(curl_easy_setopt(handle_, CURLOPT_COOKIELIST, cookie));
}

void easy::add_cookie(const std::string& cookie)
{
    CHECK(curl_easy_setopt(handle_, CURLOPT_COOKIELIST, cookie.c_str()));
}

void easy::add_cookie(const std::map<std::string, std::string> & cookiemap)
{
    std::string cookie;
    std::string::size_type size = 0;

    // find maximum size needed
    for(auto&& c : cookiemap)
    {
        size = std::max(size,
            std::get<0>(c).size()
          + std::get<1>(c).size()
          + sizeof("Set-Cookie: =;\n"));
    }
    cookie.reserve(size+1);

    // add cookies
    for(auto&& c : cookiemap)
    {
        cookie.clear();
        cookie.append("Set-Cookie: ");
        cookie.append(std::get<0>(c));
        cookie.push_back('=');
        cookie.append(std::get<1>(c));
        cookie.push_back(';');
        curl_easy_setopt(handle_, CURLOPT_COOKIELIST, cookie.c_str());
    }
}

easy easy::duplicate() const
{
    return easy(curl_easy_duphandle(handle_));
}

void easy::pause(int bitmask)
{
    CHECK(curl_easy_pause(handle_, bitmask));
}

void easy::perform()
{
    CHECK(curl_easy_perform(handle_));
}

void easy::reset()
{
    curl_easy_reset(handle_);
}

std::string easy::get()
{
    std::string buffer;
    recv_into(buffer);
    return buffer;
}

void easy::recv_into(std::string& buffer)
{
    set(CURLOPT_WRITEDATA, &buffer);
    set(CURLOPT_WRITEFUNCTION, easy::recv_into_writefunc_string_);
    perform();
}

void easy::recv_into(std::ostream& stream)
{
    set(CURLOPT_WRITEDATA, &stream);
    set(CURLOPT_WRITEFUNCTION, easy::recv_into_writefunc_stream_);
    perform();
}

void easy::recv_into(FILE * file)
{
    set(CURLOPT_WRITEDATA, file);
    set(CURLOPT_WRITEFUNCTION, easy::recv_into_writefunc_file_);
    perform();
}


bool easy::operator == (const easy& other) const
{
    return handle_ == other.handle_;
}

bool easy::operator != (const easy& other) const
{
    return handle_ != other.handle_;
}

bool easy::operator <  (const easy& other) const
{
    return handle_ < other.handle_;
}

bool easy::operator >  (const easy& other) const
{
    return handle_ > other.handle_;
}

bool easy::operator <= (const easy& other) const
{
    return handle_ <= other.handle_;
}

bool easy::operator >= (const easy& other) const
{
    return handle_ >= other.handle_;
}

size_t easy::recv_into_writefunc_string_(
    char *ptr, size_t size, size_t nmemb, std::string * buffer)
{
    buffer->append(ptr, size*nmemb);
    return size*nmemb;
}

size_t easy::recv_into_writefunc_stream_(
    char *ptr, size_t size, size_t nmemb, std::ostream * stream)
{
    stream->write(ptr, size*nmemb);
    return size*nmemb;
}

size_t easy::recv_into_writefunc_file_(
    char *ptr, size_t size, size_t nmemb, FILE * file)
{
    return fwrite(ptr, size, nmemb, file);
}


////////////////////////////////////////////////////////////////////////////////
list::list()
    : list_(nullptr)
{}

list::list(curl_slist * sl)
    : list_(sl)
{}

list::~list()
{
    curl_slist_free_all(list_);
}

void list::append(const char * value)
{
    list_ = curl_slist_append(list_, value);
}

void list::append(std::string& value)
{
    append(value.c_str());
}

list& list::operator += (const char * value)
{
    append(value);
    return *this;
}

list& list::operator += (const std::string& value)
{
    append(value.c_str());
    return *this;
}

curl_slist * list::operator * () const
{
    return list_;
}



////////////////////////////////////////////////////////////////////////////////
error::error(const char * msg)
    : buf_(strdup(msg))
{}

error::error(const char * msg, const char * file, int line)
{
    int size;
    const char * fmt = "File: %s - Line: %d\n%s\n";

    // find appropriate size for buffer
    size = snprintf(nullptr, 0, fmt, file, line, msg);

    // allocate buffer
    buf_ = static_cast<char *>(malloc(size+1));

    // generate formatted error message
    snprintf(buf_, size, fmt, file, line, msg);
}

error::error(const char * msg, CURLcode code, const char * file, int line)
{
    int size;
    const char * errstr = curl_easy_strerror(code);
    const char * fmt = "File: %s - Line: %d\n%s\n----------\n%s\n";

    // find appropriate size for buffer
    size = snprintf(nullptr, 0, fmt, file, line, msg, errstr);

    // allocate buffer
    buf_ = static_cast<char *>(malloc(size+1));

    // generate formatted error message
    snprintf(buf_, size, fmt, file, line, msg, errstr);
}

error::~error()
{
    free(buf_);
}

const char * error::what() const noexcept
{
    return buf_;
}

}

#undef ERROR
#undef CHECK
