#include "encodetotext.hpp"
#include "make_key.hpp"

#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <cstring>

using namespace std;

static bool ends_with(const string_view str, const string_view end)
{
   return end.size() <= str.size()
      && end == str.substr(str.size() - end.size());
}

static int process(int argc, char *argv[])
{
   string_view mode;
   ifstream file_in;
   ofstream file_out;
   istream *in;
   ostream *out;

   if (argc > 1)
   {
      mode = argv[1];
      if (mode != "enc" && mode != "dec" && mode != "key")
      {
         cerr << "invalid mode " << mode << " ; valid is enc, dec or key\n";
         return 2;
      }

      if (mode == "key")
      {
         if (argc > 3) cerr << "warning: only the first argument is part of the password\n";

         if (argc > 2) return make_key(argv[2]), 0;
         else return cerr << "missing arguments: mode {key}, password\n", 1;
         // not reached
      }

      if (argc <= 3)
      {
         cerr << "missing arguments: mode {enc, dec}, filename_in(or -), filename_out(or -)\n";
         return 1;
      }

      if (ends_with(argv[3], "words.txt"))
      { // basic protection of not overwriting our database
         cerr << "cannot use words.txt as filename\n";
         return 4;
      }
      if (strcmp(argv[2], "-") != 0)
      {
         file_in.open(argv[2], ios::binary);
         in = &file_in;
      }
      else
         in = &cin;

      if (!*in)
      {
         cerr << "error opening " << argv[2] << '\n';
         return 3;
      }

      if (strcmp(argv[3], "-") != 0)
      {
         file_out.open(argv[3], ios::binary);
         out = &file_out;
      }
      else
         out = &cout;

      if (!*out)
      {
         cerr << "error opening " << argv[3] << '\n';
         return 4;
      }
   }
   else
   {
      cerr << "missing arguments: mode {enc, dec, key}, filename_in(or -) or password, [filename_out(or -)]\n";
      return 1;
   }

   vector<small_string> words;
   if ( ! quick_start(words))
   {
      std::clock_t startTime(std::clock());

      generate_words(words);

      std::clock_t duration(std::clock() - startTime);
      std::cerr << "generate_words in " << (float(duration) / CLOCKS_PER_SEC) << "s." << endl;

      save_words(words);
   }
   load_static_key();

   if (mode == "enc")
   {
      cerr << "encoding the file..." << endl;
      encode(words, *in, *out);
   }
   else
   {
      unordered_map<small_string, uint16_t> words_rev;
      reverse_words(words, words_rev);

      cerr << "decoding the file..." << endl;
      decode(words_rev, *in, *out);
   }

   return 0;
}

int (*run)(int argc, char *argv[]) = process;
