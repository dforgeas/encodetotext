#pragma once
#include <cstdint>
#include <exception>
#include <string>
#include <cstring>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <array>
#include <functional>

struct error: std::exception
{
   std::string file, msg, buffer;
   std::streamsize line;

   error (std::string file, std::streamsize line, std::string msg) throw()
      : file(std::move(file)), msg(std::move(msg)), buffer(), line(std::move(line))
   {
      set_buffer();
   }
   error (const error& exc) throw()
      : file(exc.file), msg(exc.msg), buffer(), line(exc.line)
   {
      set_buffer();
   }
   error& operator= (const error& exc) throw()
   {
      file = exc.file;
      line = exc.line;
      msg = exc.msg;
      set_buffer();
      return *this;
   }
   virtual ~error() throw()
   {}
   virtual void set_buffer() throw()
   {
      std::ostringstream out;
      out << '(' << file << ':' << line << ") " << msg;
      buffer = out.str();
   }
   virtual const char* what() const throw()
   {
      return buffer.c_str();
   }
};

struct small_string: std::array<char, 8>
{
   std::size_t length() const
   {
      const char *p = static_cast<const char*>(std::memchr(data(), 0, size()));
      return p ? p - data() : size();
   }

   bool empty() const
   {
      return front() == 0;
   }

   std::string operator + (const char c) const
   {
      std::string r(data(), length());
      r += c;
      return r;
   }

   template <std::size_t S>
   bool operator == (const char (&a)[S]) const
   {
      return std::memcmp(data(), a, S) == 0 && zeroes_from(S);
   }

   bool operator == (const char c) const
   {
      return front() == c and zeroes_from(1);
   }

   bool zeroes_from(std::size_t i) const
   {
      for (; i < size(); ++i)
         if (operator[](i) != 0)
            return false;
      return true;
   }
};

template <class T>
inline T min(T t, T u)
{
   return t < u ? t : u;
}

inline std::istream& operator >> (std::istream &in, small_string &s)
{
   std::string temp;
   if (in >> temp)
   {
      if (temp.length() > s.size())
      {
         in.setstate(std::ios::failbit); // string too long
         // set s anyway
      }
      const auto length = min(s.size(), temp.length());
      std::memcpy(s.data(), temp.data(), length);
      std::memset(s.data() + length, 0, s.size() - length);
   }
   return in;
}

inline std::ostream& operator << (std::ostream &out, const small_string &s)
{
   return out.write(s.data(), s.length());
}

namespace std
{
   template<> struct hash<small_string>
   {
      typedef small_string argument_type;
      typedef std::size_t result_type;
      result_type operator () (const argument_type &s) const
      {
         result_type h = 0x9e3779b9;
         for (auto p = s.data(); p < s.data() + s.size() && *p; ++p)
         {
            h ^= *p;
            h = (h << 7) ^ (h >> 25);
         }
         return h;
      }
   };
}

void load_static_key();
void encode(const std::vector<small_string> &words, std::istream &in, std::ostream &out);
void decode(const std::unordered_map<small_string, std::uint16_t> &words_rev, std::istream &in, std::ostream &out);
void generate_words(std::vector<small_string> &words);
bool quick_start(std::vector<small_string> &words);
void save_words(const std::vector<small_string> &words);
void reverse_words(const std::vector<small_string> &words, std::unordered_map<small_string, std::uint16_t> &words_rev);
