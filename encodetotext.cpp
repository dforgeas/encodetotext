#include "encodetotext.hpp"
#include "crypto.hpp"

#include <iostream>
#include <fstream>
#include <utility>
#include <iterator>
#include <algorithm>
#include <functional>
#include <cassert>
#include <limits>
#include <cstring>
#include <climits>
#include <arpa/inet.h>

using namespace std;

// + promotes to int, gets the unsigned value even if n is signed
#define to_byte(n) (+static_cast<unsigned char>(n))

static_assert(CHAR_BIT == 8, "char isn't 8 bits");
static_assert(sizeof(uint16_t) == 2, "uint16_t isn't 16 bits");
static_assert(sizeof(uint32) == 4, "uint32 isn't 32 bits");

static uint32 readu32(const char *buffer)
{
   uint32 i;
   memcpy(&i, buffer, sizeof i);
   return ntohl(i);
}

static void writeu32(char *buffer, const uint32 value)
{
   const uint32 i = htonl(value);
   memcpy(buffer, &i, sizeof i);
}

static uint16_t readu16(const char *buffer)
{
   uint16_t i;
   memcpy(&i, buffer, sizeof i);
   return ntohs(i);
}

static void writeu16(char *buffer, const uint16_t value)
{
   const uint16_t i = htons(value);
   memcpy(buffer, &i, sizeof i);
}

static uint32 static_key[4] = {3449741923u, 1428823133u, 719882406u, 2957402939u};

void load_static_key()
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

      for (size_t i = 0; i < sizeof static_key / sizeof *static_key; ++i)
      {
         static_key[i] = readu32(buffer + i * sizeof *static_key);
      }
      cerr << "new key loaded." << endl;
   }
   else
   {
      cerr << "using the default key." << endl;
   }
}

constexpr streamsize BUFFER_SIZE = CbcMac::stateSize * sizeof(uint32) << 10; // ensure multiple of sizeof(uint32) and CbcMac::stateSize

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

void encode(const vector<string> &words, istream &in, ostream &out)
{
   char buffer[BUFFER_SIZE];
   for ( ; ; )
   {
      in.read(buffer, BUFFER_SIZE); // always read BUFFER_SIZE until EOF
      streamsize bytes_read = in.gcount();
      pad_and_crypt(buffer, bytes_read); // buffer and bytes_read are updated
      if (bytes_read == 0) { assert(bytes_read != 0); break; } // never happens because of padding, but the loop will exit below

      const streamsize data_size = bytes_read / sizeof(uint16_t);
      for (streamsize i = 0; i < data_size; ++i) {
         out << words[readu16(buffer + sizeof(uint16_t) * i)] << ' ';
      }

      if (not in.good()) break; // break if fail() or eof()
   }
}

class Buffers
{
   char data[2][BUFFER_SIZE] = {};
   streamsize sizes[2] = {};
   bool current = false;
public:
   void flip() { current = not current; }
   char *first() { return data[current]; }
   char *second() { return data[not current]; }
   streamsize& firstSize() { return sizes[current]; }
   streamsize& secondSize() { return sizes[not current]; }
};

static char *bufferise_data(Buffers &buffers, const uint16_t data)
{
   streamsize& data_size = buffers.firstSize();
   writeu16(buffers.first() + data_size, data);
   data_size += sizeof data;
   if (data_size == BUFFER_SIZE)
   {
      return buffers.first(); // a buffer full of data is available
   }
   else
   {
      return 0; // more data expected
   }
}

