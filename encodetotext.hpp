#include <cstdint>
#include <exception>
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>

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

void load_static_key();
void encode(const std::vector<std::string> &words, std::istream &in, std::ostream &out);
void decode(const std::unordered_map<std::string, std::uint16_t> &words_rev, std::istream &in, std::ostream &out);
void generate_words(std::vector<std::string> &words);
bool quick_start(std::vector<std::string> &words);
void save_words(const std::vector<std::string> &words);
void reverse_words(const std::vector<std::string> &words, std::unordered_map<std::string, std::uint16_t> &words_rev);
