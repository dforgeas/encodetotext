/* Compile with
cl /EHsc /O2 /Wall /wo4710 /wo4820 encodetotext.cpp
 * or
g++ -ansi -pedantic -Wall -march=native -O2 -fno-strict-aliasing -o encodetotext cpp/encodetotext.cpp
*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <deque>
#include <queue>
#ifdef _MSC_VER
#include <unordered_map>
#else
#include <tr1/unordered_map>
#endif
#include <utility>
#include <algorithm>
#include <functional>
#include <cassert>
#include <exception>
#include <limits>
#include <cstring>
#include <climits>
#include <arpa/inet.h>

#include "btea.h"

using namespace std;
using namespace std::tr1;

// + promotes to int, gets the unsigned value even if n is signed
#define to_byte(n) (+static_cast<unsigned char>(n))

#define STATIC static

STATIC uint32 readu32(const char *buffer)
{
   uint32 i;
   memcpy(&i, buffer, sizeof i);
   return ntohl(i);
}

STATIC void writeu32(char *buffer, const uint32 value)
{
   const uint32 i = htonl(value);
   memcpy(buffer, &i, sizeof i);
}

STATIC uint16 readu16(const char *buffer)
{
   uint16 i;
   memcpy(&i, buffer, sizeof i);
   return ntohs(i);
}

STATIC void writeu16(char *buffer, const uint16 value)
{
   const uint16 i = htons(value);
   memcpy(buffer, &i, sizeof i);
}

static uint32 static_key[4] = {3449741923u, 1428823133u, 719882406u, 2957402939u};

#define static_assert(exp) namespace { typedef char static_assert_[(exp) ? 1 : -1]; }
static_assert(CHAR_BIT == 8);
static_assert(sizeof(uint16) == 2)
static_assert(sizeof(uint32) == 4)

struct error: exception
{
   string file, msg, buffer;
   unsigned long line;

   error (string file, unsigned long line, string msg) throw()
      : file(file), msg(msg), buffer(), line(line)
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
      ostringstream out;
      out << '(' << file << ':' << line << ") " << msg;
      buffer = out.str();
   }
   virtual const char* what() const throw()
   {
      return buffer.c_str();
   }
};

static void load_static_key()
{
   ifstream in("encode.key", ios::in | ios::binary);
   if (in)
   {
      char buffer[sizeof static_key];
      in.read(buffer, sizeof static_key);
      if (in.gcount() != sizeof static_key)
      {
         throw error(__FILE__, __LINE__, "invalid key");
      }
      static_key[0] = readu32(buffer);
      static_key[1] = readu32(buffer + 4);
      static_key[2] = readu32(buffer + 8);
      static_key[3] = readu32(buffer + 12);
      cerr << "new key loaded." << endl;
   }
   else
   {
      cerr << "using the default key." << endl;
   }
}

template<typename T1, typename T2>
struct comparable_pair: pair<T1, T2>
{
   comparable_pair(const T1 &a1, const T2 &a2): pair<T1, T2>(a1, a2)
   {
   }

   bool operator < (const pair<T1, T2> &other) const
   {
      return this->first == other.first ?
         this->second < other.second :
         this->first < other.first;
   }

   bool operator > (const pair<T1, T2> &other) const
   {
      return this->first == other.first ?
         this->second > other.second :
         this->first > other.first;
   }
};

static const streamsize BUFFER_SIZE = 4 * sizeof(uint32) << 10; // ensure multiple of sizeof(uint32)

static void pad_and_crypt(char *const buffer, streamsize &bytes_read)
{
   // pad if necessary
   if (bytes_read < BUFFER_SIZE)
   {
      streamsize nb_chars_to_add = sizeof(uint32) - bytes_read % sizeof(uint32);
      if (bytes_read < 4) nb_chars_to_add += 4; // special case because 2 uint32 are the minimum for btea

      streamsize current = bytes_read;
      bytes_read += nb_chars_to_add;
      assert(bytes_read <= BUFFER_SIZE);

      for ( ; current < bytes_read; ++current)
      {
         assert(current < BUFFER_SIZE);
         buffer[current] = static_cast<char>(nb_chars_to_add);
      }
   }

   // convert to native integers
   const streamsize native_buffer_size = BUFFER_SIZE / sizeof(uint32);
   const streamsize data_size = bytes_read / sizeof(uint32);
   uint32 native_buffer[native_buffer_size];
   for (streamsize i = 0; i < data_size; ++i) {
      native_buffer[i] = readu32(buffer + sizeof(uint32) * i);
   }

   // crypt
   BOOL btea_result = btea(native_buffer, data_size, static_key);
   if (not btea_result)
   {
      ostringstream msg;
      msg << "btea failed with size " << data_size;
      throw error(__FILE__, __LINE__, msg.str());
   }

   // convert back to bytes
   for (streamsize i = 0; i < data_size; ++i) {
      writeu32(buffer + sizeof(uint32) * i, native_buffer[i]);
   }
}

static void encode(const deque<string> &words, istream &in, ostream &out)
{
   streamsize bytes_read;
   char buffer[BUFFER_SIZE];
   for ( ; ; )
   {
      in.read(buffer, BUFFER_SIZE); // always read BUFFER_SIZE until EOF
      bytes_read = in.gcount();
      pad_and_crypt(buffer, bytes_read); // buffer and bytes_read are updated
      if (bytes_read == 0) { assert(bytes_read != 0); break; } // never happens because of padding, but the loop will exit on if (!in) break;

      const streamsize data_size = bytes_read / sizeof(uint16);
      for (streamsize i = 0; i < data_size; ++i) {
         out << words[readu16(buffer + sizeof(uint16) * i)] << ' ';
      }

      if (not in) break;
   }
}

namespace buff_option
{
   enum e
   {
      nothing,
      eof,
      size
   };
}

static char *bufferise_data(const uint16 data, buff_option::e option = buff_option::nothing)
{
   static char buffer[BUFFER_SIZE];
   static streamsize data_size = 0;
   // although quite fun, this function would better be a class
   switch (option)
   {
   case buff_option::nothing:
      writeu16(buffer + data_size, data);
      data_size += sizeof data;
      assert(data_size <= BUFFER_SIZE);
      if (data_size == BUFFER_SIZE)
      {
         data_size = 0;
         return buffer; // a buffer full of data is available
      }
      else
         return 0; // more data expected
   case buff_option::eof:
      data_size = 0; // reset for the next time
      return buffer; // the buffer may not be full on eof
   case buff_option::size:
      return reinterpret_cast<char *>(&data_size);
   default:
      assert(false);
      return 0;
   }
}

static void remove_padding(ostream &out, const char *const buffer, streamsize data_size)
{
   static char prev_buffer[BUFFER_SIZE];
   static streamsize prev_data_size = 0;
   if (data_size > 0)
   { // data is available on input
      if (prev_data_size > 0)
      { // normal operation, output previous buffer
         out.write(prev_buffer, prev_data_size);
      }
      // and save the current
      memcpy(prev_buffer, buffer, data_size);
      prev_data_size = data_size;
   }
   else
   { // eof has been hit
      if (prev_data_size > 0)
      {
         const streamsize padding_size = prev_buffer[prev_data_size - 1];

         // check that padding is correct
         if (prev_data_size > 8 and (padding_size < 1 or padding_size > 4)
            or (padding_size < 1 or padding_size > 8))
            goto invalid_padding;
         for (streamsize current = prev_data_size - 2; current > prev_data_size - padding_size; --current)
            if (prev_buffer[current] != static_cast<char>(padding_size))
               goto invalid_padding;

         // output except the padding
         out.write(prev_buffer, prev_data_size - padding_size);
         // do not write it again
         prev_data_size = 0;
      }
      // else out will be empty because no data
   }

   return;

invalid_padding:
   ostringstream msg;
   msg << "invalid padding: " << hex << to_byte(prev_buffer[prev_data_size-4])
      << ' ' << to_byte(prev_buffer[prev_data_size-3])
      << ' ' << to_byte(prev_buffer[prev_data_size-2]) << ' ' << to_byte(prev_buffer[prev_data_size-1]);
   throw error(__FILE__, __LINE__, msg.str());
}

static void decode(const unordered_map<string, uint16> &words_rev, istream &in, ostream &out)
{
   string word;
   while (in >> word)
   {
      unordered_map<string, uint16>::const_iterator words_rev_iter = words_rev.find(word);
      if (words_rev_iter == words_rev.end())
      {
         string msg("unexpected word: `");
         msg += word + '\'';
         throw error(__FILE__, __LINE__, msg);
      }
      char *pbuffer;
      if (0 != (pbuffer = bufferise_data(words_rev_iter->second)))
      { // the buffer is full, process it without padding

         // convert to native integers
         const streamsize data_size = BUFFER_SIZE / sizeof(uint32);
         uint32 native_buffer[data_size];
         for (streamsize i = 0; i < data_size; ++i) {
            native_buffer[i] = readu32(pbuffer + sizeof(uint32) * i);
         }

         // decrypt
         BOOL btea_result = btea(native_buffer, -data_size, static_key);
         if (not btea_result)
         {
            ostringstream msg;
            msg << "btea failed with size " << -data_size;
            throw error(__FILE__, __LINE__, msg.str());
         }

         // convert back to bytes
         for (streamsize i = 0; i < data_size; ++i) {
            writeu32(pbuffer + sizeof(uint32) * i, native_buffer[i]);
         }
         remove_padding(out, pbuffer, BUFFER_SIZE);
      }
   }

   // special case for the last block, which may be partial
   const streamsize data_size = *reinterpret_cast<streamsize *>(bufferise_data(0, buff_option::size));
   char *pbuffer = bufferise_data(0, buff_option::eof);

   // when the previous block ends on a block boundary, there may be no data left
   if (data_size > 0)
   {
      // convert to native integers
      const streamsize native_buffer_size = BUFFER_SIZE / sizeof(uint32);
      const streamsize contents_size = data_size / sizeof(uint32);
      uint32 native_buffer[native_buffer_size];
      for (streamsize i = 0; i < contents_size; ++i) {
         native_buffer[i] = readu32(pbuffer + sizeof(uint32) * i);
      }

      BOOL btea_result = btea(native_buffer, -contents_size, static_key);
      if (not btea_result)
      {
         ostringstream msg;
         msg << "btea failed with size " << -contents_size;
         throw error(__FILE__, __LINE__, msg.str());
      }

      // convert back to bytes
      for (streamsize i = 0; i < contents_size; ++i) {
         writeu32(pbuffer + sizeof(uint32) * i, native_buffer[i]);
      }

      remove_padding(out, pbuffer, data_size);
   }

   remove_padding(out, pbuffer, 0); // flush any bufferised data
}

static bool ends_with(const string& str, const string& end)
{
   return end.size() <= str.size()
      && end == str.substr(str.size() - end.size());
}

static void generate_words(deque<string> &words)
{
   cerr << "opening words.txt..." << endl;
   deque<string> all_words;
   { // delete words_file at the end
      ifstream words_file("words.txt");
      if ( ! words_file)
      {
         throw error(__FILE__, __LINE__, "cannot open words.txt");
      }
      string line;
      while (getline(words_file, line))
      {
         if ( ! line.empty())
            all_words.push_back(line);
      }
   } // delete words_file
   cerr << "sorting words..." << endl;
   // sort in reverse order with greater instead of less
   sort(all_words.begin(), all_words.end(), greater<string>());

   { // delete pqueue_words at the end
      typedef comparable_pair<streamsize, streamsize> elem_t;
      // use greater instead of less to get the lowest elem_t at the top
      priority_queue<elem_t, deque<elem_t>, greater<elem_t> > pqueue_words;

      cerr << "filling the size queue..." << endl;
      streamsize i = 0;
      for (deque<string>::const_iterator iter = all_words.begin()
          ; iter != all_words.end()
          ; ++iter)
      { // negate the size to get the largest word at the top
         pqueue_words.push(elem_t(numeric_limits<streamsize>::max() - iter->size(), i++));
      }

      cerr << "deleting the longest words..." << endl;
      while (pqueue_words.size() > 1 << 16)
      {
         all_words[pqueue_words.top().second].clear();
         pqueue_words.pop();
      }
   } // delete pqueue_words

   cerr << "creating the word list..." << endl;
   for (deque<string>::const_iterator iter = all_words.begin()
       ; iter != all_words.end()
       ; ++iter)
   {
      if ( ! iter->empty())
         words.push_back(*iter);
   }

   if (words.size() != 1 << 16)
   {
      ostringstream msg;
      msg << "word list size is wrong: " << words.size() << " instead of " << (1 << 16);
      throw error(__FILE__, __LINE__, msg.str());
   }
}

static bool quick_start(deque<string> &words)
{
   cerr << "trying to quickstart... " << flush;
   ifstream sorted_words_file("words.quickstart");
   string line;
   while (getline(sorted_words_file, line))
   {
      if ( ! line.empty())
         words.push_back(line);
   }

   streamsize size = words.size();
   bool result = size == 1 << 16;
   if ( ! result) // failure
   {
      words.clear(); // clean before generate_words
      cerr << "failed\n";
   }
   else
      cerr << "success\n";
   return result;
}

static void save_words(const deque<string> &words)
{
   ofstream sorted_words_file("words.quickstart");
   for (deque<string>::const_iterator iter = words.begin()
       ; iter != words.end()
       ; ++iter)
   {
      sorted_words_file << *iter << '\n';
   }
}

static void reverse_words(const deque<string> &words, unordered_map<string, uint16> &words_rev)
{
   cerr << "creating the map for the reversal..." << endl;
   uint16 j = 0;
   for (deque<string>::const_iterator iter = words.begin()
       ; iter != words.end()
       ; ++iter)
   {
      words_rev[*iter] = j++;
   }
   assert(j == 0);
}

#ifndef UNIT_TESTS
static int work(int argc, char *argv[])
{
   string mode;
   ifstream in;
   ofstream out;
   if (argc > 3)
   {
      mode = argv[1];
      if (mode != "enc" && mode != "dec")
      {
         cerr << "invalid mode " << mode << " ; valid is enc or dec\n";
         return 2;
      }
      if (ends_with(argv[3], "words.txt"))
      { // basic protection of not overwriting our database
         cerr << "cannot use words.txt as filename\n";
         return 4;
      }
      in.open(argv[2], ios::in | ios::binary);
      out.open(argv[3], ios::out | ios::binary);
      if (!in)
      {
         cerr << "error opening " << argv[2] << '\n';
         return 3;
      }
      if (!out)
      {
         cerr << "error opening " << argv[3] << '\n';
         return 4;
      }
   }
   else
   {
      cerr << "missing parameters: mode {enc, dec}, filename_in, filename_out\n";
      return 1;
   }

   deque<string> words;
   if ( ! quick_start(words))
   {
      generate_words(words);
      save_words(words);
   }
   load_static_key();

   if (mode == "enc")
   {
      cerr << "encoding the file..." << endl;
      encode(words, in, out);
   }
   else
   {
      unordered_map<string, uint16> words_rev;
      reverse_words(words, words_rev);

      cerr << "decoding the file..." << endl;
      decode(words_rev, in, out);
   }

   return 0;
}

#else // UNIT_TESTS
   #if FULL_TESTS
static void writeToFile(istream& data, const string& filename)
{
   ofstream out(filename.c_str(), std::ios::binary);
   assert(out.is_open());
   out << data.rdbuf(); // this just copies everything
}
   #endif

static int unit_tests(int argc, char *argv[])
{
   #if FULL_TESTS
   for (uint32 i = 0; i <= std::numeric_limits<uint16>::max(); ++i) {
      char buffer[sizeof(uint32)];
      writeu32(buffer, i);
      assert(i == readu32(buffer));
      writeu16(buffer, i);
      assert(i == readu16(buffer));
      for (uint32 j = 0; j < std::numeric_limits<uint16>::max(); ++j) {
         const uint32 value = i << 16 | j;
         writeu32(buffer, value);
         assert(value == readu32(buffer));
      }
   }
   #endif
   streamsize start, stop;
   if (argc > 2)
   {
      {
         istringstream read_start(argv[1]);
         read_start >> start;
         if (read_start.fail())
         {
            cerr << "parameter start must be a number\n";
            return 2;
         }
      }

      {
         istringstream read_stop(argv[2]);
         read_stop >> stop;
         if (read_stop.fail())
         {
            cerr << "parameter stop must be a number\n";
            return 3;
         }
      }

      if (start >= stop)
      {
         cerr << "start must be less than stop\n";
         return 4;
      }
   }
   else
   {
      cerr << "missing parameters: start, stop\n";
      return 1;
   }

   deque<string> words;
   if ( ! quick_start(words))
   {
      generate_words(words);
      save_words(words);
   }
   load_static_key();

   unordered_map<string, uint16> words_rev;
   reverse_words(words, words_rev);

   cerr << "starting tests..."<< endl;
   vector<bool> results(stop - start);

   for (streamsize n = start; n < stop; ++n)
   { // test for size n
      stringstream in, out, result("!"); // impossible result because in starts with 'a'
      for (streamsize i = 0; i < n; ++i)
      {
         in << static_cast<char>(i + 'a');
      }

      try
      {
         /* prepare and encode */
         in.clear();
         assert(0 == in.tellg());
         assert(0 == out.tellp());
         encode(words, in, out);

   #if FULL_TESTS
         /* save the encoded data */
         ostringstream filename;
         filename << "data/out" << n << ".txt";
         writeToFile(out, filename.str());
   #endif

         /* prepare and decode */
         out.clear();
         out.seekg(0);
         assert(0 == out.tellg());
         assert(0 == result.tellp());
         decode(words_rev, out, result);
      }
      catch (const exception& exc)
      {
        cerr << "error (#" << n << "): " << exc.what() << endl;
      } // catch any exception but allow other tests to run

      results[n - start] = in.str() == result.str();
   }

   /* display the results */
   streamsize total = 0, failed = 0;
   cout << "\nFAILED: ";
   for (vector<bool>::const_iterator it = results.begin()
       ; it != results.end()
       ; ++it)
   {
      ++total;
      if ( ! *it)
      {
         ++failed;
         cout << (start + (it - results.begin())) << ' ';
      }
   }
   cout << '\n';
   cout << "failed " << (failed * 100 / total) << "%\n";
   return 0;
}
#endif

int main(int argc, char *argv[])
{
   try
   {
   #ifndef UNIT_TESTS
      return work(argc, argv);
   #else
      return unit_tests(argc, argv);
   #endif
   }
   catch (const exception& exc)
   {
      cerr << "error: " << exc.what() << endl;
      return 0xf;
   }
}