static void remove_padding(Buffers &buffers, ostream &out)
{
   const char *const prev_buffer = buffers.second();
   streamsize &prev_data_size = buffers.secondSize();
   if (buffers.firstSize() > 0)
   { // data is available on input
      if (prev_data_size > 0)
      { // normal operation, output previous buffer
         out.write(prev_buffer, prev_data_size);
         prev_data_size = 0;
      }
      // and save the current buffer
      buffers.flip(); // invalidates prev_buffer and prev_data_size
      return;
   }
   else
   { // eof has been hit
      if (prev_data_size > 0)
      {
         const streamsize padding_size = to_byte(prev_buffer[prev_data_size - 1]);

         // check that padding is correct
         if (padding_size > prev_data_size)
            goto invalid_padding;
         if (prev_data_size > 8 and (padding_size < 1 or padding_size > 4)
            or (padding_size < 1 or padding_size > 8))
            goto invalid_padding;
         for (auto i = prev_data_size - 2; i >= prev_data_size - padding_size; --i)
            if (to_byte(prev_buffer[i]) != padding_size)
               goto invalid_padding;

         // output except the padding
         out.write(prev_buffer, prev_data_size - padding_size);
         // do not write it again
         prev_data_size = 0;
      }
      // else out will be empty because no data
      return;
   }

invalid_padding:
   ostringstream msg;
   const streamsize padding_size = to_byte(prev_buffer[prev_data_size - 1]);
   const streamsize begin = std::max<streamsize>(0, prev_data_size - padding_size), end = prev_data_size;
   msg << "invalid padding[" << begin << ';' << end << "):" << hex;
   for (auto i = begin; i < end; ++i)
      msg << ' ' << to_byte(prev_buffer[i]);
   throw error(__FILE__, __LINE__, msg.str());
}

void decode(const unordered_map<string, uint16_t> &words_rev, istream &in, ostream &out)
{
   Buffers buffers;
   string word;
   while (in >> word)
   {
      const auto words_rev_iter = words_rev.find(word);
      if (words_rev_iter == words_rev.end())
      {
         string msg("unexpected word: `");
         msg += word + '\'';
         throw error(__FILE__, __LINE__, msg);
      }
      char *pbuffer;
      if (0 != (pbuffer = bufferise_data(buffers, words_rev_iter->second)))
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
         remove_padding(buffers, out);
      }
   }

   // special case for the last block, which may be partial
   const streamsize data_size = buffers.firstSize();
   char *const pbuffer = buffers.first();

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

      remove_padding(buffers, out);
   }

   remove_padding(buffers, out); // flush any buffered data
}

void generate_words(vector<string> &words)
{
   cerr << "opening words.txt..." << endl;
   vector<string> all_words;
   { // destroy words_file at the end
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
   } // destroy words_file

   if (all_words.size() < 1 << 16)
   {
      ostringstream msg;
      msg << "word.txt is too small: " << all_words.size() << " instead of " << (1 << 16);
      throw error(__FILE__, __LINE__, msg.str());
   }

   cerr << "gathering the smallest words of the list..." << endl;
   nth_element(all_words.begin(), all_words.begin() + (1 << 16), all_words.end(),
      [](const string& a, const string& b)
      {
         const auto la(a.length()), lb(b.length());
         if (la < lb) return true;
         if (la > lb) return false;
         return a < b;
      });

   cerr << "creating the word list..." << endl;
   all_words.erase(all_words.begin() + (1 << 16), all_words.end());
   // the memory isn't released but this is fine

   sort(all_words.begin(), all_words.end(), greater<string>()); // restore the reverse lexical order of the shortest words
   all_words.swap(words);

   if (words.size() != 1 << 16)
   {
      ostringstream msg;
      msg << "word list size is wrong: " << words.size() << " instead of " << (1 << 16);
      throw error(__FILE__, __LINE__, msg.str());
   }
}

bool quick_start(vector<string> &words)
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

void save_words(const vector<string> &words)
{
   ofstream sorted_words_file("words.quickstart");
   for (auto &word: words)
   {
      sorted_words_file << word << '\n';
   }
}

void reverse_words(const vector<string> &words, unordered_map<string, uint16_t> &words_rev)
{
   cerr << "creating the map for the reversal..." << endl;
   uint16_t j = 0;
   for (auto &word: words)
   {
      words_rev[word] = j++;
   }
   assert(j == 0);
}
