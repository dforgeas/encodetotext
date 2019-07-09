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
   ifstream in("encode.key", ios::binary);
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
      clog << "new key loaded." << endl;
   }
   else
   {
      clog << "using the default key." << endl;
   }
}

constexpr streamsize BUFFER_SIZE = CbcMac::stateSize * sizeof(uint32) << 10; // ensure multiple of sizeof(uint32) and CbcMac::stateSize
constexpr streamsize NATIVE_BUFFER_SIZE = BUFFER_SIZE / sizeof(uint32);

static void mac_process_buffer(uint32 *const native_buffer, const streamsize size, CbcMac &mac)
{
   streamsize i = 0;
   for (i = 0; i < size - CbcMac::stateSize; i += CbcMac::stateSize)
   {
      mac.update(reinterpret_cast<uint32 const (&)[CbcMac::stateSize]>(native_buffer[i]));
   }

   // update last (potentially partial) block
   uint32 mac_buffer[CbcMac::stateSize] = {};
   for (auto p = mac_buffer; i < size; ++i)
   {
      *p++ = native_buffer[i];
   }
   mac.update(mac_buffer);
}

static void mac_output(const vector<small_string> &words, const CbcMac &mac, ostream &out)
{
   for (uint32 const x: mac.digest())
   {
      char temp[sizeof x];
      writeu32(temp, x);
      out << words[readu16(temp)] << ' ' << words[readu16(temp + sizeof(uint16_t))] << ' ';
   }
}

static void pad_and_crypt(char *const buffer, streamsize &bytes_read, CbcMac &mac)
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
   const streamsize data_size = bytes_read / sizeof(uint32);
   uint32 native_buffer[NATIVE_BUFFER_SIZE];
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

   // update mac with encrypted data
   mac_process_buffer(native_buffer, data_size, mac);

   // convert back to bytes
   for (streamsize i = 0; i < data_size; ++i) {
      writeu32(buffer + sizeof(uint32) * i, native_buffer[i]);
   }
}

void encode(const vector<small_string> &words, istream &in, ostream &out)
{
   CbcMac mac(static_key);
   char buffer[BUFFER_SIZE];

   // process the first block separately for the first CbcMac
   in.read(buffer, BUFFER_SIZE); // always read BUFFER_SIZE until EOF
   streamsize bytes_read = in.gcount();
   pad_and_crypt(buffer, bytes_read, mac); // buffer, bytes_read and mac are updated

   mac_output(words, mac, out);
   out << ",\n"; // the new line is cosmetic, the comma is meaningful in the format

   for ( ; ; )
   {
      const streamsize data_size = bytes_read / sizeof(uint16_t);
      for (streamsize i = 0; i < data_size; ++i) {
         out << words[readu16(buffer + sizeof(uint16_t) * i)]
             << (i % 8 != 7 ? ' ' : '\n'); // the new line is cosmetic
      }


      if (not in.good()) break; // break if fail() or eof()

      in.read(buffer, BUFFER_SIZE); // always read BUFFER_SIZE until EOF
      bytes_read = in.gcount();
      pad_and_crypt(buffer, bytes_read, mac); // buffer, bytes_read and mac are updated
   }

   // write the CbcMac as words
   out << ".\n"; // the new line is cosmetic, the point is meaningful in the format
   mac_output(words, mac, out);
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

void check_mac(CbcMac const& mac, const char*const kind, uint16_t (&expectedMac)[sizeof mac.digest() / sizeof(uint16_t)])
{
   bool ok = true;
   size_t e = 0;
   // check all words in order to avoid timing attacks
   for (uint32 const x: mac.digest())
   {
      char temp[sizeof x];
      writeu32(temp, x);
      ok &= readu16(temp) == expectedMac[e++];
      ok &= readu16(temp + sizeof(uint16_t)) == expectedMac[e++];
   }

   if (not ok)
   {
      string msg("invalid ");
      msg.append(kind).append(" MAC");
      throw error(__FILE__, __LINE__, msg);
   }
}

void decode(const vector<small_string> &words, istream &in, ostream &out)
{
   CbcMac mac(static_key);
   Buffers buffers;
   small_string word;
   uint16_t expectedMac[sizeof mac.digest() / sizeof(uint16_t)] = {};
   size_t macPos;
   for (macPos = 0; macPos < sizeof mac.digest() / sizeof(uint16_t); ++macPos)
   {
      if (in >> word)
      {
         const auto words_iter = std::lower_bound(words.begin(), words.end(), word, greater<small_string>());
         if (words_iter == words.end() or *words_iter != word)
         {
            string msg("unexpected word during initial MAC: `");
            msg += word + '\'';
            throw error(__FILE__, __LINE__, msg);
         }
         expectedMac[macPos] = words_iter - words.begin();
      }
      else
      {
         string msg("unexpected ");
         msg += in.eof() ? "EOF" : "word too long `";
         if (not in.eof()) msg += word + '`';
         msg += " during initial MAC";
         throw error(__FILE__, __LINE__, msg);
      }
   }
   // check how the initial MAC ends and if it is present
   if (in >> word and word == ",")
   { // nothing to do
   }
   else
   {
      throw error(__FILE__, __LINE__, "expected `,' to terminate the initial MAC");
   }

   while (in >> word)
   {
      const auto words_iter = std::lower_bound(words.begin(), words.end(), word, greater<small_string>());
      if (words_iter == words.end() or *words_iter != word)
      {
         if (word == ".")
         { // found the marker between the data and the MAC
            goto last_block;
         }
         else
         {
            string msg("unexpected word: `");
            msg += word + '\'';
            throw error(__FILE__, __LINE__, msg);
         }
      }
      char *pbuffer;
      if (0 != (pbuffer = bufferise_data(buffers, words_iter - words.begin())))
      { // the buffer is full

         // convert to native integers
         const streamsize data_size = BUFFER_SIZE / sizeof(uint32);
         uint32 native_buffer[data_size];
         for (streamsize i = 0; i < data_size; ++i) {
            native_buffer[i] = readu32(pbuffer + sizeof(uint32) * i);
         }

         // update mac with encrypted data
         mac_process_buffer(native_buffer, data_size, mac);

         if (macPos == sizeof expectedMac / sizeof *expectedMac)
         { // have a complete inital MAC to check
            check_mac(mac, "initial", expectedMac);
            macPos = 0; // don't check till the final block and final MAC
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

   if (not in.eof())
   {
      string msg("unexpected ");
      msg += "word too long `";
      msg += word + '`';
      throw error(__FILE__, __LINE__, msg);
   }

last_block:
   // special case for the last block, which may be partial
   const streamsize data_size = buffers.firstSize();
   char *const pbuffer = buffers.first();

   // when the previous block ends on a block boundary, there may be no data left
   if (data_size > 0)
   {
      // convert to native integers
      const streamsize contents_size = data_size / sizeof(uint32);
      uint32 native_buffer[NATIVE_BUFFER_SIZE];
      for (streamsize i = 0; i < contents_size; ++i) {
         native_buffer[i] = readu32(pbuffer + sizeof(uint32) * i);
      }

      // update mac with encrypted data
      mac_process_buffer(native_buffer, contents_size, mac);

      // check the final MAC before finishing
      // TODO: if (not in.good()) make this optional?
      for (macPos = 0; macPos < sizeof mac.digest() / sizeof(uint16_t); ++macPos)
      {
         if (in >> word)
         {
            const auto words_iter = std::lower_bound(words.begin(), words.end(), word, greater<small_string>());
            if (words_iter == words.end() or *words_iter != word)
            {
               string msg("unexpected word during final MAC: `");
               msg += word + '\'';
               throw error(__FILE__, __LINE__, msg);
            }
            expectedMac[macPos] = words_iter - words.begin();
         }
         else
         {
            string msg("unexpected ");
            msg += in.eof() ? "EOF" : "word too long `";
            if (not in.eof()) msg += word + '`';
            msg += " during final MAC";
            throw error(__FILE__, __LINE__, msg);
         }
      }
      check_mac(mac, "final", expectedMac);

      // decrypt
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

void generate_words(vector<small_string> &words)
{
   clog << "opening words.txt..." << endl;
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

   clog << "gathering the smallest words of the list..." << endl;
   nth_element(all_words.begin(), all_words.begin() + (1 << 16), all_words.end(),
      [](const string& a, const string& b)
      {
         const auto la(a.length()), lb(b.length());
         if (la < lb) return true;
         if (la > lb) return false;
         return a < b;
      });

   clog << "creating the word list..." << endl;
   all_words.erase(all_words.begin() + (1 << 16), all_words.end());
   // the memory isn't released but this is fine

   sort(all_words.begin(), all_words.end(), greater<string>()); // restore the reverse lexical order of the shortest words
   words.resize(all_words.size());
   for (size_t i = 0; i < all_words.size(); ++i)
   {
      memcpy(words[i].data(), all_words[i].data(), all_words[i].length());
   }

   if (words.size() != 1 << 16)
   {
      ostringstream msg;
      msg << "word list size is wrong: " << words.size() << " instead of " << (1 << 16);
      throw error(__FILE__, __LINE__, msg.str());
   }
}

bool quick_start(vector<small_string> &words)
{
   clog << "trying to quickstart... " << flush;
   ifstream sorted_words_file("words.quickstart");
   small_string line;
   while (sorted_words_file >> line)
   {
      if ( ! line.empty())
         words.push_back(line);
   }

   streamsize size = words.size();
   bool result = size == 1 << 16;
   if ( ! result) // failure
   {
      words.clear(); // clean before generate_words
      clog << "failed\n";
   }
   else
      clog << "success\n";
   return result;
}

void save_words(const vector<small_string> &words)
{
   ofstream sorted_words_file("words.quickstart");
   for (auto &word: words)
   {
      sorted_words_file << word << '\n';
   }
}
